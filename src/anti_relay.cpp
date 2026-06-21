/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo Anti-Relay Attack (Implementacion)
 * =============================================================================
 */

#include "anti_relay.h"
#include <math.h>

static const char* TAG_AR = "ANTI-RELAY";

// Radio de la Tierra en metros (para Haversine)
static const double EARTH_RADIUS_M = 6371000.0;


// =============================================================================
// Constructor
// =============================================================================

AntiRelay::AntiRelay() {
    memset(&_status, 0, sizeof(AntiRelayStatus));
    _status.overallResult = RELAY_CLEAR;
    _rttIndex = 0;
    _rttBufferFull = false;
    _lastChallengeSentAt = 0;
    _lastRssi = -100;
    _rssiDeltaIndex = 0;
    _lastRssiUpdateAt = 0;
    memset(&_motionData, 0, sizeof(MotionData));
    memset(&_geoData, 0, sizeof(GeoData));
    _vehicleLat = 0;
    _vehicleLon = 0;
    _vehicleLocationSet = false;
    _nightModeOverride = false;
    _nightModeOverrideAt = 0;
    memset(_rttSamples, 0, sizeof(_rttSamples));
    memset(_rssiDeltaHistory, 0, sizeof(_rssiDeltaHistory));
}


// =============================================================================
// Inicializacion
// =============================================================================

bool AntiRelay::init() {
    PKE_LOG(TAG_AR, "Inicializando proteccion anti-relay...");
    PKE_LOG(TAG_AR, "  RTT max: %d ms", RTT_MAX_ACCEPTABLE_MS);
    PKE_LOG(TAG_AR, "  RSSI max change: %d dBm/intervalo", RSSI_MAX_CHANGE_PER_INTERVAL);
    PKE_LOG(TAG_AR, "  Motion threshold: %.1f m/s2", MOTION_THRESHOLD_MS2);
    PKE_LOG(TAG_AR, "  Geo max diff: %.1f m", GEO_MAX_DISTANCE_DIFF_M);
    PKE_LOG(TAG_AR, "  Night mode: %d:00 - %d:00", NIGHT_MODE_START_HOUR, NIGHT_MODE_END_HOUR);
    PKE_LOG(TAG_AR, "Anti-relay inicializado (5 capas activas)");
    return true;
}

// =============================================================================
// Evaluacion Principal
// =============================================================================

RelayDetectionResult AntiRelay::evaluate() {
    _status.alertCount = 0;
    _status.alertSources = 0;

    // Capa 1: RTT
    _status.rttSuspicious = isRttSuspicious();
    if (_status.rttSuspicious) {
        _status.alertCount++;
        _status.alertSources |= (1 << ALERT_RTT_HIGH);
        PKE_LOG(TAG_AR, "!!! ALERTA RTT: Latencia alta (%d ms)", _status.avgRttMs);
    }

    // Capa 2: RSSI Pattern
    _status.rssiSuspicious = isRssiPatternSuspicious();
    if (_status.rssiSuspicious) {
        _status.alertCount++;
        _status.alertSources |= (1 << ALERT_RSSI_JUMP);
        PKE_LOG(TAG_AR, "!!! ALERTA RSSI: Salto abrupto detectado");
    }

    // Capa 3: Motion
    _status.motionMismatch = isMotionMismatch();
    if (_status.motionMismatch) {
        _status.alertCount++;
        _status.alertSources |= (1 << ALERT_MOTION_MISMATCH);
        PKE_LOG(TAG_AR, "!!! ALERTA MOTION: Tel quieto pero RSSI cambia");
    }

    // Capa 4: Geo
    _status.geoSuspicious = isGeoMismatch();
    if (_status.geoSuspicious) {
        _status.alertCount++;
        _status.alertSources |= (1 << ALERT_GEO_MISMATCH);
        PKE_LOG(TAG_AR, "!!! ALERTA GEO: GPS vs BLE inconsistente");
    }

    // Determinar resultado general
    updateOverallResult();

    return _status.overallResult;
}


// =============================================================================
// Capa 1: RTT (Round-Trip Time)
// =============================================================================

void AntiRelay::onChallengeSent() {
    _lastChallengeSentAt = micros();  // Usar microsegundos para precision
}

