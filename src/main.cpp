/**
 * =============================================================================
 * PKE + Keyless Start System - Programa Principal
 * =============================================================================
 * 
 * Sistema de Entrada Pasiva Sin Llave + Encendido por Boton
 * Hardware: ESP32 + BLE
 * Llave: Telefono Android con app Flutter
 * 
 * Maquina de Estados Principal:
 * 
 *   [IDLE/LOCKED] --telefono detectado--> [AUTHENTICATING]
 *        ^                                       |
 *        |                              auth OK  |  auth FAIL
 *        |                                v      v
 *        |                         [UNLOCKED]  [IDLE]
 *        |                              |
 *        |                    entra cabina
 *        |                              v
 *        |                      [READY_TO_START]
 *        |                              |
 *        |                   boton + freno
 *        |                              v
 *        |                       [ENGINE_ON]
 *        |                              |
 *        |                      boton (apagar)
 *        |                              v
 *        |                     [UNLOCKED] --se aleja--> [LOCKING]
 *        |                                                  |
 *        +--------------------------------------------------+
 * 
 * =============================================================================
 */

#include <Arduino.h>
#include "config.h"
#include "ble_manager.h"
#include "security.h"
#include "zone_manager.h"
#include "actuators.h"
#include "vehicle_interface.h"


// =============================================================================
// Instancias Globales
// =============================================================================

BleManager bleManager;
ZoneManager zoneManager;
Actuators actuators;
VehicleInterface vehicle;

// Estado del sistema
SystemState currentState = STATE_IDLE;
SystemState previousState = STATE_IDLE;
uint32_t stateEnteredAt = 0;

// Control del loop
uint32_t lastLoopTime = 0;
const uint32_t LOOP_INTERVAL_MS = 50;  // 20 Hz loop principal

// Debounce del boton
bool lastButtonState = false;
uint32_t buttonPressedAt = 0;
const uint32_t BUTTON_DEBOUNCE_MS = 50;
const uint32_t BUTTON_LONG_PRESS_MS = 2000;

// Tag para logging
static const char* TAG_MAIN = "MAIN";

// =============================================================================
// Prototipos de Funciones
// =============================================================================

void changeState(SystemState newState);
void processZoneEvents();
void processBleCommands();
void processStartButton();
void handleStateIdle();
void handleStateAuthenticated();
void handleStateUnlocked();
void handleStateReadyToStart();
void handleStateEngineOn();
void handleStateLocking();
void handleStateRemoteStart();
void handleStateAlarm();
void printSystemInfo();


// =============================================================================
// SETUP
// =============================================================================

void setup() {
    // Inicializar serial para debug
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════╗");
    Serial.println("║   PKE + KEYLESS START SYSTEM                    ║");
    Serial.println("║   ESP32 + BLE + Android (Flutter)               ║");
    Serial.println("║   Version 1.0.0                                 ║");
    Serial.println("╚══════════════════════════════════════════════════╝");
    Serial.println();
    
    // --- Inicializar modulos ---
    
    // 1. Seguridad (primero - necesario para BLE auth)
    PKE_LOG(TAG_MAIN, "--- Inicializando Seguridad ---");
    if (!SecurityManager::init()) {
        PKE_LOG(TAG_MAIN, "ERROR CRITICO: Fallo al inicializar seguridad");
        while(1) { delay(1000); }  // Halt
    }
    
    // 2. Interfaz de vehiculo
    PKE_LOG(TAG_MAIN, "--- Inicializando Interfaz de Vehiculo ---");
    if (!vehicle.init()) {
        PKE_LOG(TAG_MAIN, "ERROR CRITICO: Fallo al inicializar interfaz de vehiculo");
        while(1) { delay(1000); }
    }
    
    // 3. Actuadores
    PKE_LOG(TAG_MAIN, "--- Inicializando Actuadores ---");
    if (!actuators.init()) {
        PKE_LOG(TAG_MAIN, "ERROR CRITICO: Fallo al inicializar actuadores");
        while(1) { delay(1000); }
    }
    
    // 4. Gestor de zonas
    PKE_LOG(TAG_MAIN, "--- Inicializando Zonas PKE ---");
    if (!zoneManager.init()) {
        PKE_LOG(TAG_MAIN, "ERROR CRITICO: Fallo al inicializar zonas PKE");
        while(1) { delay(1000); }
    }
    
    // 5. BLE (ultimo - comienza a escuchar conexiones)
    PKE_LOG(TAG_MAIN, "--- Inicializando BLE ---");
    if (!bleManager.init()) {
        PKE_LOG(TAG_MAIN, "ERROR CRITICO: Fallo al inicializar BLE");
        while(1) { delay(1000); }
    }
    
    // Estado inicial
    changeState(STATE_LOCKED);
    
    printSystemInfo();
    
    PKE_LOG(TAG_MAIN, "");
    PKE_LOG(TAG_MAIN, "=== SISTEMA LISTO - ESPERANDO CONEXION BLE ===");
    PKE_LOG(TAG_MAIN, "");
    
    lastLoopTime = millis();
}


