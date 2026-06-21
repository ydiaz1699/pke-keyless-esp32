/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo de Actuadores (Implementacion)
 * =============================================================================
 */

#include "actuators.h"

// Tag para logging
static const char* TAG_ACT = "ACT";

// =============================================================================
// Constructor
// =============================================================================

Actuators::Actuators() {
    memset(&_status, 0, sizeof(ActuatorStatus));
    _status.doorsLocked = true;  // Asumir bloqueado al inicio
    _initialized = false;
    _buzzerActive = false;
    _buzzerToneOn = false;
    _buzzerBeepIndex = 0;
    _buzzerLastToggle = 0;
    memset(&_activeBuzzerPattern, 0, sizeof(BuzzerPattern));
}

// =============================================================================
// Inicializacion
// =============================================================================

bool Actuators::init() {
    PKE_LOG(TAG_ACT, "Inicializando actuadores...");
    
    setupPins();
    
    // Asegurar que todos los reles esten apagados al inicio
    deactivateRelay(PIN_LOCK_UNLOCK);
    deactivateRelay(PIN_LOCK_LOCK);
    deactivateRelay(PIN_ACC);
    deactivateRelay(PIN_IGNITION);
    deactivateRelay(PIN_STARTER);
    
    // LED del boton apagado
    digitalWrite(PIN_BUTTON_LED, LOW);
    
    // LED de estado: un parpadeo rapido indica arranque exitoso
    digitalWrite(PIN_LED_STATUS, HIGH);
    delay(100);
    digitalWrite(PIN_LED_STATUS, LOW);
    
    // Leer estado inicial de sensores
    readSensors();
    
    _initialized = true;
    
    PKE_LOG(TAG_ACT, "Actuadores inicializados");
    PKE_LOG(TAG_ACT, "  Freno: %s", _status.brakePedal ? "PISADO" : "libre");
    PKE_LOG(TAG_ACT, "  Puerta: %s", _status.doorOpen ? "ABIERTA" : "cerrada");
    PKE_LOG(TAG_ACT, "  Capo: %s", _status.hoodOpen ? "ABIERTO" : "cerrado");
    
    return true;
}

