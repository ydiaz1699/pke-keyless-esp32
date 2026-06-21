/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo de Zonas PKE (Implementacion)
 * =============================================================================
 */

#include "zone_manager.h"

// Tag para logging
static const char* TAG_ZONE = "ZONE";

// Umbral minimo de cambio RSSI para considerar movimiento (dBm)
static const int16_t MOVEMENT_THRESHOLD = 3;

// =============================================================================
// Constructor
// =============================================================================

ZoneManager::ZoneManager() {
    memset(&_status, 0, sizeof(ZoneStatus));
    _status.currentZone = ZONE_UNKNOWN;
    _status.pendingZone = ZONE_UNKNOWN;
    _status.movement = MOVEMENT_UNKNOWN;
    _status.lastEvent = EVENT_NONE;
    _pendingEvent = EVENT_NONE;
    _rssiHistoryIndex = 0;
    _rssiHistoryFull = false;
    memset(_rssiHistory, 0, sizeof(_rssiHistory));
}

// =============================================================================
// Inicializacion
// =============================================================================

bool ZoneManager::init() {
    PKE_LOG(TAG_ZONE, "Inicializando gestor de zonas PKE...");
    PKE_LOG(TAG_ZONE, "Umbrales configurados:");
    PKE_LOG(TAG_ZONE, "  INSIDE: > %d dBm", RSSI_ZONE_INSIDE);
    PKE_LOG(TAG_ZONE, "  NEAR:   %d a %d dBm", RSSI_ZONE_FAR, RSSI_ZONE_INSIDE);
    PKE_LOG(TAG_ZONE, "  FAR:    < %d dBm", RSSI_ZONE_FAR);
    PKE_LOG(TAG_ZONE, "  Histeresis: %d dBm", RSSI_HYSTERESIS);
    PKE_LOG(TAG_ZONE, "  Dwell time: %d ms", ZONE_DWELL_TIME_MS);
    PKE_LOG(TAG_ZONE, "  Lock delay: %d ms", LOCK_DELAY_MS);
    
    // Estado inicial: lejos (asumimos que nadie esta cerca al encender)
    _status.currentZone = ZONE_FAR;
    _status.zoneEnteredAt = millis();
    
    return true;
}

// =============================================================================
// Actualizacion Principal
// =============================================================================

void ZoneManager::update(int16_t rssi) {
    uint32_t now = millis();
    
    // Actualizar RSSI actual
    _status.currentRssi = rssi;
    
    // Guardar en historial para calcular tendencia
    _rssiHistory[_rssiHistoryIndex] = rssi;
    _rssiHistoryIndex = (_rssiHistoryIndex + 1) % TREND_SAMPLES;
    if (_rssiHistoryIndex == 0) _rssiHistoryFull = true;
    
    // Calcular tendencia y movimiento
    _status.rssiTrend = calculateTrend();
    _status.movement = classifyMovement(_status.rssiTrend);
    
    // Si la senal estaba perdida, marcar como recuperada
    if (_status.signalLost) {
        _status.signalLost = false;
        _pendingEvent = EVENT_PHONE_FOUND;
        PKE_LOG(TAG_ZONE, "Senal BLE recuperada (RSSI: %d)", rssi);
    }
    
    // Clasificar RSSI en zona (con histeresis)
    ProximityZone detectedZone = applyHysteresis(rssi, _status.currentZone);
    
    // Si la zona detectada es diferente a la actual
    if (detectedZone != _status.currentZone) {
        
        // Si es una nueva zona pendiente diferente a la anterior pendiente
        if (detectedZone != _status.pendingZone) {
            // Iniciar nuevo periodo de dwell time
            _status.pendingZone = detectedZone;
            _status.pendingZoneStartedAt = now;
            
            #if LOG_ZONE_CHANGES
            PKE_LOG(TAG_ZONE, "Zona pendiente: %s (RSSI: %d, esperando dwell time)",
                    getZoneName(detectedZone), rssi);
            #endif
        } else {
            // Misma zona pendiente - verificar si cumple dwell time
            if (isDwellTimeComplete()) {
                // Transicion confirmada!
                ProximityZone previousZone = _status.currentZone;
                confirmZoneTransition(detectedZone);
                
                // Generar evento de transicion
                ZoneEvent event = generateTransitionEvent(previousZone, detectedZone);
                if (event != EVENT_NONE) {
                    _pendingEvent = event;
                    _status.lastEvent = event;
                    _status.lastEventAt = now;
                }
            }
        }
    } else {
        // RSSI volvio a la zona actual - cancelar pendiente
        if (_status.pendingZone != ZONE_UNKNOWN && _status.pendingZone != _status.currentZone) {
            #if LOG_ZONE_CHANGES
            PKE_LOG(TAG_ZONE, "Zona pendiente cancelada (volvio a %s)", 
                    getZoneName(_status.currentZone));
            #endif
            _status.pendingZone = ZONE_UNKNOWN;
        }
    }
}

