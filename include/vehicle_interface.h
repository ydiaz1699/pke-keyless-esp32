/**
 * =============================================================================
 * PKE + Keyless Start System - Interfaz de Vehiculo (Header)
 * =============================================================================
 * 
 * Capa de abstraccion que encapsula los 3 modos de comunicacion con el vehiculo:
 * 
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                     VEHICLE INTERFACE (Abstraccion)                      │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                         │
 * │   lockDoors()  unlockDoors()  startEngine()  stopEngine()  etc.        │
 * │                                                                         │
 * ├───────────────┬───────────────────────────┬─────────────────────────────┤
 * │               │                           │                             │
 * │  MODO 1:      │  MODO 2:                  │  MODO 3:                   │
 * │  CENTRALIZADO │  CABLE DIRECTO            │  CAN BUS                   │
 * │               │                           │                             │
 * │  Pulso a la   │  Activacion directa       │  Mensajes CAN              │
 * │  central de   │  de actuadores de         │  al BCM/ECU del            │
 * │  cierre       │  motor en puertas         │  vehiculo                  │
 * │  existente    │                           │                             │
 * │               │                           │                             │
 * │  [Rele pulse] │  [Rele sostenido]         │  [CAN TX frame]            │
 * │               │                           │                             │
 * └───────────────┴───────────────────────────┴─────────────────────────────┘
 * 
 * Modo 1 - Cierre Centralizado:
 *   Para vehiculos que YA tienen central de cierre. Se conecta en paralelo
 *   al boton del control original. Un pulso corto activa el mecanismo existente.
 *   Es la instalacion mas sencilla y menos invasiva.
 * 
 * Modo 2 - Cable Directo:
 *   Para vehiculos sin cierre centralizado (manuales, clasicos).
 *   Se instalan actuadores de motor en cada puerta y el ESP32 los controla
 *   directamente. Requiere mas cableado pero funciona en cualquier auto.
 * 
 * Modo 3 - CAN Bus:
 *   Para vehiculos modernos (2008+) con red CAN. Se envian mensajes
 *   directamente al BCM (Body Control Module) simulando los comandos
 *   del modulo de llave original. Es la integracion mas limpia pero
 *   requiere conocer los IDs CAN especificos del vehiculo.
 * 
 * =============================================================================
 */

#ifndef VEHICLE_INTERFACE_H
#define VEHICLE_INTERFACE_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Tipos de Interfaz
// =============================================================================

enum VehicleInterfaceMode {
    INTERFACE_CENTRALIZED = 0,  // Cierre centralizado (pulso)
    INTERFACE_DIRECT_WIRE,      // Cable directo (actuadores)
    INTERFACE_CAN_BUS           // Red CAN Bus
};

// =============================================================================
// Estado de la Interfaz de Vehiculo
// =============================================================================

struct VehicleStatus {
    VehicleInterfaceMode mode;  // Modo activo
    bool initialized;           // Interfaz inicializada
    bool doorsLocked;           // Estado de cerraduras
    bool engineRunning;         // Motor encendido
    bool accOn;                 // Accesorios
    bool ignitionOn;            // Ignicion
    
    // CAN Bus (solo modo 3)
    bool canBusActive;          // CAN Bus activo
    uint32_t canMessagesRx;     // Mensajes CAN recibidos
    uint32_t canMessagesTx;     // Mensajes CAN enviados
    uint32_t lastCanActivity;   // Ultimo mensaje CAN
};

// =============================================================================
// Resultado de Operaciones
// =============================================================================

enum VehicleResult {
    VEHICLE_OK = 0,
    VEHICLE_ERROR_NOT_INIT,
    VEHICLE_ERROR_CAN_SEND,
    VEHICLE_ERROR_CAN_TIMEOUT,
    VEHICLE_ERROR_INVALID_MODE,
    VEHICLE_ERROR_BUSY
};

// =============================================================================
// Configuracion CAN (para modo CAN Bus)
// =============================================================================

#ifdef VEHICLE_MODE_CAN_BUS