// =============================================================================
// LOOP PRINCIPAL
// =============================================================================

void loop() {
    uint32_t now = millis();
    
    // Control de frecuencia del loop (20 Hz)
    if (now - lastLoopTime < LOOP_INTERVAL_MS) return;
    lastLoopTime = now;
    
    // --- Actualizar todos los modulos ---
    bleManager.update();
    actuators.update();
    vehicle.update();
    
    // --- Actualizar zona PKE si hay conexion autenticada ---
    if (bleManager.isAuthenticated()) {
        int16_t rssi = bleManager.getRssi();
        if (rssi != 0) {
            zoneManager.update(rssi);
        }
    } else if (bleManager.isConnected() && !bleManager.isAuthenticated()) {
        // Conectado pero no autenticado - esperar auth
    } else if (!bleManager.isConnected() && currentState != STATE_LOCKED && 
               currentState != STATE_IDLE) {
        // Conexion perdida estando en un estado activo
        zoneManager.onSignalLost();
    }
    
    // --- Procesar eventos de zona ---
    processZoneEvents();
    
    // --- Procesar comandos BLE ---
    processBleCommands();
    
    // --- Procesar boton fisico ---
    processStartButton();
    
    // --- Maquina de estados ---
    switch (currentState) {
        case STATE_IDLE:
        case STATE_LOCKED:
            handleStateIdle();
            break;
        case STATE_AUTHENTICATED:
        case STATE_UNLOCKED:
            handleStateUnlocked();
            break;
        case STATE_READY_TO_START:
            handleStateReadyToStart();
            break;
        case STATE_ENGINE_ON:
        case STATE_ENGINE_ACC:
            handleStateEngineOn();
            break;
        case STATE_LOCKING:
            handleStateLocking();
            break;
        case STATE_REMOTE_START:
            handleStateRemoteStart();
            break;
        case STATE_ALARM_TRIGGERED:
            handleStateAlarm();
            break;
        default:
            break;
    }
}


// =============================================================================
// CAMBIO DE ESTADO
// =============================================================================

void changeState(SystemState newState) {
    if (newState == currentState) return;
    
    previousState = currentState;
    currentState = newState;
    stateEnteredAt = millis();
    
    PKE_LOG(TAG_MAIN, ">>> ESTADO: %d -> %d", previousState, newState);
    
    // Notificar al telefono del cambio de estado
    bleManager.sendStateUpdate(newState);
    
    // Acciones de entrada al nuevo estado
    switch (newState) {
        case STATE_LOCKED:
            actuators.disableStartButton();
            break;
            
        case STATE_UNLOCKED:
            actuators.disableStartButton();
            break;
            
        case STATE_READY_TO_START:
            actuators.enableStartButton();
            break;
            
        case STATE_ENGINE_ON:
            // Mantener boton habilitado (para apagar)
            break;
            
        case STATE_LOCKING:
            actuators.disableStartButton();
            break;
            
        case STATE_ALARM_TRIGGERED:
            actuators.triggerAlarm();
            break;
            
        default:
            break;
    }
}


// =============================================================================
// PROCESAMIENTO DE EVENTOS DE ZONA
// =============================================================================

