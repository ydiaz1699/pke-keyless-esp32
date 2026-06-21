/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo de Actuadores (Header)
 * =============================================================================
 * 
 * Controla todos los componentes fisicos del vehiculo:
 * - Reles de cerraduras (bloquear/desbloquear)
 * - Secuencia de encendido (ACC -> IGN -> START)
 * - Boton push-to-start (lectura + LED indicador)
 * - Buzzer (feedback sonoro)
 * - LED de estado
 * - Sensores de entrada (freno, puerta, capo)
 * - Ventanas automaticas (opcional)
 * - Espejos retrovisores (opcional)
 * 
 * Seguridades implementadas:
 * - Tiempo maximo de starter para proteger el motor de arranque
 * - Verificacion de freno pisado antes de arrancar
 * - Verificacion de puerta cerrada para arranque remoto
 * - Anti-repeticion: no activar reles si ya estan en el estado deseado
 * 
 * =============================================================================
 */

#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Estado de los Actuadores
// =============================================================================

struct ActuatorStatus {
    // Cerraduras
    bool doorsLocked;           // Estado actual de cerraduras
    uint32_t lastLockAction;    // Timestamp de ultima accion de cerradura
    
    // Sistema de encendido
    bool accOn;                 // Accesorios encendidos
    bool ignitionOn;            // Ignicion encendida
    bool starterActive;         // Motor de arranque activo
    bool engineRunning;         // Motor funcionando (estimado)
    uint32_t engineStartedAt;   // Timestamp de cuando arranco el motor
    
    // Boton
    bool startButtonPressed;    // Boton fisico presionado
    bool startButtonEnabled;    // Boton habilitado (LED encendido)
    
    // Sensores
    bool brakePedal;            // Freno pisado
    bool doorOpen;              // Alguna puerta abierta
    bool hoodOpen;              // Capo abierto
    
    // Alarma
    bool alarmArmed;            // Alarma armada
    bool alarmTriggered;        // Alarma sonando
    uint32_t alarmArmedAt;      // Timestamp de cuando se armo
    uint32_t alarmTriggeredAt;  // Timestamp de cuando se disparo
    
    // Arranque remoto
    bool remoteStartActive;     // Arranque remoto activo
    uint32_t remoteStartedAt;   // Timestamp del arranque remoto
};

// =============================================================================
// Resultado de operaciones
// =============================================================================

enum ActuatorResult {
    ACTUATOR_OK = 0,
    ACTUATOR_ERROR_NOT_INIT,
    ACTUATOR_ERROR_BRAKE_NOT_PRESSED,
    ACTUATOR_ERROR_DOOR_OPEN,
    ACTUATOR_ERROR_HOOD_OPEN,
    ACTUATOR_ERROR_ALREADY_RUNNING,
    ACTUATOR_ERROR_ALREADY_LOCKED,
    ACTUATOR_ERROR_ALREADY_UNLOCKED,
    ACTUATOR_ERROR_STARTER_TIMEOUT,
    ACTUATOR_ERROR_NOT_ENABLED,
    ACTUATOR_ERROR_SAFETY_CHECK
};

// =============================================================================
// Patrones de buzzer
// =============================================================================

struct BuzzerPattern {
    uint8_t beepCount;      // Numero de pitidos
    uint16_t beepDuration;  // Duracion de cada pitido (ms)
    uint16_t pauseDuration; // Pausa entre pitidos (ms)
    uint16_t frequency;     // Frecuencia (Hz)
};

// =============================================================================
// Clase Actuators
// =============================================================================

class Actuators {
public:
    Actuators();

    /**
     * Inicializa todos los pines de actuadores y sensores
     * @return true si la inicializacion fue exitosa
     */
    bool init();

    /**
     * Actualiza el estado de sensores y temporizadores
     * Llamar en cada iteracion del loop
     */
    void update();

    // =========================================================================
    // Cerraduras
    // =========================================================================

    /**
     * Desbloquea las puertas del vehiculo
     * @return ACTUATOR_OK si fue exitoso
     */
    ActuatorResult unlockDoors();

    /**
     * Bloquea las puertas del vehiculo
     * @return ACTUATOR_OK si fue exitoso
     */
    ActuatorResult lockDoors();

    /** @return true si las puertas estan bloqueadas */
    bool areDoorsLocked();

    // =========================================================================
    // Sistema de Encendido (Keyless Start)
    // =========================================================================

    /**
     * Enciende los accesorios (ACC) - primera posicion de llave
     * @return ACTUATOR_OK si fue exitoso
     */
    ActuatorResult turnAccOn();

    /**
     * Apaga los accesorios
     * @return ACTUATOR_OK si fue exitoso
     */
    ActuatorResult turnAccOff();

    /**
     * Enciende la ignicion (ON) - segunda posicion de llave
     * @return ACTUATOR_OK si fue exitoso
     */
    ActuatorResult turnIgnitionOn();