// =============================================================================
// Eventos de Conexion
// =============================================================================

void ZoneManager::onSignalLost() {
    PKE_LOG(TAG_ZONE, "*** SENAL BLE PERDIDA ***");
    
    _status.signalLost = true;
    _status.signalLostAt = millis();
    _status.currentRssi = -100;  // Valor minimo
    
    // Al perder senal, consideramos que el telefono se fue (zona FAR)
    // pero aplicamos un delay extra por si es una perdida momentanea
    _status.pendingZone = ZONE_FAR;
    _status.pendingZoneStartedAt = millis();
    
    _pendingEvent = EVENT_PHONE_LOST;
    _status.lastEvent = EVENT_PHONE_LOST;
    _status.lastEventAt = millis();
}

void ZoneManager::onSignalRestored(int16_t rssi) {
    _status.signalLost = false;
    _status.currentRssi = rssi;
    
    // Re-clasificar zona con el nuevo RSSI
    ProximityZone restoredZone = classifyRssi(rssi);
    
    // Si la zona es diferente a FAR, transicionar directamente
    // (ya paso suficiente tiempo durante la perdida de senal)
    if (restoredZone != _status.currentZone) {
        confirmZoneTransition(restoredZone);
        ZoneEvent event = generateTransitionEvent(ZONE_FAR, restoredZone);
        if (event != EVENT_NONE) {
            _pendingEvent = event;
            _status.lastEvent = event;
            _status.lastEventAt = millis();
        }
    }
    
    PKE_LOG(TAG_ZONE, "Senal restaurada - Zona: %s (RSSI: %d)", 
            getZoneName(restoredZone), rssi);
}

// =============================================================================
// Clasificacion de RSSI
// =============================================================================

ProximityZone ZoneManager::classifyRssi(int16_t rssi) {
    if (rssi >= RSSI_ZONE_INSIDE) {
        return ZONE_INSIDE;
    } else if (rssi >= RSSI_ZONE_NEAR) {
        return ZONE_NEAR;
    } else {
        return ZONE_FAR;
    }
}