void AntiRelay::onResponseReceived() {
    if (_lastChallengeSentAt == 0) return;

    uint32_t now = micros();
    uint32_t rttMicros = now - _lastChallengeSentAt;
    uint16_t rttMs = rttMicros / 1000;

    // Guardar muestra
    _rttSamples[_rttIndex].challengeSentAt = _lastChallengeSentAt;
    _rttSamples[_rttIndex].responseReceivedAt = now;
    _rttSamples[_rttIndex].rttMs = rttMs;
    _rttSamples[_rttIndex].valid = true;

    _rttIndex = (_rttIndex + 1) % RTT_SAMPLE_COUNT;
    if (_rttIndex == 0) _rttBufferFull = true;

    _lastChallengeSentAt = 0;

    // Calcular promedio
    _status.avgRttMs = getAverageRtt();
    _status.rttDeviationMs = calculateRttDeviation();

    #ifdef DEBUG_MODE
    PKE_LOG(TAG_AR, "[RTT] %d ms (avg: %d, dev: %d)",
            rttMs, _status.avgRttMs, _status.rttDeviationMs);
    #endif
}

uint16_t AntiRelay::getAverageRtt() {
    uint8_t count = _rttBufferFull ? RTT_SAMPLE_COUNT : _rttIndex;
    if (count == 0) return 0;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (_rttSamples[i].valid) {
            sum += _rttSamples[i].rttMs;
        }
    }
    return (uint16_t)(sum / count);
}

bool AntiRelay::isRttSuspicious() {
    uint16_t avgRtt = getAverageRtt();
    if (avgRtt == 0) return false;  // Sin datos aun

    // RTT mayor al umbral = sospechoso
    if (avgRtt > RTT_MAX_ACCEPTABLE_MS) {
        return true;
    }

    // Desviacion alta = inestable (relay inconsistente)
    if (_status.rttDeviationMs > RTT_DEVIATION_MAX_MS) {
        return true;
    }

    return false;
}

uint16_t AntiRelay::calculateRttDeviation() {
    uint8_t count = _rttBufferFull ? RTT_SAMPLE_COUNT : _rttIndex;
    if (count < 2) return 0;

    uint16_t avg = getAverageRtt();
    uint32_t sumSquares = 0;

    for (uint8_t i = 0; i < count; i++) {
        if (_rttSamples[i].valid) {
            int16_t diff = _rttSamples[i].rttMs - avg;
            sumSquares += (diff * diff);
        }
    }

    return (uint16_t)sqrt((float)sumSquares / count);
}


// =============================================================================
// Capa 2: RSSI Rate of Change
// =============================================================================

void AntiRelay::onRssiUpdate(int16_t rssi) {
    uint32_t now = millis();

    if (_lastRssiUpdateAt > 0) {
        // Calcular cambio de RSSI
        int16_t delta = abs(rssi - _lastRssi);
        _status.lastRssiChange = delta;

        // Guardar en historial
        _rssiDeltaHistory[_rssiDeltaIndex] = delta;
        _rssiDeltaIndex = (_rssiDeltaIndex + 1) % 5;

        // Verificar si el cambio es sospechoso
        if (delta > RSSI_MAX_CHANGE_PER_INTERVAL) {
            _status.rssiSuspiciousCount++;
            PKE_LOG(TAG_AR, "[RSSI] Salto sospechoso: %d dBm (max: %d)",
                    delta, RSSI_MAX_CHANGE_PER_INTERVAL);
        } else {
            // Decrementar gradualmente (perdonar saltos aislados)
            if (_status.rssiSuspiciousCount > 0) {
                _status.rssiSuspiciousCount--;
            }
        }
    }

    _lastRssi = rssi;
    _lastRssiUpdateAt = now;

    // Determinar si RSSI esta cambiando significativamente
    // (usado por la Capa 3 para comparar con movimiento)
    int16_t avgDelta = 0;
    for (uint8_t i = 0; i < 5; i++) {
        avgDelta += _rssiDeltaHistory[i];
    }
    avgDelta /= 5;
    _status.rssiChanging = (avgDelta > 3);  // >3 dBm promedio = en movimiento
}

bool AntiRelay::isRssiPatternSuspicious() {
    return (_status.rssiSuspiciousCount >= RSSI_SUSPICIOUS_COUNT);
}


// =============================================================================
// Capa 3: Motion Verification
// =============================================================================