void Actuators::setupPins() {
    // --- Salidas (Reles) ---
    pinMode(PIN_LOCK_UNLOCK, OUTPUT);
    pinMode(PIN_LOCK_LOCK, OUTPUT);
    pinMode(PIN_ACC, OUTPUT);
    pinMode(PIN_IGNITION, OUTPUT);
    pinMode(PIN_STARTER, OUTPUT);
    pinMode(PIN_BUTTON_LED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    
    #if AUTO_WINDOW_CLOSE
    pinMode(PIN_WINDOW_CLOSE, OUTPUT);
    digitalWrite(PIN_WINDOW_CLOSE, LOW);
    #endif
    
    #if AUTO_MIRRORS
    pinMode(PIN_MIRRORS_FOLD, OUTPUT);
    pinMode(PIN_MIRRORS_UNFOLD, OUTPUT);
    digitalWrite(PIN_MIRRORS_FOLD, LOW);
    digitalWrite(PIN_MIRRORS_UNFOLD, LOW);
    #endif
    
    // --- Entradas (Sensores) ---
    pinMode(PIN_START_BUTTON, INPUT_PULLUP);  // Boton con pull-up
    pinMode(PIN_BRAKE_PEDAL, INPUT);          // Sensor de freno
    pinMode(PIN_DOOR_SENSOR, INPUT_PULLUP);   // Sensor de puerta
    pinMode(PIN_HOOD_SENSOR, INPUT_PULLUP);   // Sensor de capo
}

// =============================================================================
// Update (llamar en loop)
// =============================================================================

void Actuators::update() {
    if (!_initialized) return;
    
    // Leer sensores
    readSensors();
    
    // Actualizar temporizadores
    updateTimers();
    
    // Actualizar buzzer non-blocking
    if (_buzzerActive) {
        updateBuzzer();
    }
}

void Actuators::readSensors() {
    // Leer boton (activo en LOW con pull-up)
    _status.startButtonPressed = (digitalRead(PIN_START_BUTTON) == LOW);
    
    // Leer freno (activo en HIGH cuando esta pisado)
    _status.brakePedal = (digitalRead(PIN_BRAKE_PEDAL) == HIGH);
    
    // Leer puerta (activo en LOW = abierta, con pull-up)
    _status.doorOpen = (digitalRead(PIN_DOOR_SENSOR) == LOW);
    
    // Leer capo (activo en LOW = abierto, con pull-up)
    _status.hoodOpen = (digitalRead(PIN_HOOD_SENSOR) == LOW);
}

void Actuators::updateTimers() {
    uint32_t now = millis();
    
    // --- Timeout de alarma ---
    if (_status.alarmTriggered) {
        if (now - _status.alarmTriggeredAt >= ALARM_SIREN_TIME_MS) {
            silenceAlarm();
            PKE_LOG(TAG_ACT, "Alarma: timeout de sirena alcanzado");
        }
    }
    
    // --- Timeout de arranque remoto ---
    if (_status.remoteStartActive) {
        if (now - _status.remoteStartedAt >= REMOTE_START_MAX_TIME) {
            PKE_LOG(TAG_ACT, "Arranque remoto: timeout alcanzado - apagando motor");
            remoteStop();
        }
    }
    
    // --- Armado de alarma con delay ---
    #if ALARM_ENABLED
    if (_status.doorsLocked && !_status.alarmArmed && _status.lastLockAction > 0) {
        if (now - _status.lastLockAction >= ALARM_ARM_DELAY_MS) {
            _status.alarmArmed = true;
            PKE_LOG(TAG_ACT, "Alarma armada automaticamente");
        }
    }
    #endif
    
    // --- Deteccion de intrusion (alarma armada + puerta abierta) ---
    if (_status.alarmArmed && _status.doorOpen && !_status.alarmTriggered) {
        PKE_LOG(TAG_ACT, "!!! INTRUSION DETECTADA - DISPARANDO ALARMA !!!");
        triggerAlarm();
    }
}

// =============================================================================
// Cerraduras
// =============================================================================

ActuatorResult Actuators::unlockDoors() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    if (!_status.doorsLocked) {
        PKE_LOG(TAG_ACT, "Puertas ya desbloqueadas");
        return ACTUATOR_ERROR_ALREADY_UNLOCKED;
    }
    
    PKE_LOG(TAG_ACT, ">>> DESBLOQUEANDO PUERTAS");
    
    // Desarmar alarma primero
    disarmAlarm();
    
    // Enviar pulso al rele de desbloqueo
    pulseRelay(PIN_LOCK_UNLOCK, LOCK_PULSE_DURATION_MS);
    
    _status.doorsLocked = false;
    _status.lastLockAction = millis();
    
    // Feedback sonoro
    beepUnlock();
    
    // Desplegar espejos
    #if AUTO_MIRRORS
    unfoldMirrors();
    #endif
    
    #if LOG_RELAY_ACTIONS
    PKE_LOG(TAG_ACT, "Puertas desbloqueadas OK");
    #endif
    
    return ACTUATOR_OK;
}

ActuatorResult Actuators::lockDoors() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    if (_status.doorsLocked) {
        PKE_LOG(TAG_ACT, "Puertas ya bloqueadas");
        return ACTUATOR_ERROR_ALREADY_LOCKED;
    }
    
    PKE_LOG(TAG_ACT, ">>> BLOQUEANDO PUERTAS");
    
    // Enviar pulso al rele de bloqueo
    pulseRelay(PIN_LOCK_LOCK, LOCK_PULSE_DURATION_MS);
    
    _status.doorsLocked = true;
    _status.lastLockAction = millis();
    
    // Feedback sonoro
    beepLock();
    
    // Cerrar ventanas automaticamente
    #if AUTO_WINDOW_CLOSE
    closeWindows();
    #endif
    
    // Plegar espejos
    #if AUTO_MIRRORS
    foldMirrors();
    #endif
    
    #if LOG_RELAY_ACTIONS
    PKE_LOG(TAG_ACT, "Puertas bloqueadas OK (alarma se armara en %d ms)", ALARM_ARM_DELAY_MS);
    #endif
    
    return ACTUATOR_OK;
}