ProximityZone ZoneManager::applyHysteresis(int16_t rssi, ProximityZone currentZone) {
    // La histeresis evita oscilaciones en los bordes de las zonas
    // Si estas en ZONE_NEAR y el RSSI baja a -71, no salta a FAR inmediatamente
    // Tiene que bajar a -75 (ZONE_FAR - HYSTERESIS) para confirmar el cambio
    
    switch (currentZone) {
        case ZONE_INSIDE:
            // Para salir de INSIDE, RSSI debe bajar mas alla del umbral con histeresis
            if (rssi < (RSSI_ZONE_INSIDE - RSSI_HYSTERESIS)) {
                if (rssi >= RSSI_ZONE_NEAR) return ZONE_NEAR;
                return ZONE_FAR;
            }
            return ZONE_INSIDE;
            
        case ZONE_NEAR:
            // Para subir a INSIDE
            if (rssi >= (RSSI_ZONE_INSIDE + RSSI_HYSTERESIS)) {
                return ZONE_INSIDE;
            }
            // Para bajar a FAR
            if (rssi < (RSSI_ZONE_NEAR - RSSI_HYSTERESIS)) {
                return ZONE_FAR;
            }
            return ZONE_NEAR;
            
        case ZONE_FAR:
            // Para subir a NEAR, RSSI debe superar el umbral con histeresis
            if (rssi >= (RSSI_ZONE_NEAR + RSSI_HYSTERESIS)) {
                // Verificar si va directo a INSIDE
                if (rssi >= (RSSI_ZONE_INSIDE + RSSI_HYSTERESIS)) {
                    return ZONE_INSIDE;
                }
                return ZONE_NEAR;
            }
            return ZONE_FAR;
            
        case ZONE_UNKNOWN:
        default:
            // Sin zona previa, usar clasificacion directa
            return classifyRssi(rssi);
    }
}

// =============================================================================
// Calculo de Tendencia y Movimiento
// =============================================================================

int16_t ZoneManager::calculateTrend() {
    if (!_rssiHistoryFull && _rssiHistoryIndex < 2) {
        return 0;  // No hay suficientes datos
    }
    
    // Calcular diferencia entre las ultimas muestras y las primeras
    // Tendencia positiva = RSSI subiendo = acercandose
    // Tendencia negativa = RSSI bajando = alejandose
    
    uint8_t count = _rssiHistoryFull ? TREND_SAMPLES : _rssiHistoryIndex;
    if (count < 2) return 0;
    
    // Promedio de la primera mitad vs segunda mitad
    int32_t firstHalf = 0;
    int32_t secondHalf = 0;
    uint8_t halfCount = count / 2;
    
    for (uint8_t i = 0; i < halfCount; i++) {
        uint8_t idx = (_rssiHistoryIndex + TREND_SAMPLES - count + i) % TREND_SAMPLES;
        firstHalf += _rssiHistory[idx];
    }
    
    for (uint8_t i = halfCount; i < count; i++) {
        uint8_t idx = (_rssiHistoryIndex + TREND_SAMPLES - count + i) % TREND_SAMPLES;
        secondHalf += _rssiHistory[idx];
    }
    
    firstHalf /= halfCount;
    secondHalf /= (count - halfCount);
    
    return (int16_t)(secondHalf - firstHalf);
}

MovementDirection ZoneManager::classifyMovement(int16_t trend) {
    if (trend > MOVEMENT_THRESHOLD) {
        return MOVEMENT_APPROACHING;
    } else if (trend < -MOVEMENT_THRESHOLD) {
        return MOVEMENT_DEPARTING;
    } else {
        return MOVEMENT_STATIONARY;
    }
}

// =============================================================================
// Dwell Time y Transiciones
// =============================================================================

bool ZoneManager::isDwellTimeComplete() {
    if (_status.pendingZone == ZONE_UNKNOWN) return false;
    
    uint32_t elapsed = millis() - _status.pendingZoneStartedAt;
    
    // Para la zona FAR (bloqueo), usar un delay mas largo
    // para dar oportunidad al usuario de volver
    if (_status.pendingZone == ZONE_FAR) {
        return (elapsed >= LOCK_DELAY_MS);
    }
    
    // Para otras zonas, usar el dwell time estandar
    return (elapsed >= ZONE_DWELL_TIME_MS);
}

void ZoneManager::confirmZoneTransition(ProximityZone newZone) {
    ProximityZone oldZone = _status.currentZone;
    
    _status.currentZone = newZone;
    _status.zoneEnteredAt = millis();
    _status.pendingZone = ZONE_UNKNOWN;
    
    PKE_LOG(TAG_ZONE, "=== TRANSICION DE ZONA: %s -> %s ===", 
            getZoneName(oldZone), getZoneName(newZone));
}

