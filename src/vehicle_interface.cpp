/**
 * =============================================================================
 * PKE + Keyless Start System - Interfaz de Vehiculo (Implementacion)
 * =============================================================================
 */

#include "vehicle_interface.h"

#ifdef VEHICLE_MODE_CAN_BUS
#include <CAN.h>
#endif

// Tag para logging
static const char* TAG_VEH = "VEH";

// =============================================================================
// Constructor
// =============================================================================

VehicleInterface::VehicleInterface() {
    memset(&_status, 0, sizeof(VehicleStatus));
    
    // Determinar modo segun compilacion
    #if defined(VEHICLE_MODE_CENTRALIZED)
    _status.mode = INTERFACE_CENTRALIZED;
    #elif defined(VEHICLE_MODE_DIRECT_WIRE)
    _status.mode = INTERFACE_DIRECT_WIRE;
    #elif defined(VEHICLE_MODE_CAN_BUS)
    _status.mode = INTERFACE_CAN_BUS;
    #endif
}

// =============================================================================
// Inicializacion
// =============================================================================

bool VehicleInterface::init() {
    PKE_LOG(TAG_VEH, "Inicializando interfaz de vehiculo...");
    PKE_LOG(TAG_VEH, "Modo: %s", getModeName());
    
    bool result = false;
    
    switch (_status.mode) {
        case INTERFACE_CENTRALIZED:
            result = initCentralized();
            break;
        case INTERFACE_DIRECT_WIRE:
            result = initDirectWire();
            break;
        #ifdef VEHICLE_MODE_CAN_BUS
        case INTERFACE_CAN_BUS:
            result = initCanBus();
            break;
        #endif
        default:
            PKE_LOG(TAG_VEH, "ERROR: Modo de interfaz no valido");
            return false;
    }
    
    if (result) {
        _status.initialized = true;
        PKE_LOG(TAG_VEH, "Interfaz de vehiculo inicializada correctamente");
    } else {
        PKE_LOG(TAG_VEH, "ERROR: Fallo al inicializar interfaz de vehiculo");
    }
    
    return result;
}

// =============================================================================
// Update
// =============================================================================

void VehicleInterface::update() {
    if (!_status.initialized) return;
    
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode == INTERFACE_CAN_BUS) {
        canProcessIncoming();
    }
    #endif
}

// =============================================================================
// Cerraduras (API Publica - Abstraccion)
// =============================================================================

VehicleResult VehicleInterface::unlockDoors() {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    PKE_LOG(TAG_VEH, ">>> Comando: DESBLOQUEAR PUERTAS");
    
    VehicleResult result = VEHICLE_ERROR_INVALID_MODE;
    
    switch (_status.mode) {
        case INTERFACE_CENTRALIZED:
            result = centralizedUnlock();
            break;
        case INTERFACE_DIRECT_WIRE:
            result = directWireUnlock();
            break;
        #ifdef VEHICLE_MODE_CAN_BUS
        case INTERFACE_CAN_BUS:
            result = canUnlock();
            break;
        #endif
    }
    
    if (result == VEHICLE_OK) {
        _status.doorsLocked = false;
        PKE_LOG(TAG_VEH, "Puertas desbloqueadas OK");
    }
    
    return result;
}

VehicleResult VehicleInterface::lockDoors() {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    PKE_LOG(TAG_VEH, ">>> Comando: BLOQUEAR PUERTAS");
    
    VehicleResult result = VEHICLE_ERROR_INVALID_MODE;
    
    switch (_status.mode) {
        case INTERFACE_CENTRALIZED:
            result = centralizedLock();
            break;
        case INTERFACE_DIRECT_WIRE:
            result = directWireLock();
            break;
        #ifdef VEHICLE_MODE_CAN_BUS
        case INTERFACE_CAN_BUS:
            result = canLock();
            break;
        #endif
    }
    
    if (result == VEHICLE_OK) {
        _status.doorsLocked = true;
        PKE_LOG(TAG_VEH, "Puertas bloqueadas OK");
    }
    
    return result;
}