bool Actuators::areDoorsLocked() {
    return _status.doorsLocked;
}

// =============================================================================
// Sistema de Encendido
// =============================================================================

ActuatorResult Actuators::turnAccOn() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    activateRelay(PIN_ACC);
    _status.accOn = true;
    
    PKE_LOG(TAG_ACT, "ACC: ON");
    return ACTUATOR_OK;
}

ActuatorResult Actuators::turnAccOff() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    deactivateRelay(PIN_ACC);
    _status.accOn = false;
    
    PKE_LOG(TAG_ACT, "ACC: OFF");
    return ACTUATOR_OK;
}

ActuatorResult Actuators::turnIgnitionOn() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    activateRelay(PIN_IGNITION);
    _status.ignitionOn = true;
    
    PKE_LOG(TAG_ACT, "IGNICION: ON");
    return ACTUATOR_OK;
}

ActuatorResult Actuators::turnIgnitionOff() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    deactivateRelay(PIN_IGNITION);
    _status.ignitionOn = false;
    
    PKE_LOG(TAG_ACT, "IGNICION: OFF");
    return ACTUATOR_OK;
}

ActuatorResult Actuators::startEngine() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    // Verificar seguridad
    ActuatorResult safety = checkStartSafety(true);  // Requiere freno
    if (safety != ACTUATOR_OK) return safety;
    
    if (_status.engineRunning) {
        PKE_LOG(TAG_ACT, "Motor ya esta encendido");
        return ACTUATOR_ERROR_ALREADY_RUNNING;
    }
    
    PKE_LOG(TAG_ACT, "=== SECUENCIA DE ARRANQUE INICIADA ===");
    
    // Paso 1: ACC
    turnAccOn();
    delay(IGNITION_SEQUENCE_DELAY);
    
    // Paso 2: Ignicion
    turnIgnitionOn();
    delay(IGNITION_SEQUENCE_DELAY);
    
    // Paso 3: Starter (con timeout de seguridad)
    PKE_LOG(TAG_ACT, "STARTER: ACTIVANDO (max %d ms)", STARTER_MAX_TIME_MS);
    activateRelay(PIN_STARTER);
    _status.starterActive = true;
    
    // Esperar con timeout
    uint32_t startTime = millis();
    while (millis() - startTime < STARTER_MAX_TIME_MS) {
        // Aqui podriamos detectar si el motor ya arranco
        // (por RPM via CAN, o por caida de corriente del starter)
        // Por ahora, activamos por un tiempo fijo corto
        delay(10);
    }
    
    // Desactivar starter
    deactivateRelay(PIN_STARTER);
    _status.starterActive = false;
    PKE_LOG(TAG_ACT, "STARTER: DESACTIVADO");
    
    // Marcar motor como encendido
    _status.engineRunning = true;
    _status.engineStartedAt = millis();
    
    PKE_LOG(TAG_ACT, "=== MOTOR ENCENDIDO ===");
    
    return ACTUATOR_OK;
}

ActuatorResult Actuators::stopEngine() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    PKE_LOG(TAG_ACT, "=== APAGANDO MOTOR ===");
    
    // Apagar en orden inverso
    deactivateRelay(PIN_STARTER);  // Por seguridad
    _status.starterActive = false;
    
    delay(100);
    
    turnIgnitionOff();
    delay(100);
    
    turnAccOff();
    
    _status.engineRunning = false;
    _status.remoteStartActive = false;
    
    PKE_LOG(TAG_ACT, "=== MOTOR APAGADO ===");
    
    return ACTUATOR_OK;
}

