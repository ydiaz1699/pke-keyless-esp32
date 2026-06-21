/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo de Zonas PKE (Header)
 * =============================================================================
 * 
 * Gestiona las 3 zonas de proximidad del sistema PKE basandose en RSSI:
 * 
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │                                                                     │
 * │   ZONA 3: LEJOS (RSSI < -80 dBm)                                  │
 * │   -> Auto bloqueado, alarma armada                                  │
 * │                                                                     │
 * │      ┌─────────────────────────────────────────────────────┐       │
 * │      │                                                     │       │
 * │      │   ZONA 2: CERCA (RSSI entre -70 y -50 dBm)        │       │
 * │      │   -> Desbloqueo automatico, espejos desplegados     │       │
 * │      │                                                     │       │
 * │      │      ┌─────────────────────────────────────┐       │       │
 * │      │      │                                     │       │       │
 * │      │      │   ZONA 1: DENTRO (RSSI > -50 dBm)  │       │       │
 * │      │      │   -> Boton START habilitado         │       │       │
 * │      │      │                                     │       │       │
 * │      │      └─────────────────────────────────────┘       │       │
 * │      │                                                     │       │
 * │      └─────────────────────────────────────────────────────┘       │
 * │                                                                     │
 * └─────────────────────────────────────────────────────────────────────┘
 * 
 * Caracteristicas:
 * - Histeresis para evitar oscilaciones entre zonas
 * - Tiempo de permanencia minimo (dwell time) antes de activar acciones
 * - Delay configurable al alejarse (para no bloquear prematuramente)
 * - Deteccion de velocidad de cambio de RSSI (acercandose vs alejandose)
 * 
 * =============================================================================
 */

#ifndef ZONE_MANAGER_H
#define ZONE_MANAGER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Enumeraciones
// =============================================================================

/**
 * Zonas de proximidad del sistema PKE
 */
enum ProximityZone {
    ZONE_UNKNOWN = 0,   // Sin datos suficientes
    ZONE_FAR,           // Zona 3: Lejos (bloquear)
    ZONE_NEAR,          // Zona 2: Cerca (desbloquear)
    ZONE_INSIDE         // Zona 1: Dentro (habilitar START)
};

/**
 * Direccion de movimiento del usuario
 */
enum MovementDirection {
    MOVEMENT_UNKNOWN = 0,
    MOVEMENT_APPROACHING,   // Acercandose al vehiculo
    MOVEMENT_DEPARTING,     // Alejandose del vehiculo
    MOVEMENT_STATIONARY     // Quieto (sin cambio significativo)
};

/**
 * Eventos generados por cambios de zona
 */
enum ZoneEvent {
    EVENT_NONE = 0,
    EVENT_ENTERED_NEAR,         // Paso de FAR -> NEAR (desbloquear)
    EVENT_ENTERED_INSIDE,       // Paso de NEAR -> INSIDE (habilitar START)
    EVENT_EXITED_INSIDE,        // Paso de INSIDE -> NEAR (deshabilitar START)
    EVENT_EXITED_NEAR,          // Paso de NEAR -> FAR (bloquear)
    EVENT_PHONE_LOST,           // Senal perdida completamente
    EVENT_PHONE_FOUND           // Senal recuperada tras perdida
};

// =============================================================================
// Estructura de Estado de Zona
// =============================================================================

struct ZoneStatus {
    ProximityZone currentZone;      // Zona actual confirmada
    ProximityZone pendingZone;      // Zona pendiente (en periodo dwell)
    MovementDirection movement;     // Direccion de movimiento
    int16_t currentRssi;           // RSSI actual (filtrado)
    int16_t rssiTrend;             // Tendencia del RSSI (positivo=acercandose)
    uint32_t zoneEnteredAt;        // Timestamp de entrada en zona actual
    uint32_t pendingZoneStartedAt; // Timestamp de inicio de zona pendiente
    uint32_t lastEventAt;          // Timestamp del ultimo evento
    ZoneEvent lastEvent;           // Ultimo evento generado
    bool signalLost;               // Si se perdio la senal BLE
    uint32_t signalLostAt;         // Cuando se perdio la senal
};