void processZoneEvents() {
    if (!zoneManager.hasEvent()) return;
    
    ZoneEvent event = zoneManager.getEvent();
    
    PKE_LOG(TAG_MAIN, "Evento de zona: %s", zoneManager.getEventName(event));
    
    switch (event) {
        case EVENT_ENTERED_NEAR:
            // Telefono se acerco al auto (1.5-2m)
            if (currentState == STATE_LOCKED || currentState == STATE_IDLE) {
                // DESBLOQUEO AUTOMATICO PKE
                PKE_LOG(TAG_MAIN, "*** PKE: DESBLOQUEO AUTOMATICO ***");
                vehicle.unlockDoors();
                actuators.unlockDoors();
                changeState(STATE_UNLOCKED);
            }
            break;
            
        case EVENT_ENTERED_INSIDE:
            // Telefono entro a la cabina
            if (currentState == STATE_UNLOCKED || currentState == STATE_AUTHENTICATED) {
                // HABILITAR BOTON START
                PKE_LOG(TAG_MAIN, "*** PKE: BOTON START HABILITADO ***");
                changeState(STATE_READY_TO_START);
            }
            break;
            
        case EVENT_EXITED_INSIDE:
            // Telefono salio de la cabina (pero sigue cerca)
            if (currentState == STATE_READY_TO_START) {
                // Deshabilitar boton (ya no esta dentro)
                PKE_LOG(TAG_MAIN, "*** PKE: BOTON START DESHABILITADO ***");
                changeState(STATE_UNLOCKED);
            }
            break;
            
        case EVENT_EXITED_NEAR:
            // Telefono se alejo del auto (>2m)
            if (currentState == STATE_UNLOCKED || 
                currentState == STATE_READY_TO_START ||
                currentState == STATE_AUTHENTICATED) {
                // INICIAR PROCESO DE BLOQUEO
                PKE_LOG(TAG_MAIN, "*** PKE: INICIANDO BLOQUEO AUTOMATICO ***");
                changeState(STATE_LOCKING);
            }
            break;
            
        case EVENT_PHONE_LOST:
            // Senal BLE perdida completamente
            if (currentState != STATE_ENGINE_ON && 
                currentState != STATE_REMOTE_START &&
                currentState != STATE_LOCKED) {
                PKE_LOG(TAG_MAIN, "*** SENAL PERDIDA: BLOQUEANDO ***");
                changeState(STATE_LOCKING);
            }
            break;
            
        case EVENT_PHONE_FOUND:
            // Senal recuperada
            PKE_LOG(TAG_MAIN, "Senal BLE recuperada");
            break;
            
        default:
            break;
    }
}


// =============================================================================
// PROCESAMIENTO DE COMANDOS BLE (desde la app Flutter)
// =============================================================================

void processBleCommands() {
    BleCommand cmd = bleManager.getLastCommand();
    if (cmd == CMD_NONE) return;
    
    bleManager.clearCommand();
    
    PKE_LOG(TAG_MAIN, "Comando BLE recibido: 0x%02X", cmd);
    
    switch (cmd) {
        case CMD_LOCK:
            PKE_LOG(TAG_MAIN, "[CMD] Bloquear puertas");
            vehicle.lockDoors();
            actuators.lockDoors();
            changeState(STATE_LOCKED);
            break;
            
        case CMD_UNLOCK:
            PKE_LOG(TAG_MAIN, "[CMD] Desbloquear puertas");
            vehicle.unlockDoors();
            actuators.unlockDoors();
            changeState(STATE_UNLOCKED);
            break;
            
        case CMD_START_ENGINE:
            PKE_LOG(TAG_MAIN, "[CMD] Arranque remoto");
            if (currentState == STATE_LOCKED || currentState == STATE_IDLE) {
                ActuatorResult result = actuators.remoteStart();
                if (result == ACTUATOR_OK) {
                    vehicle.activateAcc();
                    delay(IGNITION_SEQUENCE_DELAY);
                    vehicle.activateIgnition();
                    delay(IGNITION_SEQUENCE_DELAY);
                    vehicle.activateStarter(STARTER_MAX_TIME_MS);
                    changeState(STATE_REMOTE_START);
                } else {
                    PKE_LOG(TAG_MAIN, "Arranque remoto rechazado: %s", 
                            actuators.getResultName(result));
                    actuators.beepError();
                }
            }
            break;
            
        case CMD_STOP_ENGINE:
            PKE_LOG(TAG_MAIN, "[CMD] Apagar motor");
            actuators.stopEngine();
            vehicle.deactivateStarter();
            vehicle.deactivateIgnition();
            vehicle.deactivateAcc();
            if (currentState == STATE_REMOTE_START) {
                changeState(STATE_LOCKED);
            } else {
                changeState(STATE_UNLOCKED);
            }
            break;
            
        case CMD_TRUNK_OPEN:
            PKE_LOG(TAG_MAIN, "[CMD] Abrir maletero");
            vehicle.openTrunk();
            break;
            
        case CMD_PANIC:
            PKE_LOG(TAG_MAIN, "[CMD] PANICO");
            actuators.triggerAlarm();
            changeState(STATE_ALARM_TRIGGERED);
            break;
            
        case CMD_FIND_CAR:
            PKE_LOG(TAG_MAIN, "[CMD] Encontrar auto");
            vehicle.flashLights(3);
            actuators.beepUnlock();
            break;
            
        case CMD_STATUS_REQ:
            PKE_LOG(TAG_MAIN, "[CMD] Solicitud de estado");
            bleManager.sendStateUpdate(currentState);
            bleManager.sendRssiUpdate();
            break;
            
        case CMD_WINDOW_CLOSE:
            PKE_LOG(TAG_MAIN, "[CMD] Cerrar ventanas");
            actuators.closeWindows();
            break;
            
        case CMD_PAIR_REQUEST:
            PKE_LOG(TAG_MAIN, "[CMD] Solicitud de emparejamiento");
            if (bleManager.pairCurrentDevice()) {
                SecurityManager::resetRollingCode();
                PKE_LOG(TAG_MAIN, "Emparejamiento exitoso");
                actuators.beepUnlock();
            }
            break;
            
        case CMD_UNPAIR:
            PKE_LOG(TAG_MAIN, "[CMD] Desemparejar");
            bleManager.unpairDevice();
            SecurityManager::resetRollingCode();
            actuators.beepError();
            break;
            
        default:
            PKE_LOG(TAG_MAIN, "Comando no reconocido: 0x%02X", cmd);
            break;
    }
}