void AntiRelay::updateMotionData(const MotionData& data) {
    _motionData = data;
    _motionData.lastUpdateAt = millis();

    // Calcular magnitud si no viene calculada
    if (_motionData.magnitude == 0) {
        _motionData.magnitude = sqrt(
            data.accelX * data.accelX +
            data.accelY * data.accelY +
            data.accelZ * data.accelZ
        );
    }

    // Determinar si el telefono se esta moviendo
    // Restar gravedad (~9.8 m/s²) y ver si queda algo significativo
    float netAccel = fabs(_motionData.magnitude - 9.81f);
    _motionData.isMoving = (netAccel > MOTION_THRESHOLD_MS2);
    _status.phoneMoving = _motionData.isMoving;
}

bool AntiRelay::isMotionMismatch() {
    // Si no tenemos datos de movimiento recientes, no podemos evaluar
    if (millis() - _motionData.lastUpdateAt > MOTION_DATA_TIMEOUT_MS) {
        return false;  // Sin datos = no alertar (falso positivo indeseable)
    }

    // DISCREPANCIA: RSSI indica movimiento PERO telefono esta quieto
    // Esto sugiere que la senal esta siendo retransmitida por un relay
    // mientras el telefono real esta inmovil (ej: en la mesa de noche)
    if (_status.rssiChanging && !_status.phoneMoving) {
        PKE_LOG(TAG_AR, "[MOTION] RSSI cambiando (avg delta >3) pero tel quieto");
        return true;
    }

    return false;
}


// =============================================================================
// Capa 4: Geofencing
// =============================================================================

void AntiRelay::updateGeoData(const GeoData& data) {
    _geoData = data;
    _geoData.lastUpdateAt = millis();

    // Calcular distancia GPS al vehiculo
    if (_vehicleLocationSet) {
        _geoData.distanceToVehicle = haversineDistance(
            data.latitude, data.longitude,
            _vehicleLat, _vehicleLon
        );
        _status.geoDistanceDiff = fabs(
            _geoData.distanceToVehicle - estimateDistanceFromRssi(_lastRssi)
        );
    }
}

void AntiRelay::setVehicleLocation(double lat, double lon) {
    _vehicleLat = lat;
    _vehicleLon = lon;
    _vehicleLocationSet = true;
    PKE_LOG(TAG_AR, "[GEO] Ubicacion del vehiculo: %.6f, %.6f", lat, lon);
}

bool AntiRelay::isGeoMismatch() {
    // Sin datos GPS recientes, no evaluar
    if (millis() - _geoData.lastUpdateAt > GEO_DATA_TIMEOUT_MS) {
        return false;
    }

    // Sin ubicacion del vehiculo configurada
    if (!_vehicleLocationSet) return false;

    // Comparar distancia GPS con distancia estimada por BLE
    float gpsDistance = _geoData.distanceToVehicle;
    float bleDistance = estimateDistanceFromRssi(_lastRssi);

    // Si GPS dice lejos pero BLE dice cerca → RELAY
    if (gpsDistance > GEO_MAX_DISTANCE_DIFF_M && bleDistance < 5.0f) {
        PKE_LOG(TAG_AR, "[GEO] GPS=%.1fm, BLE=%.1fm → DISCREPANCIA",
                gpsDistance, bleDistance);
        return true;
    }

    return false;
}

float AntiRelay::haversineDistance(double lat1, double lon1,
                                    double lat2, double lon2) {
    // Convertir a radianes
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = lat1 * M_PI / 180.0;
    lat2 = lat2 * M_PI / 180.0;

    // Formula de Haversine
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1) * cos(lat2) *
               sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return (float)(EARTH_RADIUS_M * c);
}

float AntiRelay::estimateDistanceFromRssi(int16_t rssi) {
    // Modelo Log-Distance Path Loss simplificado
    // d = 10 ^ ((TxPower - RSSI) / (10 * n))
    // TxPower a 1 metro tipico BLE: -59 dBm
    // n (path loss exponent): 2.0 en espacio libre, 3.0 en interiores
    const float txPower = -59.0f;
    const float n = 2.5f;  // Intermedio

    if (rssi >= 0) return 0.1f;  // Muy cerca

    float exponent = (txPower - (float)rssi) / (10.0f * n);
    return pow(10.0f, exponent);
}


// =============================================================================
// Capa 5: Night Mode
// =============================================================================