VehicleResult VehicleInterface::openTrunk() {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    PKE_LOG(TAG_VEH, ">>> Comando: ABRIR MALETERO");
    
    // Para cierre centralizado y cable directo, pulsar el rele de desbloqueo
    // dos veces rapido (patron comun para abrir solo maletero)
    // Para CAN, enviar el frame especifico
    
    switch (_status.mode) {
        case INTERFACE_CENTRALIZED:
        case INTERFACE_DIRECT_WIRE:
            // Doble pulso = abrir maletero en muchos vehiculos
            pulseRelayPin(PIN_LOCK_UNLOCK, LOCK_PULSE_DURATION_MS);
            delay(200);
            pulseRelayPin(PIN_LOCK_UNLOCK, LOCK_PULSE_DURATION_MS);
            break;
        #ifdef VEHICLE_MODE_CAN_BUS
        case INTERFACE_CAN_BUS:
            // Enviar frame CAN de maletero (personalizar segun vehiculo)
            {
                uint8_t trunkData[2] = {0x01, 0x00};
                canSendFrame(CAN_ID_DOOR_UNLOCK, trunkData, 2);
            }
            break;
        #endif
    }
    
    return VEHICLE_OK;
}

// =============================================================================
// Sistema de Encendido (API Publica)
// =============================================================================

VehicleResult VehicleInterface::activateAcc() {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode == INTERFACE_CAN_BUS) {
        // En CAN Bus, enviar frame de ACC
        uint8_t accData[3] = {0x01, 0x00, 0x00};
        canSendFrame(CAN_ID_ENGINE_START, accData, 3);
    } else {
    #endif
        activateRelayPin(PIN_ACC);
    #ifdef VEHICLE_MODE_CAN_BUS
    }
    #endif
    
    _status.accOn = true;
    PKE_LOG(TAG_VEH, "ACC: ON");
    return VEHICLE_OK;
}

VehicleResult VehicleInterface::deactivateAcc() {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode == INTERFACE_CAN_BUS) {
        uint8_t accData[3] = {0x00, 0x00, 0x00};
        canSendFrame(CAN_ID_ENGINE_START, accData, 3);
    } else {
    #endif
        deactivateRelayPin(PIN_ACC);
    #ifdef VEHICLE_MODE_CAN_BUS
    }
    #endif
    
    _status.accOn = false;
    PKE_LOG(TAG_VEH, "ACC: OFF");
    return VEHICLE_OK;
}

VehicleResult VehicleInterface::activateIgnition() {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode == INTERFACE_CAN_BUS) {
        uint8_t ignData[3] = {0x01, 0x01, 0x00};
        canSendFrame(CAN_ID_ENGINE_START, ignData, 3);
    } else {
    #endif
        activateRelayPin(PIN_IGNITION);
    #ifdef VEHICLE_MODE_CAN_BUS
    }
    #endif
    
    _status.ignitionOn = true;
    PKE_LOG(TAG_VEH, "IGNICION: ON");
    return VEHICLE_OK;
}

VehicleResult VehicleInterface::deactivateIgnition() {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode == INTERFACE_CAN_BUS) {
        uint8_t ignData[3] = {0x00, 0x01, 0x00};
        canSendFrame(CAN_ID_ENGINE_START, ignData, 3);
    } else {
    #endif
        deactivateRelayPin(PIN_IGNITION);
    #ifdef VEHICLE_MODE_CAN_BUS
    }
    #endif
    
    _status.ignitionOn = false;
    PKE_LOG(TAG_VEH, "IGNICION: OFF");
    return VEHICLE_OK;
}

VehicleResult VehicleInterface::activateStarter(uint16_t maxDurationMs) {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    PKE_LOG(TAG_VEH, "STARTER: ACTIVANDO (max %d ms)", maxDurationMs);
    
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode == INTERFACE_CAN_BUS) {
        uint8_t startData[3] = {0x01, 0x01, 0x01};
        canSendFrame(CAN_ID_ENGINE_START, startData, 3);
        // En CAN, el BCM maneja el tiempo del starter
        delay(maxDurationMs);
        uint8_t stopData[3] = {0x01, 0x01, 0x00};
        canSendFrame(CAN_ID_ENGINE_START, stopData, 3);
    } else {
    #endif
        activateRelayPin(PIN_STARTER);
        delay(maxDurationMs);
        deactivateRelayPin(PIN_STARTER);
    #ifdef VEHICLE_MODE_CAN_BUS
    }
    #endif
    
    _status.engineRunning = true;
    PKE_LOG(TAG_VEH, "STARTER: COMPLETADO");
    return VEHICLE_OK;
}

