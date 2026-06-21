/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo Anti-Relay Attack (Header)
 * =============================================================================
 *
 * Implementa 5 capas de proteccion contra ataques de relay:
 *
 * 1. RTT (Round-Trip Time): Mide latencia de comunicacion BLE.
 *    Un relay agrega minimo 5ms extra. Si RTT > umbral, rechazar.
 *
 * 2. RSSI Rate of Change: Una aproximacion natural cambia RSSI
 *    gradualmente (~2-5 dBm/s). Un relay aparece de golpe.
 *
 * 3. Motion Verification: El telefono envia datos del acelerometro.
 *    Si el telefono esta quieto pero el RSSI indica movimiento, alerta.
 *
 * 4. Geofencing: El telefono envia coordenadas GPS. Si la distancia
 *    GPS no coincide con la distancia RSSI, es relay.
 *
 * 5. Night Mode: Desactiva PKE automatico en horario nocturno.
 *    Solo permite desbloqueo manual desde la app.
 *
 * =============================================================================
 */

#ifndef ANTI_RELAY_H
#define ANTI_RELAY_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Configuracion Anti-Relay
// =============================================================================

// --- Capa 1: RTT ---
// Latencia maxima aceptable en un challenge-response BLE (ms)
// BLE normal: 5-15ms. Con relay: 20-80ms
#define RTT_MAX_ACCEPTABLE_MS       20

// Numero de muestras RTT para promediar
#define RTT_SAMPLE_COUNT            5

// Umbral de desviacion estandar (ms) - si varia mucho, sospechoso
#define RTT_DEVIATION_MAX_MS        8

// --- Capa 2: RSSI Rate of Change ---
// Cambio maximo de RSSI permitido en un intervalo (dBm)
// Caminando: ~2-5 dBm en 500ms
// Relay encendido: 20-40 dBm instantaneo
#define RSSI_MAX_CHANGE_PER_INTERVAL 12  // dBm en 500ms

// Numero de intervalos sospechosos consecutivos para disparar alerta
#define RSSI_SUSPICIOUS_COUNT        3

// --- Capa 3: Motion Verification ---
// El telefono debe reportar movimiento si el RSSI esta cambiando
// Umbral de aceleracion (m/s²) que indica que el telefono se mueve
#define MOTION_THRESHOLD_MS2         0.5f

// Tiempo maximo sin datos de movimiento antes de considerar sospechoso (ms)
#define MOTION_DATA_TIMEOUT_MS       5000

// --- Capa 4: Geofencing ---
// Diferencia maxima entre distancia GPS y distancia BLE (metros)
// GPS dice 5m, BLE dice 2m → diferencia de 3m → OK
// GPS dice 200m, BLE dice 2m → diferencia de 198m → RELAY!
#define GEO_MAX_DISTANCE_DIFF_M      20.0f

// Timeout de datos GPS (ms) - si no recibe GPS, desactivar esta capa
#define GEO_DATA_TIMEOUT_MS          30000

// --- Capa 5: Night Mode ---
// Hora de inicio del modo nocturno (24h) - PKE se desactiva
#define NIGHT_MODE_START_HOUR        23  // 11:00 PM

// Hora de fin del modo nocturno (24h) - PKE se reactiva
#define NIGHT_MODE_END_HOUR          6   // 6:00 AM

// =============================================================================
// Resultados de Analisis
// =============================================================================

enum RelayDetectionResult {
    RELAY_CLEAR = 0,        // No se detecta relay - todo normal
    RELAY_SUSPICIOUS,       // Comportamiento sospechoso (1 capa alerta)
    RELAY_LIKELY,           // Probable relay (2+ capas alertan)
    RELAY_CONFIRMED,        // Relay confirmado (3+ capas o RTT critico)
};

enum RelayAlertSource {
    ALERT_NONE = 0,
    ALERT_RTT_HIGH,         // Latencia excesiva
    ALERT_RTT_UNSTABLE,     // Latencia muy variable
    ALERT_RSSI_JUMP,        // Salto abrupto de RSSI
    ALERT_MOTION_MISMATCH,  // Telefono quieto pero señal en movimiento
    ALERT_GEO_MISMATCH,     // GPS no coincide con BLE
    ALERT_NIGHT_MODE,       // Intento en horario nocturno
};

// =============================================================================
// Estructuras de Datos
// =============================================================================

struct RttMeasurement {
    uint32_t challengeSentAt;    // Cuando se envio el challenge
    uint32_t responseReceivedAt; // Cuando llego la respuesta
    uint16_t rttMs;              // RTT calculado en ms
    bool valid;                  // Si la medicion es valida
};

struct MotionData {
    float accelX;               // Aceleracion X (m/s²)
    float accelY;               // Aceleracion Y (m/s²)
    float accelZ;               // Aceleracion Z (m/s²)
    float magnitude;            // Magnitud total
    bool isMoving;              // Si el telefono esta en movimiento
    uint32_t lastUpdateAt;      // Timestamp de ultimo dato
};

struct GeoData {
    double latitude;            // Latitud del telefono
    double longitude;           // Longitud del telefono
    float accuracy;             // Precision GPS (metros)
    float distanceToVehicle;    // Distancia calculada al vehiculo
    uint32_t lastUpdateAt;      // Timestamp de ultimo dato
};

struct AntiRelayStatus {
    RelayDetectionResult overallResult;
    uint8_t alertCount;          // Numero de capas alertando
    uint8_t alertSources;        // Bitmask de fuentes de alerta

    // Capa 1: RTT
    uint16_t avgRttMs;           // RTT promedio
    uint16_t rttDeviationMs;     // Desviacion estandar del RTT
    bool rttSuspicious;