ZoneEvent ZoneManager::generateTransitionEvent(ProximityZone fromZone, ProximityZone toZone) {
    // Transiciones hacia adentro (acercandose)
    if (fromZone == ZONE_FAR && toZone == ZONE_NEAR) {
        PKE_LOG(TAG_ZONE, ">>> EVENTO: Entro en zona CERCA (desbloquear)");
        return EVENT_ENTERED_NEAR;
    }
    
    if ((fromZone == ZONE_FAR || fromZone == ZONE_NEAR) && toZone == ZONE_INSIDE) {
        PKE_LOG(TAG_ZONE, ">>> EVENTO: Entro en zona DENTRO (habilitar START)");
        return EVENT_ENTERED_INSIDE;
    }
    
    if (fromZone == ZONE_NEAR && toZone == ZONE_INSIDE) {
        PKE_LOG(TAG_ZONE, ">>> EVENTO: Entro en zona DENTRO (habilitar START)");
        return EVENT_ENTERED_INSIDE;
    }
    
    // Transiciones hacia afuera (alejandose)
    if (fromZone == ZONE_INSIDE && toZone == ZONE_NEAR) {
        PKE_LOG(TAG_ZONE, ">>> EVENTO: Salio de zona DENTRO (deshabilitar START)");
        return EVENT_EXITED_INSIDE;
    }
    
    if ((fromZone == ZONE_INSIDE || fromZone == ZONE_NEAR) && toZone == ZONE_FAR) {
        PKE_LOG(TAG_ZONE, ">>> EVENTO: Salio de zona CERCA (bloquear)");
        return EVENT_EXITED_NEAR;
    }
    
    if (fromZone == ZONE_NEAR && toZone == ZONE_FAR) {
        PKE_LOG(TAG_ZONE, ">>> EVENTO: Salio de zona CERCA (bloquear)");
        return EVENT_EXITED_NEAR;
    }
    
    return EVENT_NONE;
}

// =============================================================================
// Getters
// =============================================================================

ProximityZone ZoneManager::getCurrentZone() {
    return _status.currentZone;
}

MovementDirection ZoneManager::getMovement() {
    return _status.movement;
}

ZoneStatus ZoneManager::getStatus() {
    return _status;
}

bool ZoneManager::hasEvent() {
    return (_pendingEvent != EVENT_NONE);
}

ZoneEvent ZoneManager::getEvent() {
    ZoneEvent event = _pendingEvent;
    _pendingEvent = EVENT_NONE;  // Consumir el evento
    return event;
}

uint32_t ZoneManager::getTimeInCurrentZone() {
    return millis() - _status.zoneEnteredAt;
}

bool ZoneManager::isSignalLost() {
    return _status.signalLost;
}

// =============================================================================
// Nombres legibles (para debug)
// =============================================================================

const char* ZoneManager::getZoneName(ProximityZone zone) {
    switch (zone) {
        case ZONE_INSIDE:   return "DENTRO";
        case ZONE_NEAR:     return "CERCA";
        case ZONE_FAR:      return "LEJOS";
        case ZONE_UNKNOWN:  return "DESCONOCIDA";
        default:            return "???";
    }
}

const char* ZoneManager::getEventName(ZoneEvent event) {
    switch (event) {
        case EVENT_NONE:            return "NINGUNO";
        case EVENT_ENTERED_NEAR:    return "ENTRO_CERCA";
        case EVENT_ENTERED_INSIDE:  return "ENTRO_DENTRO";
        case EVENT_EXITED_INSIDE:   return "SALIO_DENTRO";
        case EVENT_EXITED_NEAR:     return "SALIO_CERCA";
        case EVENT_PHONE_LOST:      return "SENAL_PERDIDA";
        case EVENT_PHONE_FOUND:     return "SENAL_RECUPERADA";
        default:                    return "???";
    }
}
