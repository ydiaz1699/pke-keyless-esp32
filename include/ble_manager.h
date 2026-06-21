/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo BLE Manager (Header)
 * =============================================================================
 * 
 * Gestiona toda la comunicacion Bluetooth Low Energy:
 * - GATT Server con servicios y caracteristicas
 * - Advertising para ser descubierto por la app Flutter
 * - Lectura de RSSI para deteccion de proximidad
 * - Autenticacion challenge-response
 * - Envio de notificaciones al telefono
 * 
 * =============================================================================
 */

#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "config.h"

// =============================================================================
// Callbacks forward declarations
// =============================================================================

class BleManager;  // Forward declaration

// Callback de conexion/desconexion del servidor BLE
class PkeServerCallbacks : public BLEServerCallbacks {
public:
    PkeServerCallbacks(BleManager* manager);
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

private:
    BleManager* _manager;
};

// Callback para la caracteristica de autenticacion
class AuthCharCallbacks : public BLECharacteristicCallbacks {
public:
    AuthCharCallbacks(BleManager* manager);
    void onWrite(BLECharacteristic* pCharacteristic) override;
    void onRead(BLECharacteristic* pCharacteristic) override;

private:
    BleManager* _manager;
};

// Callback para la caracteristica de comandos
class CommandCharCallbacks : public BLECharacteristicCallbacks {
public:
    CommandCharCallbacks(BleManager* manager);
    void onWrite(BLECharacteristic* pCharacteristic) override;

private:
    BleManager* _manager;
};

// =============================================================================
// Estructura de datos BLE
// =============================================================================

struct BleStatus {
    bool initialized;           // BLE inicializado correctamente
    bool advertising;           // Advertising activo
    bool connected;             // Dispositivo conectado
    bool authenticated;         // Dispositivo autenticado
    int16_t rssi;              // Ultimo RSSI leido
    int16_t rssiFiltered;      // RSSI filtrado (promedio)
    uint32_t lastRssiRead;     // Timestamp ultimo RSSI
    uint32_t connectedSince;   // Timestamp de conexion
    String connectedDeviceAddr; // MAC del dispositivo conectado
};

// =============================================================================
// Clase Principal BLE Manager
// =============================================================================

class BleManager {
public:
    BleManager();

    /**
     * Inicializa el stack BLE completo
     * - Crea el servidor GATT
     * - Registra servicios y caracteristicas
     * - Inicia advertising
     * @return true si la inicializacion fue exitosa
     */
    bool init();

    /**
     * Actualiza el modulo BLE (llamar en loop)
     * - Lee RSSI periodicamente
     * - Verifica timeouts
     * - Gestiona reconexiones
     */
    void update();

    /**
     * Inicia el advertising BLE
     */
    void startAdvertising();

    /**
     * Detiene el advertising BLE
     */
    void stopAdvertising();

    // --- Estado ---
    
    /** @return true si hay un dispositivo conectado */
    bool isConnected();

    /** @return true si el dispositivo conectado esta autenticado */
    bool isAuthenticated();

    /** @return RSSI filtrado actual (0 si no hay conexion) */
    int16_t getRssi();

    /** @return RSSI sin filtrar (ultima lectura) */
    int16_t getRawRssi();

    /** @return estructura completa de estado BLE */
    BleStatus getStatus();

    // --- Autenticacion ---

    /**
     * Genera un nuevo challenge y lo envia al telefono
     * @return true si el challenge fue enviado
     */
    bool sendChallenge();

    /**
     * Verifica la respuesta al challenge
     * @param response Buffer con la respuesta cifrada del telefono
     * @param length Tamano de la respuesta
     * @return true si la autenticacion es valida
     */
    bool verifyAuthResponse(uint8_t* response, size_t length);

    /**
     * Marca el dispositivo como autenticado
     */
    void setAuthenticated(bool state);

    /**
     * Verifica si el dispositivo conectado es el emparejado
     * @return true si la MAC coincide con el dispositivo guardado
     */
    bool isPairedDevice();

    // --- Notificaciones ---

    /**
     * Envia una notificacion de cambio de estado al telefono
     * @param notification Tipo de notificacion
     * @param data Datos adicionales (opcional)
     * @param length Tamano de datos adicionales
     */
    void sendNotification(BleNotification notification, uint8_t* data = nullptr, size_t length = 0);

    /**
     * Envia el estado actual del sistema al telefono
     * @param state Estado actual del sistema
     */
    void sendStateUpdate(SystemState state);

    /**
     * Envia el valor RSSI actual al telefono
     */
    void sendRssiUpdate();

    // --- Emparejamiento ---

    /**
     * Guarda el dispositivo actual como dispositivo emparejado
     * @return true si se guardo correctamente en NVS
     */
    bool pairCurrentDevice();

    /**
     * Elimina el dispositivo emparejado
     * @return true si se elimino correctamente
     */
    bool unpairDevice();

    /**
     * Obtiene la MAC del dispositivo emparejado
     * @return String con la MAC guardada ("" si no hay)
     */
    String getPairedDeviceAddress();

    // --- Comandos recibidos ---

    /**
     * Obtiene el ultimo comando recibido del telefono
     * @return Codigo del comando (CMD_NONE si no hay pendiente)
     */
    BleCommand getLastCommand();

    /**
     * Limpia el comando pendiente (ya fue procesado)
     */
    void clearCommand();

    // --- Amigos (callbacks necesitan acceso interno) ---
    friend class PkeServerCallbacks;
    friend class AuthCharCallbacks;
    friend class CommandCharCallbacks;

private:
    // Componentes BLE
    BLEServer* _server;
    BLEService* _service;
    BLECharacteristic* _authChar;
    BLECharacteristic* _commandChar;
    BLECharacteristic* _statusChar;
    BLECharacteristic* _rssiChar;
    BLEAdvertising* _advertising;

    // Callbacks
    PkeServerCallbacks* _serverCallbacks;
    AuthCharCallbacks* _authCallbacks;
    CommandCharCallbacks* _commandCallbacks;

    // Estado interno
    BleStatus _status;
    BleCommand _lastCommand;
    uint8_t _currentChallenge[CHALLENGE_SIZE];
    bool _challengePending;
    uint32_t _challengeTimestamp;

    // Buffer RSSI para filtrado
    int16_t _rssiSamples[RSSI_SAMPLE_COUNT];
    uint8_t _rssiSampleIndex;
    bool _rssiBufferFull;

    // --- Metodos privados ---

    /** Configura el servidor GATT con servicios y caracteristicas */
    void setupGattServer();

    /** Lee el RSSI actual de la conexion */
    void readRssi();

    /** Aplica filtro de media movil al RSSI */
    int16_t filterRssi(int16_t rawRssi);

    /** Verifica timeouts de autenticacion */
    void checkAuthTimeout();

    /** Carga el dispositivo emparejado desde NVS */
    void loadPairedDevice();

    /** Guarda dispositivo emparejado en NVS */
    void savePairedDevice(const String& address);
};

#endif // BLE_MANAGER_H