    // Capa 2: RSSI
    int16_t lastRssiChange;      // Ultimo cambio de RSSI
    uint8_t rssiSuspiciousCount; // Intervalos sospechosos consecutivos
    bool rssiSuspicious;

    // Capa 3: Motion
    bool phoneMoving;            // El telefono reporta movimiento
    bool rssiChanging;           // El RSSI indica movimiento
    bool motionMismatch;         // Discrepancia motion vs RSSI

    // Capa 4: Geo
    float geoDistanceDiff;       // Diferencia GPS vs BLE
    bool geoSuspicious;

    // Capa 5: Night
    bool nightModeActive;        // Modo nocturno activo
    bool nightModeOverride;      // Override manual del usuario
};

// =============================================================================
// Clase AntiRelay
// =============================================================================

class AntiRelay {
public:
    AntiRelay();

    /**
     * Inicializa el modulo anti-relay
     * @return true si la inicializacion fue exitosa
     */
    bool init();

    /**
     * Evaluacion principal: analiza todas las capas y retorna
     * si se detecta un posible relay attack
     *
     * @return Resultado de la evaluacion (CLEAR, SUSPICIOUS, LIKELY, CONFIRMED)
     */
    RelayDetectionResult evaluate();

    // =========================================================================
    // Capa 1: RTT (Round-Trip Time)
    // =========================================================================

    /**
     * Registra el inicio de un challenge (para medir RTT)
     */
    void onChallengeSent();

    /**
     * Registra la recepcion de la respuesta al challenge
     * Calcula el RTT y lo agrega al historial
     */
    void onResponseReceived();

    /**
     * Obtiene el RTT promedio actual
     * @return RTT promedio en ms
     */
    uint16_t getAverageRtt();

    /**
     * Verifica si el RTT es sospechoso
     * @return true si el RTT supera el umbral
     */
    bool isRttSuspicious();

    // =========================================================================
    // Capa 2: RSSI Rate of Change
    // =========================================================================

    /**
     * Reporta un nuevo valor RSSI para analisis de tasa de cambio
     * @param rssi Nuevo valor RSSI
     */
    void onRssiUpdate(int16_t rssi);

    /**
     * Verifica si el patron de RSSI es sospechoso
     * @return true si se detectan saltos abruptos
     */
    bool isRssiPatternSuspicious();

    // =========================================================================
    // Capa 3: Motion Verification
    // =========================================================================

    /**
     * Actualiza los datos de movimiento del telefono
     * (recibidos via BLE desde la app Flutter)
     * @param data Datos del acelerometro
     */
    void updateMotionData(const MotionData& data);

    /**
     * Verifica si hay discrepancia entre movimiento y RSSI
     * @return true si el telefono esta quieto pero el RSSI cambia
     */
    bool isMotionMismatch();

    // =========================================================================
    // Capa 4: Geofencing
    // =========================================================================

    /**
     * Actualiza las coordenadas GPS del telefono
     * (recibidas via BLE desde la app Flutter)
     * @param data Datos de geolocalizacion
     */
    void updateGeoData(const GeoData& data);

    /**
     * Configura la ubicacion del vehiculo (fija)
     * @param lat Latitud del vehiculo
     * @param lon Longitud del vehiculo
     */
    void setVehicleLocation(double lat, double lon);

    /**
     * Verifica si la distancia GPS vs BLE es inconsistente
     * @return true si hay discrepancia significativa
     */
    bool isGeoMismatch();

    // =========================================================================
    // Capa 5: Night Mode
    // =========================================================================

    /**
     * Verifica si el modo nocturno esta activo
     * @param currentHour Hora actual (0-23)
     * @return true si estamos en horario nocturno
     */
    bool isNightModeActive(uint8_t currentHour);

    /**
     * Override manual del modo nocturno (usuario confirma en la app)
     */
    void overrideNightMode();

    /**
     * Resetea el override del modo nocturno
     */
    void resetNightModeOverride();

    // =========================================================================
    // Estado y Configuracion
    // =========================================================================

    /** @return Estado completo del analisis anti-relay */
    AntiRelayStatus getStatus();

    /** @return true si el sistema considera que es seguro desbloquear */
    bool isSafeToUnlock();

    /** @return Descripcion textual del resultado */
    const char* getResultName(RelayDetectionResult result);

    /** @return Descripcion de la fuente de alerta */
    const char* getAlertName(RelayAlertSource source);

    /** Resetea todos los contadores y alertas */
    void reset();

private:
    AntiRelayStatus _status;

    // Historial RTT
    RttMeasurement _rttSamples[RTT_SAMPLE_COUNT];
    uint8_t _rttIndex;
    bool _rttBufferFull;
    uint32_t _lastChallengeSentAt;

    // Historial RSSI para rate of change
    int16_t _lastRssi;
    int16_t _rssiDeltaHistory[5];
    uint8_t _rssiDeltaIndex;
    uint32_t _lastRssiUpdateAt;

    // Datos de movimiento
    MotionData _motionData;

    // Datos de geolocalizacion
    GeoData _geoData;
    double _vehicleLat;
    double _vehicleLon;
    bool _vehicleLocationSet;

    // Night mode
    bool _nightModeOverride;
    uint32_t _nightModeOverrideAt;

    // --- Metodos privados ---

    /** Calcula la desviacion estandar del RTT */
    uint16_t calculateRttDeviation();

    /** Calcula distancia Haversine entre 2 coordenadas */
    float haversineDistance(double lat1, double lon1,
                           double lat2, double lon2);

    /** Estima distancia basada en RSSI (aproximado) */
    float estimateDistanceFromRssi(int16_t rssi);

    /** Actualiza el resultado general basado en todas las capas */
    void updateOverallResult();
};

#endif // ANTI_RELAY_H
