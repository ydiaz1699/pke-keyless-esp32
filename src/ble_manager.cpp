/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo BLE Manager (Implementacion)
 * =============================================================================
 */

#include "ble_manager.h"
#include "security.h"
#include <Preferences.h>

// Tag para logging
static const char* TAG_BLE = "BLE";

// =============================================================================
// CALLBACKS - Implementacion
// =============================================================================

// --- Server Callbacks ---

PkeServerCallbacks::PkeServerCallbacks(BleManager* manager) : _manager(manager) {}

void PkeServerCallbacks::onConnect(BLEServer* pServer) {
    PKE_LOG(TAG_BLE, "Dispositivo conectado");
    _manager->_status.connected = true;
    _manager->_status.connectedSince = millis();
    _manager->_status.authenticated = false;
    
    // Obtener info de la conexion
    // Nota: La direccion del dispositivo se obtiene despues de la conexion
    _manager->_status.connectedDeviceAddr = "connected";
    
    // Detener advertising mientras hay una conexion activa
    _manager->stopAdvertising();
    
    // Iniciar autenticacion enviando challenge
    delay(100); // Pequeño delay para estabilizar la conexion
    _manager->sendChallenge();
}

void PkeServerCallbacks::onDisconnect(BLEServer* pServer) {
    PKE_LOG(TAG_BLE, "Dispositivo desconectado");
    _manager->_status.connected = false;
    _manager->_status.authenticated = false;
    _manager->_status.rssi = 0;
    _manager->_status.rssiFiltered = 0;
    _manager->_status.connectedDeviceAddr = "";
    _manager->_lastCommand = CMD_NONE;
    _manager->_challengePending = false;
    
    // Reiniciar buffer RSSI
    _manager->_rssiSampleIndex = 0;
    _manager->_rssiBufferFull = false;
    memset(_manager->_rssiSamples, 0, sizeof(_manager->_rssiSamples));
    
    // Reiniciar advertising para permitir reconexiones
    delay(500);
    _manager->startAdvertising();
}

// --- Auth Characteristic Callbacks ---

AuthCharCallbacks::AuthCharCallbacks(BleManager* manager) : _manager(manager) {}

void AuthCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    // El telefono escribe su respuesta al challenge aqui
    std::string value = pCharacteristic->getValue();
    
    if (value.length() > 0) {
        PKE_LOG(TAG_BLE, "Respuesta de autenticacion recibida (%d bytes)", value.length());
        
        uint8_t* responseData = (uint8_t*)value.data();
        size_t responseLen = value.length();
        
        if (_manager->verifyAuthResponse(responseData, responseLen)) {
            PKE_LOG(TAG_BLE, "*** AUTENTICACION EXITOSA ***");
            _manager->setAuthenticated(true);
            _manager->sendNotification(NOTIFY_AUTH_SUCCESS);
        } else {
            PKE_LOG(TAG_BLE, "!!! AUTENTICACION FALLIDA !!!");
            _manager->setAuthenticated(false);
            _manager->sendNotification(NOTIFY_AUTH_FAIL);
        }
    }
}

void AuthCharCallbacks::onRead(BLECharacteristic* pCharacteristic) {
    // Cuando el telefono lee esta caracteristica, recibe el challenge actual
    PKE_LOG(TAG_BLE, "Challenge leido por el telefono");
}

// --- Command Characteristic Callbacks ---

CommandCharCallbacks::CommandCharCallbacks(BleManager* manager) : _manager(manager) {}

void CommandCharCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    // Solo procesar comandos si esta autenticado
    if (!_manager->_status.authenticated) {
        PKE_LOG(TAG_BLE, "Comando rechazado: no autenticado");
        return;
    }
    
    std::string value = pCharacteristic->getValue();
    
    if (value.length() >= 1) {
        uint8_t cmdByte = (uint8_t)value[0];
        
        // Validar que es un comando conocido
        if (cmdByte >= CMD_LOCK && cmdByte <= CMD_UNPAIR) {
            _manager->_lastCommand = (BleCommand)cmdByte;
            PKE_LOG(TAG_BLE, "Comando recibido: 0x%02X", cmdByte);
        } else {
            PKE_LOG(TAG_BLE, "Comando desconocido: 0x%02X", cmdByte);
        }
    }
}

// =============================================================================
// BLE MANAGER - Implementacion
// =============================================================================