VehicleResult VehicleInterface::deactivateStarter() {
    if (!_status.initialized) return VEHICLE_ERROR_NOT_INIT;
    
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode != INTERFACE_CAN_BUS) {
    #endif
        deactivateRelayPin(PIN_STARTER);
    #ifdef VEHICLE_MODE_CAN_BUS
    }
    #endif
    
    PKE_LOG(TAG_VEH, "STARTER: DESACTIVADO");
    return VEHICLE_OK;
}

// =============================================================================
// Luces
// =============================================================================

void VehicleInterface::flashLights(uint8_t count) {
    PKE_LOG(TAG_VEH, "Parpadeo de luces x%d", count);
    
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode == INTERFACE_CAN_BUS) {
        for (uint8_t i = 0; i < count; i++) {
            uint8_t lightsOn[2] = {0x01, 0x00};
            canSendFrame(CAN_ID_LIGHTS, lightsOn, 2);
            delay(300);
            uint8_t lightsOff[2] = {0x00, 0x00};
            canSendFrame(CAN_ID_LIGHTS, lightsOff, 2);
            if (i < count - 1) delay(300);
        }
    }
    // Nota: En modo centralizado/cable directo, las luces se manejan
    // tipicamente por la propia central del auto al recibir el pulso
    // de bloqueo/desbloqueo (ya incluido en el comportamiento del vehiculo)
    #endif
}

void VehicleInterface::lightsOn(uint32_t durationMs) {
    #ifdef VEHICLE_MODE_CAN_BUS
    if (_status.mode == INTERFACE_CAN_BUS) {
        uint8_t lightsOn[2] = {0x01, 0x00};
        canSendFrame(CAN_ID_LIGHTS, lightsOn, 2);
        delay(durationMs);
        uint8_t lightsOff[2] = {0x00, 0x00};
        canSendFrame(CAN_ID_LIGHTS, lightsOff, 2);
    }
    #endif
}

// =============================================================================
// MODO 1: Cierre Centralizado
// =============================================================================

bool VehicleInterface::initCentralized() {
    PKE_LOG(TAG_VEH, "Configurando Modo: CIERRE CENTRALIZADO");
    PKE_LOG(TAG_VEH, "  Pin Desbloqueo: GPIO%d", PIN_LOCK_UNLOCK);
    PKE_LOG(TAG_VEH, "  Pin Bloqueo: GPIO%d", PIN_LOCK_LOCK);
    PKE_LOG(TAG_VEH, "  Duracion pulso: %d ms", LOCK_PULSE_DURATION_MS);
    
    // Configurar pines de cerraduras como salida
    pinMode(PIN_LOCK_UNLOCK, OUTPUT);
    pinMode(PIN_LOCK_LOCK, OUTPUT);
    
    // Asegurar estado inicial
    deactivateRelayPin(PIN_LOCK_UNLOCK);
    deactivateRelayPin(PIN_LOCK_LOCK);
    
    // Configurar pines de encendido
    pinMode(PIN_ACC, OUTPUT);
    pinMode(PIN_IGNITION, OUTPUT);
    pinMode(PIN_STARTER, OUTPUT);
    deactivateRelayPin(PIN_ACC);
    deactivateRelayPin(PIN_IGNITION);
    deactivateRelayPin(PIN_STARTER);
    
    return true;
}

VehicleResult VehicleInterface::centralizedUnlock() {
    // En cierre centralizado, un pulso corto en el cable "unlock"
    // activa el modulo de cierre original del vehiculo
    centralizedPulse(PIN_LOCK_UNLOCK, LOCK_PULSE_DURATION_MS);
    return VEHICLE_OK;
}

VehicleResult VehicleInterface::centralizedLock() {
    // Pulso corto en el cable "lock"
    centralizedPulse(PIN_LOCK_LOCK, LOCK_PULSE_DURATION_MS);
    return VEHICLE_OK;
}

void VehicleInterface::centralizedPulse(gpio_num_t pin, uint16_t durationMs) {
    PKE_LOG(TAG_VEH, "[CENTRALIZADO] Pulso GPIO%d por %d ms", pin, durationMs);
    activateRelayPin(pin);
    delay(durationMs);
    deactivateRelayPin(pin);
}

// =============================================================================
// MODO 2: Cable Directo
// =============================================================================