// =============================================================================
// PROCESAMIENTO DEL BOTON PUSH-TO-START
// =============================================================================

void processStartButton() {
    bool buttonPressed = actuators.isStartButtonPressed();
    
    // Detectar flanco de subida (boton recien presionado)
    if (buttonPressed && !lastButtonState) {
        buttonPressedAt = millis();
    }
    
    // Detectar flanco de bajada (boton soltado)
    if (!buttonPressed && lastButtonState) {
        uint32_t pressDuration = millis() - buttonPressedAt;
        
        // Ignorar rebotes
        if (pressDuration < BUTTON_DEBOUNCE_MS) {
            lastButtonState = buttonPressed;
            return;
        }
        
        // Pulsacion corta: toggle encendido/apagado
        if (pressDuration < BUTTON_LONG_PRESS_MS) {
            PKE_LOG(TAG_MAIN, "Boton START: pulsacion corta (%d ms)", pressDuration);
            
            switch (currentState) {
                case STATE_READY_TO_START:
                    // Arrancar motor (requiere freno)
                    if (actuators.isBrakePressed()) {
                        PKE_LOG(TAG_MAIN, "*** ARRANCANDO MOTOR ***");
                        vehicle.activateAcc();
                        delay(IGNITION_SEQUENCE_DELAY);
                        vehicle.activateIgnition();
                        delay(IGNITION_SEQUENCE_DELAY);
                        vehicle.activateStarter(STARTER_MAX_TIME_MS);
                        actuators.startEngine();
                        changeState(STATE_ENGINE_ON);
                    } else {
                        // Sin freno: solo ACC
                        PKE_LOG(TAG_MAIN, "Sin freno - activando solo ACC");
                        vehicle.activateAcc();
                        actuators.turnAccOn();
                        changeState(STATE_ENGINE_ACC);
                    }
                    break;
                    
                case STATE_ENGINE_ACC:
                    // Apagar ACC
                    PKE_LOG(TAG_MAIN, "Apagando ACC");
                    vehicle.deactivateAcc();
                    actuators.turnAccOff();
                    changeState(STATE_READY_TO_START);
                    break;
                    
                case STATE_ENGINE_ON:
                    // Apagar motor
                    PKE_LOG(TAG_MAIN, "*** APAGANDO MOTOR ***");
                    actuators.stopEngine();
                    vehicle.deactivateStarter();
                    vehicle.deactivateIgnition();
                    vehicle.deactivateAcc();
                    changeState(STATE_UNLOCKED);
                    break;
                    
                default:
                    // Boton no habilitado en este estado
                    if (!actuators.isStartButtonEnabled()) {
                        PKE_LOG(TAG_MAIN, "Boton presionado pero no habilitado");
                        actuators.beepError();
                    }
                    break;
            }
        }
        // Pulsacion larga: funcion especial (ej: panico)
        else {
            PKE_LOG(TAG_MAIN, "Boton START: pulsacion LARGA (%d ms)", pressDuration);
            // Pulsacion larga con motor apagado = encontrar auto
            if (currentState == STATE_LOCKED || currentState == STATE_IDLE) {
                vehicle.flashLights(5);
                actuators.beepAlarm();
            }
        }
    }
    
    lastButtonState = buttonPressed;
}