BleManager::BleManager() {
    _server = nullptr;
    _service = nullptr;
    _authChar = nullptr;
    _commandChar = nullptr;
    _statusChar = nullptr;
    _rssiChar = nullptr;
    _advertising = nullptr;
    _serverCallbacks = nullptr;
    _authCallbacks = nullptr;
    _commandCallbacks = nullptr;
    _lastCommand = CMD_NONE;
    _challengePending = false;
    _challengeTimestamp = 0;
    _rssiSampleIndex = 0;
    _rssiBufferFull = false;
    
    memset(&_status, 0, sizeof(BleStatus));
    memset(_currentChallenge, 0, CHALLENGE_SIZE);
    memset(_rssiSamples, 0, sizeof(_rssiSamples));
}

// =============================================================================
// Inicializacion
// =============================================================================

bool BleManager::init() {
    PKE_LOG(TAG_BLE, "Inicializando BLE...");
    
    // Inicializar dispositivo BLE
    BLEDevice::init(BLE_DEVICE_NAME);
    
    // Configurar potencia de transmision
    // ESP_PWR_LVL_P9 = +9dBm (maximo alcance)
    // ESP_PWR_LVL_P3 = +3dBm (alcance medio - recomendado para PKE)
    BLEDevice::setPower(ESP_PWR_LVL_P3);
    
    // Crear servidor GATT
    setupGattServer();
    
    // Cargar dispositivo emparejado desde NVS
    loadPairedDevice();
    
    // Iniciar advertising
    startAdvertising();
    
    _status.initialized = true;
    PKE_LOG(TAG_BLE, "BLE inicializado correctamente");
    PKE_LOG(TAG_BLE, "Nombre: %s", BLE_DEVICE_NAME);
    PKE_LOG(TAG_BLE, "Servicio UUID: %s", SERVICE_UUID);
    
    return true;
}