bool VehicleInterface::initDirectWire() {
    PKE_LOG(TAG_VEH, "Configurando Modo: CABLE DIRECTO");
    PKE_LOG(TAG_VEH, "  Los actuadores de puerta se controlan directamente");
    PKE_LOG(TAG_VEH, "  Pin Desbloqueo (actuadores): GPIO%d", PIN_LOCK_UNLOCK);
    PKE_LOG(TAG_VEH, "  Pin Bloqueo (actuadores): GPIO%d", PIN_LOCK_LOCK);
    
    // En cable directo, los pines controlan actuadores de motor
    // que fisicamente mueven las cerraduras
    pinMode(PIN_LOCK_UNLOCK, OUTPUT);
    pinMode(PIN_LOCK_LOCK, OUTPUT);
    
    deactivateRelayPin(PIN_LOCK_UNLOCK);
    deactivateRelayPin(PIN_LOCK_LOCK);
    
    // Pines de encendido
    pinMode(PIN_ACC, OUTPUT);
    pinMode(PIN_IGNITION, OUTPUT);
    pinMode(PIN_STARTER, OUTPUT);
    deactivateRelayPin(PIN_ACC);
    deactivateRelayPin(PIN_IGNITION);
    deactivateRelayPin(PIN_STARTER);
    
    return true;
}

VehicleResult VehicleInterface::directWireLock() {
    // En cable directo, activamos el actuador de bloqueo
    // por un tiempo suficiente para que el motor complete el movimiento
    PKE_LOG(TAG_VEH, "[CABLE DIRECTO] Activando actuadores de bloqueo");
    
    // Asegurar que unlock no este activo (evitar cortocircuito)
    deactivateRelayPin(PIN_LOCK_UNLOCK);
    delay(50);  // Delay de seguridad
    
    // Activar bloqueo por tiempo suficiente para el motor del actuador
    activateRelayPin(PIN_LOCK_LOCK);
    delay(800);  // Los actuadores de motor necesitan ~500-800ms
    deactivateRelayPin(PIN_LOCK_LOCK);
    
    return VEHICLE_OK;
}

VehicleResult VehicleInterface::directWireUnlock() {
    PKE_LOG(TAG_VEH, "[CABLE DIRECTO] Activando actuadores de desbloqueo");
    
    // Asegurar que lock no este activo
    deactivateRelayPin(PIN_LOCK_LOCK);
    delay(50);
    
    // Activar desbloqueo
    activateRelayPin(PIN_LOCK_UNLOCK);
    delay(800);
    deactivateRelayPin(PIN_LOCK_UNLOCK);
    
    return VEHICLE_OK;
}

// =============================================================================
// MODO 3: CAN Bus
// =============================================================================

#ifdef VEHICLE_MODE_CAN_BUS

bool VehicleInterface::initCanBus() {
    PKE_LOG(TAG_VEH, "Configurando Modo: CAN BUS");
    PKE_LOG(TAG_VEH, "  TX: GPIO%d, RX: GPIO%d", PIN_CAN_TX, PIN_CAN_RX);
    PKE_LOG(TAG_VEH, "  Velocidad: %d kbps", (int)(CAN_SPEED / 1000));
    
    // Configurar pines CAN
    CAN.setPins(PIN_CAN_RX, PIN_CAN_TX);
    
    // Iniciar CAN Bus
    if (!CAN.begin((long)CAN_SPEED)) {
        PKE_LOG(TAG_VEH, "ERROR: Fallo al iniciar CAN Bus!");
        return false;
    }
    
    // Configurar IDs CAN (estos valores son genericos y DEBEN personalizarse)
    _canConfig.lockId = CAN_ID_DOOR_LOCK;
    _canConfig.unlockId = CAN_ID_DOOR_UNLOCK;
    _canConfig.engineStartId = CAN_ID_ENGINE_START;
    _canConfig.doorStatusId = CAN_ID_DOOR_STATUS;
    _canConfig.engineStatusId = CAN_ID_ENGINE_STATUS;
    _canConfig.lightsId = CAN_ID_LIGHTS;
    
    // Datos de frame para bloqueo (personalizar segun vehiculo)
    _canConfig.lockData[0] = 0x01;
    _canConfig.lockData[1] = 0x00;
    _canConfig.lockDataLen = 2;
    
    // Datos de frame para desbloqueo
    _canConfig.unlockData[0] = 0x02;
    _canConfig.unlockData[1] = 0x00;
    _canConfig.unlockDataLen = 2;
    
    _status.canBusActive = true;
    
    // Tambien configurar pines de rele para encendido (backup)
    // Algunos vehiculos necesitan reles ademas de CAN para el starter
    pinMode(PIN_ACC, OUTPUT);
    pinMode(PIN_IGNITION, OUTPUT);
    pinMode(PIN_STARTER, OUTPUT);
    deactivateRelayPin(PIN_ACC);
    deactivateRelayPin(PIN_IGNITION);
    deactivateRelayPin(PIN_STARTER);
    
    PKE_LOG(TAG_VEH, "CAN Bus iniciado correctamente");
    return true;
}