    /**
     * Apaga la ignicion
     * @return ACTUATOR_OK si fue exitoso
     */
    ActuatorResult turnIgnitionOff();

    /**
     * Ejecuta la secuencia completa de arranque:
     * ACC -> delay -> IGN -> delay -> STARTER (maximo STARTER_MAX_TIME_MS)
     * REQUIERE: freno pisado
     * @return ACTUATOR_OK si el arranque fue iniciado
     */
    ActuatorResult startEngine();

    /**
     * Apaga el motor: desactiva IGN y ACC
     * @return ACTUATOR_OK si fue exitoso
     */
    ActuatorResult stopEngine();

    /**
     * Ejecuta arranque remoto (desde la app)
     * REQUIERE: puertas cerradas, capo cerrado
     * @return ACTUATOR_OK si el arranque remoto fue iniciado
     */
    ActuatorResult remoteStart();

    /**
     * Detiene el arranque remoto
     * @return ACTUATOR_OK si fue exitoso
     */
    ActuatorResult remoteStop();

    /** @return true si el motor esta funcionando */
    bool isEngineRunning();

    /** @return true si el arranque remoto esta activo */
    bool isRemoteStartActive();

    // =========================================================================
    // Boton Push-to-Start
    // =========================================================================

    /**
     * Habilita el boton de encendido (enciende LED del boton)
     */
    void enableStartButton();

    /**
     * Deshabilita el boton de encendido (apaga LED del boton)
     */
    void disableStartButton();

    /** @return true si el boton fisico esta siendo presionado */
    bool isStartButtonPressed();

    /** @return true si el boton esta habilitado */
    bool isStartButtonEnabled();

    // =========================================================================
    // Sensores
    // =========================================================================

    /** @return true si el pedal de freno esta pisado */
    bool isBrakePressed();

    /** @return true si alguna puerta esta abierta */
    bool isDoorOpen();

    /** @return true si el capo esta abierto */
    bool isHoodOpen();

    // =========================================================================
    // Alarma
    // =========================================================================

    /**
     * Arma la alarma antirrobo (con delay configurable)
     */
    void armAlarm();

    /**
     * Desarma la alarma
     */
    void disarmAlarm();

    /**
     * Dispara la alarma (sirena)
     */
    void triggerAlarm();

    /**
     * Detiene la sirena de alarma
     */
    void silenceAlarm();

    /** @return true si la alarma esta armada */
    bool isAlarmArmed();

    /** @return true si la alarma esta sonando */
    bool isAlarmTriggered();

    // =========================================================================
    // Buzzer (Feedback Sonoro)
    // =========================================================================

    /**
     * Emite un patron de pitidos
     * @param pattern Estructura con el patron a reproducir
     */
    void playBuzzerPattern(const BuzzerPattern& pattern);

    /** Pitido corto de confirmacion (bloqueo) */
    void beepLock();

    /** Pitidos de confirmacion (desbloqueo) */
    void beepUnlock();

    /** Pitido de error */
    void beepError();

    /** Pitido de panico/alarma */
    void beepAlarm();

    /** Pitido de arranque remoto confirmado */
    void beepRemoteStart();

    // =========================================================================
    // Ventanas y Espejos (opcionales)
    // =========================================================================

    /** Envia senal para cerrar ventanas automaticamente */
    void closeWindows();

    /** Despliega espejos retrovisores */
    void unfoldMirrors();

    /** Pliega espejos retrovisores */
    void foldMirrors();

    // =========================================================================
    // Estado y Diagnostico
    // =========================================================================

    /** @return Estructura completa de estado de actuadores */
    ActuatorStatus getStatus();

    /** @return String descriptivo del error */
    const char* getResultName(ActuatorResult result);

private:
    ActuatorStatus _status;
    bool _initialized;
    
    // Temporizador de buzzer (no bloqueante)
    BuzzerPattern _activeBuzzerPattern;
    uint8_t _buzzerBeepIndex;
    uint32_t _buzzerLastToggle;
    bool _buzzerActive;
    bool _buzzerToneOn;

    // --- Metodos privados ---

    /** Configura los pines GPIO */
    void setupPins();

    /** Lee el estado actual de todos los sensores */
    void readSensors();

    /** Actualiza temporizadores (alarma, arranque remoto, buzzer) */
    void updateTimers();

    /** Actualiza el patron de buzzer (non-blocking) */
    void updateBuzzer();

    /** Activa un rele con proteccion */
    void activateRelay(gpio_num_t pin);

    /** Desactiva un rele */
    void deactivateRelay(gpio_num_t pin);

    /** Envia pulso a un rele (para cerraduras centralizadas) */
    void pulseRelay(gpio_num_t pin, uint16_t durationMs);

    /** Verifica condiciones de seguridad para arranque */
    ActuatorResult checkStartSafety(bool requireBrake);

    /** Verifica condiciones para arranque remoto */
    ActuatorResult checkRemoteStartSafety();
};

#endif // ACTUATORS_H