ActuatorResult Actuators::remoteStart() {
    if (!_initialized) return ACTUATOR_ERROR_NOT_INIT;
    
    #if !REMOTE_START_ENABLED
    PKE_LOG(TAG_ACT, "Arranque remoto deshabilitado en configuracion");
    return ACTUATOR_ERROR_NOT_ENABLED;
    #endif
    
    // Verificar seguridad para arranque remoto
    ActuatorResult safety = checkRemoteStartSafety();
    if (safety != ACTUATOR_OK) return safety;
    
    PKE_LOG(TAG_ACT, "=== ARRANQUE REMOTO INICIADO ===");
    
    // Secuencia de arranque (sin requerir freno)
    turnAccOn();
    delay(IGNITION_SEQUENCE_DELAY);
    
    turnIgnitionOn();
    delay(IGNITION_SEQUENCE_DELAY);
    
    // Starter
    activateRelay(PIN_STARTER);
    _status.starterActive = true;
    delay(STARTER_MAX_TIME_MS);
    deactivateRelay(PIN_STARTER);
    _status.starterActive = false;
    
    _status.engineRunning = true;
    _status.remoteStartActive = true;
    _status.remoteStartedAt = millis();
    _status.engineStartedAt = millis();
    
    // Feedback
    beepRemoteStart();
    
    PKE_LOG(TAG_ACT, "Arranque remoto activo (timeout: %d min)", REMOTE_START_MAX_TIME / 60000);
    
    return ACTUATOR_OK;
}

ActuatorResult Actuators::remoteStop() {
    if (!_status.remoteStartActive) return ACTUATOR_OK;
    
    PKE_LOG(TAG_ACT, "Deteniendo arranque remoto...");
    return stopEngine();
}

bool Actuators::isEngineRunning() {
    return _status.engineRunning;
}

bool Actuators::isRemoteStartActive() {
    return _status.remoteStartActive;
}

// =============================================================================
// Verificaciones de Seguridad
// =============================================================================

ActuatorResult Actuators::checkStartSafety(bool requireBrake) {
    if (requireBrake && !_status.brakePedal) {
        PKE_LOG(TAG_ACT, "SEGURIDAD: Freno no pisado - arranque rechazado");
        beepError();
        return ACTUATOR_ERROR_BRAKE_NOT_PRESSED;
    }
    
    if (_status.hoodOpen) {
        PKE_LOG(TAG_ACT, "SEGURIDAD: Capo abierto - arranque rechazado");
        beepError();
        return ACTUATOR_ERROR_HOOD_OPEN;
    }
    
    return ACTUATOR_OK;
}

ActuatorResult Actuators::checkRemoteStartSafety() {
    #if REMOTE_START_SAFETY
    if (_status.doorOpen) {
        PKE_LOG(TAG_ACT, "SEGURIDAD: Puerta abierta - arranque remoto rechazado");
        return ACTUATOR_ERROR_DOOR_OPEN;
    }
    
    if (_status.hoodOpen) {
        PKE_LOG(TAG_ACT, "SEGURIDAD: Capo abierto - arranque remoto rechazado");
        return ACTUATOR_ERROR_HOOD_OPEN;
    }
    
    if (_status.engineRunning) {
        PKE_LOG(TAG_ACT, "SEGURIDAD: Motor ya encendido");
        return ACTUATOR_ERROR_ALREADY_RUNNING;
    }
    #endif
    
    return ACTUATOR_OK;
}

// =============================================================================
// Boton Push-to-Start
// =============================================================================

void Actuators::enableStartButton() {
    if (!_status.startButtonEnabled) {
        digitalWrite(PIN_BUTTON_LED, HIGH);
        _status.startButtonEnabled = true;
        PKE_LOG(TAG_ACT, "Boton START: HABILITADO (LED encendido)");
    }
}

void Actuators::disableStartButton() {
    if (_status.startButtonEnabled) {
        digitalWrite(PIN_BUTTON_LED, LOW);
        _status.startButtonEnabled = false;
        PKE_LOG(TAG_ACT, "Boton START: DESHABILITADO (LED apagado)");
    }
}

bool Actuators::isStartButtonPressed() {
    return _status.startButtonPressed;
}

bool Actuators::isStartButtonEnabled() {
    return _status.startButtonEnabled;
}

// =============================================================================
// Sensores
// =============================================================================