struct CanConfig {
    uint32_t lockId;            // ID CAN para bloquear
    uint32_t unlockId;          // ID CAN para desbloquear
    uint32_t engineStartId;     // ID CAN para arranque
    uint32_t engineStopId;      // ID CAN para apagar
    uint32_t doorStatusId;      // ID CAN de estado de puertas
    uint32_t engineStatusId;    // ID CAN de estado de motor
    uint32_t lightsId;          // ID CAN para luces
    uint8_t lockData[8];        // Datos del frame de bloqueo
    uint8_t unlockData[8];      // Datos del frame de desbloqueo
    uint8_t lockDataLen;        // Longitud datos bloqueo
    uint8_t unlockDataLen;      // Longitud datos desbloqueo
};

#endif

// =============================================================================
// Clase VehicleInterface
// =============================================================================

class VehicleInterface {
public:
    VehicleInterface();

    /**
     * Inicializa la interfaz de vehiculo segun el modo compilado
     * @return true si la inicializacion fue exitosa
     */
    bool init();

    /**
     * Actualiza la interfaz (lectura CAN, estados)
     * Llamar en cada iteracion del loop
     */
    void update();

    // =========================================================================
    // Cerraduras (abstracto - funciona igual en los 3 modos)
    // =========================================================================

    /**
     * Desbloquea las puertas del vehiculo
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult unlockDoors();

    /**
     * Bloquea las puertas del vehiculo
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult lockDoors();

    /**
     * Abre el maletero/cajuela
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult openTrunk();

    // =========================================================================
    // Encendido (abstracto)
    // =========================================================================

    /**
     * Activa accesorios (ACC)
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult activateAcc();

    /**
     * Desactiva accesorios
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult deactivateAcc();

    /**
     * Activa ignicion
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult activateIgnition();

    /**
     * Desactiva ignicion
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult deactivateIgnition();

    /**
     * Activa el motor de arranque
     * @param maxDurationMs Tiempo maximo de starter
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult activateStarter(uint16_t maxDurationMs);

    /**
     * Desactiva el motor de arranque
     * @return VEHICLE_OK si fue exitoso
     */
    VehicleResult deactivateStarter();

    // =========================================================================
    // Luces y Señales
    // =========================================================================

    /**
     * Parpadea las luces de parqueo (confirmacion visual)
     * @param count Numero de parpadeos
     */
    void flashLights(uint8_t count);

    /**
     * Activa las luces de parqueo (encontrar auto)
     * @param durationMs Duracion en ms
     */
    void lightsOn(uint32_t durationMs);

    // =========================================================================
    // Estado
    // =========================================================================

    /** @return Modo de interfaz activo */
    VehicleInterfaceMode getMode();

    /** @return Estado completo de la interfaz */
    VehicleStatus getStatus();

    /** @return true si la interfaz esta inicializada */
    bool isInitialized();

    /** @return String con el nombre del modo */
    const char* getModeName();

private:
    VehicleStatus _status;

    // =========================================================================
    // Implementaciones por modo (privadas)
    // =========================================================================

    // --- Modo 1: Cierre Centralizado ---
    bool initCentralized();
    VehicleResult centralizedLock();
    VehicleResult centralizedUnlock();
    void centralizedPulse(gpio_num_t pin, uint16_t durationMs);

    // --- Modo 2: Cable Directo ---
    bool initDirectWire();
    VehicleResult directWireLock();
    VehicleResult directWireUnlock();

    // --- Modo 3: CAN Bus ---
    #ifdef VEHICLE_MODE_CAN_BUS
    bool initCanBus();
    VehicleResult canLock();
    VehicleResult canUnlock();
    VehicleResult canSendFrame(uint32_t id, uint8_t* data, uint8_t length);
    void canProcessIncoming();
    CanConfig _canConfig;
    #endif

    // --- Comunes ---
    void activateRelayPin(gpio_num_t pin);
    void deactivateRelayPin(gpio_num_t pin);
    void pulseRelayPin(gpio_num_t pin, uint16_t durationMs);
};

#endif // VEHICLE_INTERFACE_H