// =============================================================================
// HANDLERS DE ESTADO
// =============================================================================

void handleStateIdle() {
    // Estado IDLE/LOCKED: esperando conexion BLE
    // La transicion ocurre automaticamente cuando:
    // 1. El telefono se conecta via BLE
    // 2. La autenticacion challenge-response es exitosa
    // 3. El RSSI indica zona NEAR (evento EVENT_ENTERED_NEAR)
    
    if (bleManager.isAuthenticated() && currentState == STATE_IDLE) {
        changeState(STATE_LOCKED);
    }
    
    // Parpadeo lento del LED de estado (indica standby)
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 3000) {
        digitalWrite(PIN_LED_STATUS, HIGH);
        delay(50);
        digitalWrite(PIN_LED_STATUS, LOW);
        lastBlink = millis();
    }
}

void handleStateUnlocked() {
    // Puertas desbloqueadas, telefono cerca pero no dentro
    // Esperando a que entre al vehiculo o se aleje
    
    // LED de estado encendido fijo (indica desbloqueado)
    digitalWrite(PIN_LED_STATUS, HIGH);
    
    // Si la conexion BLE se pierde mientras esta desbloqueado,
    // dar un tiempo antes de bloquear (por si es interferencia momentanea)
    if (!bleManager.isConnected()) {
        // zoneManager.onSignalLost() ya se llama en el loop
        // La transicion a LOCKING ocurre por EVENT_PHONE_LOST o EVENT_EXITED_NEAR
    }
}

void handleStateReadyToStart() {
    // Dentro del vehiculo, boton habilitado
    // LED del boton encendido, esperando pulsacion
    
    // Parpadeo rapido del LED de estado
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 500) {
        static bool ledState = false;
        ledState = !ledState;
        digitalWrite(PIN_LED_STATUS, ledState ? HIGH : LOW);
        lastBlink = millis();
    }
    
    // Si el telefono sale de la cabina mientras no se ha arrancado
    // el zone_manager generara EVENT_EXITED_INSIDE y processZoneEvents lo manejara
}

void handleStateEngineOn() {
    // Motor encendido - mantener reles activos
    // El unico cambio posible es:
    // - Boton presionado: apagar motor
    // - Comando BLE CMD_STOP_ENGINE: apagar motor
    
    // LED de estado encendido solido
    digitalWrite(PIN_LED_STATUS, HIGH);
    
    // SEGURIDAD: Si el telefono se desconecta con el motor encendido,
    // NO apagar el motor (seria peligroso en movimiento)
    // Solo deshabilitar el boton para que no se pueda volver a arrancar
    if (!bleManager.isAuthenticated()) {
        // Motor sigue encendido pero no se puede interactuar
        // hasta que el telefono se reconecte
        actuators.disableStartButton();
    }
}

void handleStateLocking() {
    // Proceso de bloqueo en curso
    // Se ejecuta el bloqueo real y se transiciona a LOCKED
    
    PKE_LOG(TAG_MAIN, "*** PKE: BLOQUEANDO VEHICULO ***");
    
    // Bloquear puertas
    vehicle.lockDoors();
    actuators.lockDoors();
    
    // Transicionar a bloqueado
    changeState(STATE_LOCKED);
    
    // LED apagado
    digitalWrite(PIN_LED_STATUS, LOW);
}