void BleManager::setupGattServer() {
    // Crear servidor
    _server = BLEDevice::createServer();
    
    // Registrar callbacks de conexion
    _serverCallbacks = new PkeServerCallbacks(this);
    _server->setCallbacks(_serverCallbacks);
    
    // Crear servicio principal
    _service = _server->createService(SERVICE_UUID);
    
    // --- Caracteristica de Autenticacion ---
    // READ: El telefono lee el challenge
    // WRITE: El telefono envia la respuesta al challenge
    _authChar = _service->createCharacteristic(
        CHAR_AUTH_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE
    );
    _authCallbacks = new AuthCharCallbacks(this);
    _authChar->setCallbacks(_authCallbacks);
    
    // --- Caracteristica de Comandos ---
    // WRITE: El telefono envia comandos (lock, unlock, start, etc.)
    _commandChar = _service->createCharacteristic(
        CHAR_COMMAND_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _commandCallbacks = new CommandCharCallbacks(this);
    _commandChar->setCallbacks(_commandCallbacks);
    
    // --- Caracteristica de Estado ---
    // NOTIFY: El ESP32 notifica cambios de estado al telefono
    _statusChar = _service->createCharacteristic(
        CHAR_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    _statusChar->addDescriptor(new BLE2902());  // Descriptor para habilitar notificaciones
    
    // --- Caracteristica de RSSI ---
    // NOTIFY: El ESP32 notifica el nivel RSSI actual
    _rssiChar = _service->createCharacteristic(
        CHAR_RSSI_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    _rssiChar->addDescriptor(new BLE2902());
    
    // Iniciar servicio
    _service->start();
    
    PKE_LOG(TAG_BLE, "Servidor GATT configurado con 4 caracteristicas");
}

// =============================================================================
// Advertising
// =============================================================================

void BleManager::startAdvertising() {
    _advertising = BLEDevice::getAdvertising();
    _advertising->addServiceUUID(SERVICE_UUID);
    _advertising->setScanResponse(true);
    
    // Configurar intervalos de advertising
    // Menor intervalo = deteccion mas rapida pero mayor consumo
    _advertising->setMinPreferred(0x06);  // ~7.5ms (conexion rapida)
    _advertising->setMaxPreferred(0x12);  // ~22.5ms
    
    BLEDevice::startAdvertising();
    _status.advertising = true;
    PKE_LOG(TAG_BLE, "Advertising iniciado");
}

void BleManager::stopAdvertising() {
    BLEDevice::getAdvertising()->stop();
    _status.advertising = false;
    PKE_LOG(TAG_BLE, "Advertising detenido");
}

// =============================================================================
// Update (llamar en loop)
// =============================================================================

void BleManager::update() {
    if (!_status.initialized) return;
    
    // Si hay conexion activa, leer RSSI periodicamente
    if (_status.connected) {
        uint32_t now = millis();
        
        // Leer RSSI cada RSSI_READ_INTERVAL_MS
        if (now - _status.lastRssiRead >= RSSI_READ_INTERVAL_MS) {
            readRssi();
            _status.lastRssiRead = now;
        }
        
        // Verificar timeout de autenticacion
        if (_challengePending) {
            checkAuthTimeout();
        }
    }
}

// =============================================================================
// RSSI y Proximidad
// =============================================================================

void BleManager::readRssi() {
    if (!_status.connected || _server == nullptr) return;
    
    // Obtener RSSI de la conexion activa
    // El ESP32 BLE stack proporciona esta funcionalidad
    int rssi = _server->getPeerMTU(0);  // Placeholder - usaremos esp_ble_gap_read_rssi
    
    // Usar la API de bajo nivel para obtener RSSI real
    uint16_t connId = _server->getConnId();
    if (connId != 0xFFFF) {
        // esp_ble_gap_read_rssi provee el RSSI real
        // Para simplificar, usamos la conexion directa
        esp_ble_gap_cb_param_t* param = nullptr;
        
        // Metodo alternativo: leer desde el GAP
        // El RSSI se actualiza via callback, aqui leemos el ultimo valor
        int rawRssi = -100;  // Valor por defecto si no hay lectura
        
        // Intentar obtener RSSI via BLE API
        BLEAddress peerAddress = BLEAddress("");
        
        // Nota: En ESP32 Arduino BLE, el RSSI se obtiene mejor via
        // esp_ble_gap_read_rssi() que es asincrono. Implementamos un
        // approach simplificado usando el advertising RSSI reportado
        // por el cliente al conectarse.
        
        // Para el ESP32, usamos la API directa de ESP-IDF:
        esp_bd_addr_t remote_bda;
        // El RSSI de conexion activa se lee con:
        esp_err_t ret = esp_ble_gap_read_rssi(remote_bda);
        
        // El valor real llega por callback GAP - usamos cache
        rawRssi = _status.rssi;  // Mantener ultimo valor si falla
        
        // Aplicar filtro
        _status.rssi = rawRssi;
        _status.rssiFiltered = filterRssi(rawRssi);
    }
}

int16_t BleManager::filterRssi(int16_t rawRssi) {
    // Filtro de media movil para suavizar lecturas RSSI
    _rssiSamples[_rssiSampleIndex] = rawRssi;
    _rssiSampleIndex = (_rssiSampleIndex + 1) % RSSI_SAMPLE_COUNT;
    
    if (_rssiSampleIndex == 0) {
        _rssiBufferFull = true;
    }
    
    // Calcular promedio
    int32_t sum = 0;
    uint8_t count = _rssiBufferFull ? RSSI_SAMPLE_COUNT : _rssiSampleIndex;
    
    if (count == 0) return rawRssi;
    
    for (uint8_t i = 0; i < count; i++) {
        sum += _rssiSamples[i];
    }
    
    return (int16_t)(sum / count);
}

// =============================================================================
// Autenticacion Challenge-Response
// =============================================================================

bool BleManager::sendChallenge() {
    if (!_status.connected) return false;
    
    // Generar challenge aleatorio
    SecurityManager::generateChallenge(_currentChallenge, CHALLENGE_SIZE);
    
    // Escribir challenge en la caracteristica para que el telefono lo lea
    _authChar->setValue(_currentChallenge, CHALLENGE_SIZE);
    _authChar->notify();  // Notificar que hay un nuevo challenge
    
    _challengePending = true;
    _challengeTimestamp = millis();
    
    PKE_LOG(TAG_BLE, "Challenge enviado (%d bytes)", CHALLENGE_SIZE);
    
    return true;
}

bool BleManager::verifyAuthResponse(uint8_t* response, size_t length) {
    if (!_challengePending) {
        PKE_LOG(TAG_BLE, "No hay challenge pendiente");
        return false;
    }
    
    _challengePending = false;
    
    // Verificar la respuesta usando el modulo de seguridad
    bool valid = SecurityManager::verifyResponse(
        _currentChallenge, CHALLENGE_SIZE,
        response, length
    );
    
    if (!valid) {
        // Incrementar contador de fallos
        PKE_LOG(TAG_BLE, "Respuesta invalida - intento fallido");
    }
    
    // Limpiar challenge usado (one-time use)
    memset(_currentChallenge, 0, CHALLENGE_SIZE);
    
    return valid;
}

void BleManager::setAuthenticated(bool state) {
    _status.authenticated = state;
    
    if (state) {
        PKE_LOG(TAG_BLE, "Dispositivo marcado como AUTENTICADO");
    } else {
        PKE_LOG(TAG_BLE, "Dispositivo marcado como NO autenticado");
    }
}

void BleManager::checkAuthTimeout() {
    if (_challengePending && (millis() - _challengeTimestamp > AUTH_TIMEOUT_MS)) {
        PKE_LOG(TAG_BLE, "Timeout de autenticacion - challenge expirado");
        _challengePending = false;
        memset(_currentChallenge, 0, CHALLENGE_SIZE);
        
        // Enviar nuevo challenge (reintentar)
        sendChallenge();
    }
}

bool BleManager::isPairedDevice() {
    if (_status.connectedDeviceAddr.length() == 0) return false;
    
    String pairedAddr = getPairedDeviceAddress();
    if (pairedAddr.length() == 0) return true;  // Si no hay emparejado, aceptar cualquiera
    
    return (_status.connectedDeviceAddr == pairedAddr);
}

// =============================================================================
// Notificaciones
// =============================================================================

void BleManager::sendNotification(BleNotification notification, uint8_t* data, size_t length) {
    if (!_status.connected) return;
    
    // Crear buffer de notificacion: [tipo][datos...]
    uint8_t notifyBuffer[20];  // MTU minimo BLE = 20 bytes
    notifyBuffer[0] = (uint8_t)notification;
    
    if (data != nullptr && length > 0 && length < 19) {
        memcpy(&notifyBuffer[1], data, length);
    }
    
    size_t totalLen = 1 + (data ? min(length, (size_t)19) : 0);
    
    _statusChar->setValue(notifyBuffer, totalLen);
    _statusChar->notify();
    
    PKE_LOG(TAG_BLE, "Notificacion enviada: 0x%02X (%d bytes)", notification, totalLen);
}

void BleManager::sendStateUpdate(SystemState state) {
    uint8_t stateData[1] = { (uint8_t)state };
    sendNotification(NOTIFY_STATE_CHANGE, stateData, 1);
}

void BleManager::sendRssiUpdate() {
    if (!_status.connected) return;
    
    // Enviar RSSI como int16 (2 bytes, little-endian)
    int16_t rssi = _status.rssiFiltered;
    uint8_t rssiData[2];
    rssiData[0] = (uint8_t)(rssi & 0xFF);
    rssiData[1] = (uint8_t)((rssi >> 8) & 0xFF);
    
    _rssiChar->setValue(rssiData, 2);
    _rssiChar->notify();
}

// =============================================================================
// Emparejamiento (Persistencia NVS)
// =============================================================================

bool BleManager::pairCurrentDevice() {
    if (!_status.connected) return false;
    
    String addr = _status.connectedDeviceAddr;
    if (addr.length() == 0) return false;
    
    savePairedDevice(addr);
    PKE_LOG(TAG_BLE, "Dispositivo emparejado: %s", addr.c_str());
    
    return true;
}

bool BleManager::unpairDevice() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove(NVS_KEY_PAIRED_DEVICE);
    prefs.end();
    
    PKE_LOG(TAG_BLE, "Dispositivo desemparejado");
    return true;
}

String BleManager::getPairedDeviceAddress() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);  // Read-only
    String addr = prefs.getString(NVS_KEY_PAIRED_DEVICE, "");
    prefs.end();
    return addr;
}

void BleManager::loadPairedDevice() {
    String addr = getPairedDeviceAddress();
    if (addr.length() > 0) {
        PKE_LOG(TAG_BLE, "Dispositivo emparejado cargado: %s", addr.c_str());
    } else {
        PKE_LOG(TAG_BLE, "No hay dispositivo emparejado - aceptando cualquier conexion");
    }
}

void BleManager::savePairedDevice(const String& address) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(NVS_KEY_PAIRED_DEVICE, address);
    prefs.end();
}

// =============================================================================
// Getters de Estado
// =============================================================================

bool BleManager::isConnected() {
    return _status.connected;
}

bool BleManager::isAuthenticated() {
    return _status.connected && _status.authenticated;
}

int16_t BleManager::getRssi() {
    return _status.rssiFiltered;
}

int16_t BleManager::getRawRssi() {
    return _status.rssi;
}

BleStatus BleManager::getStatus() {
    return _status;
}

// =============================================================================
// Comandos
// =============================================================================

BleCommand BleManager::getLastCommand() {
    return _lastCommand;
}

void BleManager::clearCommand() {
    _lastCommand = CMD_NONE;
}