VehicleResult VehicleInterface::canLock() {
    PKE_LOG(TAG_VEH, "[CAN] Enviando frame de bloqueo (ID: 0x%03X)", _canConfig.lockId);
    return canSendFrame(_canConfig.lockId, _canConfig.lockData, _canConfig.lockDataLen);
}

VehicleResult VehicleInterface::canUnlock() {
    PKE_LOG(TAG_VEH, "[CAN] Enviando frame de desbloqueo (ID: 0x%03X)", _canConfig.unlockId);
    return canSendFrame(_canConfig.unlockId, _canConfig.unlockData, _canConfig.unlockDataLen);
}

VehicleResult VehicleInterface::canSendFrame(uint32_t id, uint8_t* data, uint8_t length) {
    CAN.beginPacket(id);
    CAN.write(data, length);
    
    if (CAN.endPacket()) {
        _status.canMessagesTx++;
        _status.lastCanActivity = millis();
        PKE_LOG(TAG_VEH, "[CAN TX] ID=0x%03X, Len=%d", id, length);
        return VEHICLE_OK;
    } else {
        PKE_LOG(TAG_VEH, "[CAN ERROR] Fallo al enviar frame ID=0x%03X", id);
        return VEHICLE_ERROR_CAN_SEND;
    }
}

void VehicleInterface::canProcessIncoming() {
    // Verificar si hay frames CAN entrantes
    int packetSize = CAN.parsePacket();
    
    if (packetSize > 0 && !CAN.packetRtr()) {
        uint32_t packetId = CAN.packetId();
        uint8_t data[8];
        uint8_t len = 0;
        
        while (CAN.available() && len < 8) {
            data[len++] = CAN.read();
        }
        
        _status.canMessagesRx++;
        _status.lastCanActivity = millis();
        
        // Procesar frames de estado
        if (packetId == _canConfig.doorStatusId) {
            // Interpretar estado de puertas desde el CAN
            // Byte 0: 0x00 = cerradas, 0x01 = alguna abierta
            if (len > 0) {
                _status.doorsLocked = (data[0] == 0x00);
            }
        }
        else if (packetId == _canConfig.engineStatusId) {
            // Interpretar estado del motor
            if (len > 0) {
                _status.engineRunning = (data[0] > 0);
            }
        }
        
        #ifdef DEBUG_MODE
        PKE_LOG(TAG_VEH, "[CAN RX] ID=0x%03X, Len=%d, Data=%02X%02X%02X%02X",
                packetId, len,
                len > 0 ? data[0] : 0,
                len > 1 ? data[1] : 0,
                len > 2 ? data[2] : 0,
                len > 3 ? data[3] : 0);
        #endif
    }
}

#endif // VEHICLE_MODE_CAN_BUS

// =============================================================================
// Utilidades Comunes
// =============================================================================

void VehicleInterface::activateRelayPin(gpio_num_t pin) {
    digitalWrite(pin, RELAY_ACTIVE);
}

void VehicleInterface::deactivateRelayPin(gpio_num_t pin) {
    digitalWrite(pin, RELAY_INACTIVE);
}

void VehicleInterface::pulseRelayPin(gpio_num_t pin, uint16_t durationMs) {
    activateRelayPin(pin);
    delay(durationMs);
    deactivateRelayPin(pin);
}

// =============================================================================
// Getters de Estado
// =============================================================================

VehicleInterfaceMode VehicleInterface::getMode() {
    return _status.mode;
}

VehicleStatus VehicleInterface::getStatus() {
    return _status;
}

bool VehicleInterface::isInitialized() {
    return _status.initialized;
}

const char* VehicleInterface::getModeName() {
    switch (_status.mode) {
        case INTERFACE_CENTRALIZED: return "CIERRE CENTRALIZADO";
        case INTERFACE_DIRECT_WIRE: return "CABLE DIRECTO";
        case INTERFACE_CAN_BUS:     return "CAN BUS";
        default:                    return "DESCONOCIDO";
    }
}