bool Actuators::isBrakePressed() {
    return _status.brakePedal;
}

bool Actuators::isDoorOpen() {
    return _status.doorOpen;
}

bool Actuators::isHoodOpen() {
    return _status.hoodOpen;
}

// =============================================================================
// Alarma
// =============================================================================

void Actuators::armAlarm() {
    #if ALARM_ENABLED
    _status.alarmArmed = true;
    _status.alarmArmedAt = millis();
    PKE_LOG(TAG_ACT, "Alarma: ARMADA");
    #endif
}

void Actuators::disarmAlarm() {
    _status.alarmArmed = false;
    _status.alarmTriggered = false;
    
    // Detener buzzer si estaba sonando
    noTone(PIN_BUZZER);
    
    PKE_LOG(TAG_ACT, "Alarma: DESARMADA");
}

void Actuators::triggerAlarm() {
    #if ALARM_ENABLED
    if (!_status.alarmArmed) return;
    
    _status.alarmTriggered = true;
    _status.alarmTriggeredAt = millis();
    
    PKE_LOG(TAG_ACT, "!!! ALARMA DISPARADA !!!");
    
    // Iniciar patron de alarma (se maneja en updateBuzzer/updateTimers)
    beepAlarm();
    #endif
}

void Actuators::silenceAlarm() {
    _status.alarmTriggered = false;
    noTone(PIN_BUZZER);
    _buzzerActive = false;
    PKE_LOG(TAG_ACT, "Alarma: SILENCIADA");
}

bool Actuators::isAlarmArmed() {
    return _status.alarmArmed;
}

bool Actuators::isAlarmTriggered() {
    return _status.alarmTriggered;
}

// =============================================================================
// Buzzer (Non-blocking)
// =============================================================================

void Actuators::playBuzzerPattern(const BuzzerPattern& pattern) {
    _activeBuzzerPattern = pattern;
    _buzzerBeepIndex = 0;
    _buzzerLastToggle = millis();
    _buzzerActive = true;
    _buzzerToneOn = true;
    
    // Iniciar primer tono
    tone(PIN_BUZZER, pattern.frequency);
}

void Actuators::updateBuzzer() {
    if (!_buzzerActive) return;
    
    uint32_t now = millis();
    uint32_t elapsed = now - _buzzerLastToggle;
    
    if (_buzzerToneOn) {
        // Tono activo - verificar si debe apagarse
        if (elapsed >= _activeBuzzerPattern.beepDuration) {
            noTone(PIN_BUZZER);
            _buzzerToneOn = false;
            _buzzerLastToggle = now;
            _buzzerBeepIndex++;
            
            // Verificar si terminamos todos los pitidos
            if (_buzzerBeepIndex >= _activeBuzzerPattern.beepCount) {
                _buzzerActive = false;
            }
        }
    } else {
        // Pausa entre pitidos
        if (elapsed >= _activeBuzzerPattern.pauseDuration) {
            tone(PIN_BUZZER, _activeBuzzerPattern.frequency);
            _buzzerToneOn = true;
            _buzzerLastToggle = now;
        }
    }
}

void Actuators::beepLock() {
    BuzzerPattern pattern = {
        BEEP_LOCK_COUNT,
        BEEP_DURATION_MS,
        100,
        BEEP_FREQUENCY_HZ
    };
    playBuzzerPattern(pattern);
}

void Actuators::beepUnlock() {
    BuzzerPattern pattern = {
        BEEP_UNLOCK_COUNT,
        BEEP_DURATION_MS,
        80,
        BEEP_FREQUENCY_HZ
    };
    playBuzzerPattern(pattern);
}

void Actuators::beepError() {
    BuzzerPattern pattern = {
        3,          // 3 pitidos rapidos
        50,         // Cortos
        50,         // Pausa corta
        1000        // Tono bajo (error)
    };
    playBuzzerPattern(pattern);
}

void Actuators::beepAlarm() {
    BuzzerPattern pattern = {
        20,         // Muchos pitidos
        500,        // Largos
        200,        // Pausa corta
        3500        // Tono alto (sirena)
    };
    playBuzzerPattern(pattern);
}