// =============================================================================
// Clase ZoneManager
// =============================================================================

class ZoneManager {
public:
    ZoneManager();

    /**
     * Inicializa el gestor de zonas
     * @return true si la inicializacion fue exitosa
     */
    bool init();

    /**
     * Actualiza el estado de las zonas basandose en un nuevo valor RSSI
     * Llamar cada vez que se obtiene una nueva lectura RSSI del BLE
     * @param rssi Valor RSSI filtrado actual
     */
    void update(int16_t rssi);

    /**
     * Notifica que la conexion BLE se perdio
     * Inicia el proceso de bloqueo por senal perdida
     */
    void onSignalLost();

    /**
     * Notifica que la conexion BLE se recupero
     * @param rssi Primer RSSI tras reconexion
     */
    void onSignalRestored(int16_t rssi);

    // --- Getters de Estado ---

    /** @return Zona de proximidad actual confirmada */
    ProximityZone getCurrentZone();

    /** @return Direccion de movimiento detectada */
    MovementDirection getMovement();

    /** @return Estructura completa de estado */
    ZoneStatus getStatus();

    /** @return true si hay un evento pendiente de procesar */
    bool hasEvent();

    /**
     * Obtiene y consume el evento pendiente
     * @return El evento generado (EVENT_NONE si no hay)
     */
    ZoneEvent getEvent();

    /** @return String legible del nombre de la zona */
    const char* getZoneName(ProximityZone zone);

    /** @return String legible del nombre del evento */
    const char* getEventName(ZoneEvent event);

    /** @return Tiempo en milisegundos que lleva en la zona actual */
    uint32_t getTimeInCurrentZone();

    /** @return true si la senal BLE esta perdida */
    bool isSignalLost();

private:
    ZoneStatus _status;
    ZoneEvent _pendingEvent;

    // Historial de RSSI para calcular tendencia
    static const uint8_t TREND_SAMPLES = 5;
    int16_t _rssiHistory[5];
    uint8_t _rssiHistoryIndex;
    bool _rssiHistoryFull;

    // --- Metodos privados ---

    /**
     * Determina en que zona cae un valor RSSI dado
     * Aplica histeresis segun la zona actual
     * @param rssi Valor RSSI a evaluar
     * @return Zona correspondiente al RSSI
     */
    ProximityZone classifyRssi(int16_t rssi);

    /**
     * Aplica histeresis para evitar oscilaciones entre zonas
     * @param rssi Valor RSSI
     * @param currentZone Zona actual (para aplicar histeresis correcta)
     * @return Zona con histeresis aplicada
     */
    ProximityZone applyHysteresis(int16_t rssi, ProximityZone currentZone);

    /**
     * Calcula la tendencia del RSSI (derivada)
     * @return Valor positivo = acercandose, negativo = alejandose
     */
    int16_t calculateTrend();

    /**
     * Determina la direccion de movimiento basada en la tendencia
     * @param trend Valor de tendencia
     * @return Direccion de movimiento
     */
    MovementDirection classifyMovement(int16_t trend);

    /**
     * Verifica si una zona pendiente ha cumplido el dwell time
     * @return true si la zona pendiente es estable
     */
    bool isDwellTimeComplete();

    /**
     * Genera un evento de transicion entre zonas
     * @param fromZone Zona anterior
     * @param toZone Zona nueva
     * @return Evento correspondiente a la transicion
     */
    ZoneEvent generateTransitionEvent(ProximityZone fromZone, ProximityZone toZone);

    /**
     * Confirma la transicion a una nueva zona
     * @param newZone La zona a la que se transiciona
     */
    void confirmZoneTransition(ProximityZone newZone);
};

#endif // ZONE_MANAGER_H