bool AntiRelay::isNightModeActive(uint8_t currentHour) {
    // Verificar si hay override manual activo
    if (_nightModeOverride) {
        // Override expira despues de 5 minutos
        if (millis() - _nightModeOverrideAt > 300000) {
            _nightModeOverride = false;
        } else {
            return false;  // Override activo - permitir
        }
    }

    // Verificar horario
    if (NIGHT_MODE_START_HOUR > NIGHT_MODE_END_HOUR) {
        // Cruce de medianoche (ej: 23:00 - 06:00)
        _status.nightModeActive = (currentHour >= NIGHT_MODE_START_HOUR ||
                                    currentHour < NIGHT_MODE_END_HOUR);
    } else {
        _status.nightModeActive = (currentHour >= NIGHT_MODE_START_HOUR &&
                                    currentHour < NIGHT_MODE_END_HOUR);
    }

    return _status.nightModeActive;
}

void AntiRelay::overrideNightMode() {
    _nightModeOverride = true;
    _nightModeOverrideAt = millis();
    _status.nightModeOverride = true;
    PKE_LOG(TAG_AR, "[NIGHT] Override activado (5 min)");
}

void AntiRelay::resetNightModeOverride() {
    _nightModeOverride = false;
    _status.nightModeOverride = false;
}


// =============================================================================
// Resultado General y Utilidades
// =============================================================================

void AntiRelay::updateOverallResult() {
    // RTT critico = confirmado inmediatamente (la prueba mas confiable)
    if (_status.avgRttMs > RTT_MAX_ACCEPTABLE_MS * 3) {
        _status.overallResult = RELAY_CONFIRMED;
        return;
    }

    // Basado en numero de capas alertando
    switch (_status.alertCount) {
        case 0:
            _status.overallResult = RELAY_CLEAR;
            break;
        case 1:
            _status.overallResult = RELAY_SUSPICIOUS;
            break;
        case 2:
            _status.overallResult = RELAY_LIKELY;
            break;
        default:  // 3+
            _status.overallResult = RELAY_CONFIRMED;
            break;
    }
}

bool AntiRelay::isSafeToUnlock() {
    RelayDetectionResult result = evaluate();

    switch (result) {
        case RELAY_CLEAR:
            return true;  // Todo bien

        case RELAY_SUSPICIOUS:
            // 1 alerta: permitir pero loguear
            PKE_LOG(TAG_AR, "SOSPECHOSO (1 capa) - permitiendo con advertencia");
            return true;

        case RELAY_LIKELY:
            // 2 alertas: bloquear desbloqueo automatico
            // El usuario puede desbloquear manualmente desde la app
            PKE_LOG(TAG_AR, "*** PROBABLE RELAY - PKE AUTOMATICO BLOQUEADO ***");
            return false;

        case RELAY_CONFIRMED:
            // 3+ alertas: bloquear completamente
            PKE_LOG(TAG_AR, "!!! RELAY CONFIRMADO - DESBLOQUEO BLOQUEADO !!!");
            return false;

        default:
            return false;
    }
}

AntiRelayStatus AntiRelay::getStatus() {
    return _status;
}

void AntiRelay::reset() {
    memset(&_status, 0, sizeof(AntiRelayStatus));
    _status.overallResult = RELAY_CLEAR;
    _rttIndex = 0;
    _rttBufferFull = false;
    _status.rssiSuspiciousCount = 0;
    _nightModeOverride = false;
    PKE_LOG(TAG_AR, "Anti-relay reseteado");
}

const char* AntiRelay::getResultName(RelayDetectionResult result) {
    switch (result) {
        case RELAY_CLEAR:       return "LIMPIO";
        case RELAY_SUSPICIOUS:  return "SOSPECHOSO";
        case RELAY_LIKELY:      return "PROBABLE";
        case RELAY_CONFIRMED:   return "CONFIRMADO";
        default:                return "???";
    }
}

const char* AntiRelay::getAlertName(RelayAlertSource source) {
    switch (source) {
        case ALERT_NONE:            return "NINGUNA";
        case ALERT_RTT_HIGH:        return "RTT_ALTO";
        case ALERT_RTT_UNSTABLE:    return "RTT_INESTABLE";
        case ALERT_RSSI_JUMP:       return "RSSI_SALTO";
        case ALERT_MOTION_MISMATCH: return "MOTION_DISCREPANCIA";
        case ALERT_GEO_MISMATCH:    return "GEO_DISCREPANCIA";
        case ALERT_NIGHT_MODE:      return "MODO_NOCTURNO";
        default:                    return "???";
    }
}