void Actuators::beepRemoteStart() {
    BuzzerPattern pattern = {
        2,          // 2 pitidos
        200,        // Medianos
        150,        // Pausa media
        2000        // Tono medio
    };
    playBuzzerPattern(pattern);
}

// =============================================================================
// Ventanas y Espejos
// =============================================================================

void Actuators::closeWindows() {
    #if AUTO_WINDOW_CLOSE
    PKE_LOG(TAG_ACT, "Cerrando ventanas automaticamente...");
    digitalWrite(PIN_WINDOW_CLOSE, HIGH);
    delay(WINDOW_CLOSE_TIME_MS);
    digitalWrite(PIN_WINDOW_CLOSE, LOW);
    PKE_LOG(TAG_ACT, "Ventanas cerradas");
    #endif
}

void Actuators::unfoldMirrors() {
    #if AUTO_MIRRORS
    PKE_LOG(TAG_ACT, "Desplegando espejos...");
    digitalWrite(PIN_MIRRORS_UNFOLD, HIGH);
    delay(MIRROR_ACTUATOR_TIME_MS);
    digitalWrite(PIN_MIRRORS_UNFOLD, LOW);
    #endif
}

void Actuators::foldMirrors() {
    #if AUTO_MIRRORS
    PKE_LOG(TAG_ACT, "Plegando espejos...");
    digitalWrite(PIN_MIRRORS_FOLD, HIGH);
    delay(MIRROR_ACTUATOR_TIME_MS);
    digitalWrite(PIN_MIRRORS_FOLD, LOW);
    #endif
}

// =============================================================================
// Control de Reles
// =============================================================================

void Actuators::activateRelay(gpio_num_t pin) {
    digitalWrite(pin, RELAY_ACTIVE);
    
    #if LOG_RELAY_ACTIONS
    PKE_LOG(TAG_ACT, "Rele GPIO%d: ACTIVADO", pin);
    #endif
}

void Actuators::deactivateRelay(gpio_num_t pin) {
    digitalWrite(pin, RELAY_INACTIVE);
    
    #if LOG_RELAY_ACTIONS
    PKE_LOG(TAG_ACT, "Rele GPIO%d: desactivado", pin);
    #endif
}

void Actuators::pulseRelay(gpio_num_t pin, uint16_t durationMs) {
    activateRelay(pin);
    delay(durationMs);
    deactivateRelay(pin);
    
    #if LOG_RELAY_ACTIONS
    PKE_LOG(TAG_ACT, "Rele GPIO%d: pulso de %d ms", pin, durationMs);
    #endif
}

// =============================================================================
// Estado y Diagnostico
// =============================================================================

ActuatorStatus Actuators::getStatus() {
    return _status;
}

const char* Actuators::getResultName(ActuatorResult result) {
    switch (result) {
        case ACTUATOR_OK:                       return "OK";
        case ACTUATOR_ERROR_NOT_INIT:           return "NO_INICIALIZADO";
        case ACTUATOR_ERROR_BRAKE_NOT_PRESSED:  return "FRENO_NO_PISADO";
        case ACTUATOR_ERROR_DOOR_OPEN:          return "PUERTA_ABIERTA";
        case ACTUATOR_ERROR_HOOD_OPEN:          return "CAPO_ABIERTO";
        case ACTUATOR_ERROR_ALREADY_RUNNING:    return "YA_ENCENDIDO";
        case ACTUATOR_ERROR_ALREADY_LOCKED:     return "YA_BLOQUEADO";
        case ACTUATOR_ERROR_ALREADY_UNLOCKED:   return "YA_DESBLOQUEADO";
        case ACTUATOR_ERROR_STARTER_TIMEOUT:    return "TIMEOUT_STARTER";
        case ACTUATOR_ERROR_NOT_ENABLED:        return "NO_HABILITADO";
        case ACTUATOR_ERROR_SAFETY_CHECK:       return "FALLO_SEGURIDAD";
        default:                                return "DESCONOCIDO";
    }
}