void handleStateRemoteStart() {
    // Motor encendido por arranque remoto
    // Monitoreando timeout y esperando a que alguien se suba
    
    // Parpadeo medio del LED (indica arranque remoto)
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 1000) {
        static bool ledState = false;
        ledState = !ledState;
        digitalWrite(PIN_LED_STATUS, ledState ? HIGH : LOW);
        lastBlink = millis();
    }
    
    // Si el telefono entra en zona INSIDE, transicionar a ENGINE_ON normal
    if (zoneManager.getCurrentZone() == ZONE_INSIDE && bleManager.isAuthenticated()) {
        PKE_LOG(TAG_MAIN, "Telefono dentro del auto - pasando a modo ENGINE_ON");
        actuators.enableStartButton();
        changeState(STATE_ENGINE_ON);
    }
    
    // El timeout del arranque remoto lo maneja actuators.update()
    if (!actuators.isRemoteStartActive() && currentState == STATE_REMOTE_START) {
        PKE_LOG(TAG_MAIN, "Arranque remoto terminado (timeout o detenido)");
        vehicle.deactivateIgnition();
        vehicle.deactivateAcc();
        changeState(STATE_LOCKED);
    }
}

void handleStateAlarm() {
    // Alarma sonando
    // Se detiene por:
    // - Autenticacion exitosa (telefono del dueno)
    // - Timeout de sirena (ALARM_SIREN_TIME_MS)
    // - Comando CMD_UNLOCK
    
    if (bleManager.isAuthenticated()) {
        PKE_LOG(TAG_MAIN, "Dueno autenticado - desarmando alarma");
        actuators.silenceAlarm();
        actuators.disarmAlarm();
        changeState(STATE_UNLOCKED);
    }
    
    // Si la alarma se silencio por timeout, volver a LOCKED
    if (!actuators.isAlarmTriggered()) {
        changeState(STATE_LOCKED);
    }
}


// =============================================================================
// UTILIDADES
// =============================================================================

void printSystemInfo() {
    Serial.println();
    PKE_LOG(TAG_MAIN, "=== INFORMACION DEL SISTEMA ===");
    PKE_LOG(TAG_MAIN, "Chip: %s Rev %d", ESP.getChipModel(), ESP.getChipRevision());
    PKE_LOG(TAG_MAIN, "CPU: %d MHz, %d cores", ESP.getCpuFreqMHz(), ESP.getChipCores());
    PKE_LOG(TAG_MAIN, "Flash: %d MB", ESP.getFlashChipSize() / 1024 / 1024);
    PKE_LOG(TAG_MAIN, "Heap libre: %d bytes", ESP.getFreeHeap());
    PKE_LOG(TAG_MAIN, "SDK: %s", ESP.getSdkVersion());
    PKE_LOG(TAG_MAIN, "");
    PKE_LOG(TAG_MAIN, "--- Configuracion ---");
    PKE_LOG(TAG_MAIN, "Modo vehiculo: %s", vehicle.getModeName());
    PKE_LOG(TAG_MAIN, "BLE nombre: %s", BLE_DEVICE_NAME);
    PKE_LOG(TAG_MAIN, "Zonas RSSI: INSIDE>%d, NEAR>%d, FAR<%d",
            RSSI_ZONE_INSIDE, RSSI_ZONE_NEAR, RSSI_ZONE_FAR);
    PKE_LOG(TAG_MAIN, "Histeresis: %d dBm", RSSI_HYSTERESIS);
    PKE_LOG(TAG_MAIN, "Dwell time: %d ms", ZONE_DWELL_TIME_MS);
    PKE_LOG(TAG_MAIN, "Lock delay: %d ms", LOCK_DELAY_MS);
    PKE_LOG(TAG_MAIN, "Rolling codes: %s", ENABLE_ROLLING_CODES ? "SI" : "NO");
    PKE_LOG(TAG_MAIN, "Arranque remoto: %s", REMOTE_START_ENABLED ? "SI" : "NO");
    PKE_LOG(TAG_MAIN, "Alarma: %s", ALARM_ENABLED ? "SI" : "NO");
    PKE_LOG(TAG_MAIN, "Ventanas auto: %s", AUTO_WINDOW_CLOSE ? "SI" : "NO");
    PKE_LOG(TAG_MAIN, "Espejos auto: %s", AUTO_MIRRORS ? "SI" : "NO");
    PKE_LOG(TAG_MAIN, "================================");
    Serial.println();
}
