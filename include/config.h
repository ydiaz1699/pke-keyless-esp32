/**
 * =============================================================================
 * PKE + Keyless Start System - Configuracion Principal
 * =============================================================================
 * 
 * Este archivo contiene TODA la configuracion del sistema.
 * Modifica estos valores segun tu vehiculo e instalacion.
 * 
 * Autor: PKE-ESP32 Project
 * Licencia: MIT
 * =============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// MODO DE INTERFAZ DEL VEHICULO
// =============================================================================
// Descomenta UNO SOLO segun tu tipo de vehiculo:

#define VEHICLE_MODE_CENTRALIZED    // Cierre centralizado (pulso positivo/negativo)
// #define VEHICLE_MODE_DIRECT_WIRE   // Cable directo (actuadores de motor)
// #define VEHICLE_MODE_CAN_BUS       // Red CAN Bus (vehiculos modernos 2008+)

// =============================================================================
// CONFIGURACION BLE
// =============================================================================

// Nombre del dispositivo BLE (aparecera en el escaneo)
#define BLE_DEVICE_NAME         "PKE-AutoKey"

// UUID del servicio principal GATT
#define SERVICE_UUID            "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// UUIDs de las caracteristicas GATT
#define CHAR_AUTH_UUID          "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Autenticacion challenge-response
#define CHAR_COMMAND_UUID       "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"  // Comandos del telefono al ESP32
#define CHAR_STATUS_UUID        "a3c87500-8ed3-4bdf-8a39-a01bebede295"  // Estado del sistema (notify)
#define CHAR_RSSI_UUID          "d1e8de50-f048-4b37-8b55-3a6b2157a2c2"  // Nivel RSSI actual (notify)

// Intervalo de advertising BLE (ms)
// Menor = deteccion mas rapida, Mayor = menor consumo
#define BLE_ADV_INTERVAL_MS     100

// Timeout de conexion BLE (ms) - si el telefono no responde en este tiempo, se desconecta
#define BLE_CONNECTION_TIMEOUT  5000

// Intervalo de lectura RSSI (ms)
#define RSSI_READ_INTERVAL_MS   200

// =============================================================================
// ZONAS DE PROXIMIDAD (basadas en RSSI)
// =============================================================================
// RSSI es negativo: -30 = muy cerca, -90 = muy lejos
// Ajusta estos valores segun tu vehiculo (prueba y error)

// Zona 1 - DENTRO DEL VEHICULO: Habilita boton START
// RSSI mayor (menos negativo) que este umbral = dentro
#define RSSI_ZONE_INSIDE        -50   // dBm (telefono dentro de la cabina)

// Zona 2 - CERCA DEL VEHICULO: Desbloqueo automatico PKE
// RSSI entre ZONE_NEAR y ZONE_INSIDE = cerca (1.5-2 metros)
#define RSSI_ZONE_NEAR          -70   // dBm (telefono a ~1.5-2 metros)

// Zona 3 - LEJOS DEL VEHICULO: Bloqueo automatico + alarma
// RSSI menor (mas negativo) que este umbral = lejos
#define RSSI_ZONE_FAR           -80   // dBm (telefono a mas de ~3 metros)

// Histeresis para evitar oscilaciones entre zonas (dBm)
#define RSSI_HYSTERESIS         5

// Numero de muestras RSSI para promediar (filtro de ruido)
#define RSSI_SAMPLE_COUNT       8

// Tiempo minimo en una zona antes de ejecutar accion (ms)
// Evita activaciones accidentales al pasar caminando
#define ZONE_DWELL_TIME_MS      1500

// Tiempo de espera al alejarse antes de bloquear (ms)
// Da tiempo a que regreses si olvidaste algo
#define LOCK_DELAY_MS           3000

// =============================================================================
// PINES GPIO - ACTUADORES
// =============================================================================

// --- Cerraduras ---
#define PIN_LOCK_UNLOCK         GPIO_NUM_25   // Rele: Desbloquear puertas
#define PIN_LOCK_LOCK           GPIO_NUM_26   // Rele: Bloquear puertas

// --- Sistema de Encendido ---
#define PIN_ACC                  GPIO_NUM_27   // Rele: Accesorios (ACC)
#define PIN_IGNITION            GPIO_NUM_14   // Rele: Ignicion (ON)
#define PIN_STARTER             GPIO_NUM_12   // Rele: Motor de arranque (START)

// --- Boton Push-to-Start ---
#define PIN_START_BUTTON        GPIO_NUM_33   // Entrada: Boton fisico de encendido
#define PIN_BUTTON_LED          GPIO_NUM_32   // Salida: LED del boton (indica disponibilidad)

// --- Sensores ---
#define PIN_BRAKE_PEDAL         GPIO_NUM_34   // Entrada: Sensor de freno pisado
#define PIN_DOOR_SENSOR         GPIO_NUM_35   // Entrada: Sensor de puerta abierta/cerrada
#define PIN_HOOD_SENSOR         GPIO_NUM_36   // Entrada: Sensor de capo (seguridad)

// --- Feedback ---
#define PIN_BUZZER              GPIO_NUM_13   // Salida: Buzzer piezoelectrico
#define PIN_LED_STATUS          GPIO_NUM_2    // Salida: LED de estado (LED integrado ESP32)

// --- CAN Bus (solo si VEHICLE_MODE_CAN_BUS esta activo) ---
#define PIN_CAN_TX              GPIO_NUM_21   // CAN Transceiver TX
#define PIN_CAN_RX              GPIO_NUM_22   // CAN Transceiver RX

// =============================================================================
// CONFIGURACION DE RELES
// =============================================================================

// Tipo de activacion de reles (HIGH o LOW segun tu modulo)
#define RELAY_ACTIVE            LOW    // LOW = reles activos en bajo (mas comun)
#define RELAY_INACTIVE          HIGH

// Duracion del pulso para cerraduras centralizadas (ms)
// Algunos autos necesitan 200ms, otros 500ms
#define LOCK_PULSE_DURATION_MS  300

// Tiempo maximo que el motor de arranque puede girar (ms)
// SEGURIDAD: evita quemar el starter si algo falla
#define STARTER_MAX_TIME_MS     3000

// Delay entre ACC -> IGN -> START (ms)
// Simula la secuencia normal de giro de llave
#define IGNITION_SEQUENCE_DELAY 500

// =============================================================================
// SEGURIDAD Y CRIPTOGRAFIA
// =============================================================================

// Clave AES-128 pre-compartida (16 bytes)
// IMPORTANTE: Cambia esta clave antes de usar el sistema!
// Debe ser la misma en el ESP32 y en la app Flutter
#define AES_KEY                 { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, \
                                  0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C }

// Vector de inicializacion AES (16 bytes)
// Se regenera en cada sesion para mayor seguridad
#define AES_IV_DEFAULT          { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, \
                                  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F }

// Tamano del challenge en bytes
#define CHALLENGE_SIZE          16

// Tiempo maximo para responder un challenge (ms)
// Si el telefono no responde a tiempo, se invalida
#define AUTH_TIMEOUT_MS         3000

// Numero maximo de intentos fallidos antes de bloqueo temporal
#define MAX_AUTH_FAILURES       5

// Tiempo de bloqueo tras exceder intentos fallidos (ms)
#define AUTH_LOCKOUT_TIME_MS    60000  // 1 minuto

// Habilitar rolling codes (recomendado)
#define ENABLE_ROLLING_CODES    true

// Ventana de aceptacion de rolling codes
// Acepta codigos hasta N posiciones adelante (por si se pierde sincronizacion)
#define ROLLING_CODE_WINDOW     10

// =============================================================================
// CONFIGURACION DE ALARMA
// =============================================================================

// Habilitar alarma antirrobo
#define ALARM_ENABLED           true

// Patron de pitidos al bloquear (confirmacion)
#define BEEP_LOCK_COUNT         1     // 1 pitido corto al bloquear
#define BEEP_UNLOCK_COUNT       2     // 2 pitidos al desbloquear
#define BEEP_DURATION_MS        100   // Duracion de cada pitido (ms)
#define BEEP_FREQUENCY_HZ       2700  // Frecuencia del buzzer (Hz)

// Tiempo de armado de alarma tras bloquear (ms)
#define ALARM_ARM_DELAY_MS      5000

// Duracion de la sirena de alarma (ms)
#define ALARM_SIREN_TIME_MS     30000  // 30 segundos

// =============================================================================
// ARRANQUE REMOTO
// =============================================================================

// Habilitar arranque remoto del motor desde la app
#define REMOTE_START_ENABLED    true

// Tiempo maximo de funcionamiento en arranque remoto (ms)
// SEGURIDAD: el motor se apaga solo si nadie se sube
#define REMOTE_START_MAX_TIME   600000  // 10 minutos

// El arranque remoto requiere que:
// - Todas las puertas esten cerradas
// - El capo este cerrado  
// - El freno de mano este puesto (si hay sensor)
#define REMOTE_START_SAFETY     true

// =============================================================================
// CIERRE AUTOMATICO DE VENTANAS (si el vehiculo lo soporta)
// =============================================================================

// Habilitar subida automatica de ventanas al bloquear
#define AUTO_WINDOW_CLOSE       false  // Cambiar a true si tu auto lo soporta

// Pin de salida para la señal de subir ventanas
#define PIN_WINDOW_CLOSE        GPIO_NUM_4

// Duracion de la señal de cierre de ventanas (ms)
// Tiempo suficiente para que suban completamente
#define WINDOW_CLOSE_TIME_MS    5000

// =============================================================================
// ESPEJOS RETROVISORES (si el vehiculo lo soporta)
// =============================================================================

// Habilitar despliegue/plegado automatico de espejos
#define AUTO_MIRRORS            false  // Cambiar a true si tu auto lo soporta

#define PIN_MIRRORS_FOLD        GPIO_NUM_16   // Plegar espejos
#define PIN_MIRRORS_UNFOLD      GPIO_NUM_17   // Desplegar espejos
#define MIRROR_ACTUATOR_TIME_MS 2000          // Tiempo de actuacion

// =============================================================================
// CAN BUS (solo para VEHICLE_MODE_CAN_BUS)
// =============================================================================

#ifdef VEHICLE_MODE_CAN_BUS

// Velocidad del bus CAN (kbps)
// 500kbps = la mayoria de vehiculos modernos
// 250kbps = algunos vehiculos mas antiguos
#define CAN_SPEED               500E3

// IDs CAN comunes (varian segun fabricante - consultar base de datos OBD)
// Estos son ejemplos genericos, DEBES adaptarlos a tu vehiculo
#define CAN_ID_DOOR_LOCK        0x290   // Comando de bloqueo
#define CAN_ID_DOOR_UNLOCK      0x291   // Comando de desbloqueo
#define CAN_ID_ENGINE_START     0x350   // Comando de arranque
#define CAN_ID_DOOR_STATUS      0x220   // Estado de puertas (lectura)
#define CAN_ID_ENGINE_STATUS    0x360   // Estado del motor (lectura)
#define CAN_ID_LIGHTS           0x1A0   // Control de luces (parpadeo confirmacion)

#endif // VEHICLE_MODE_CAN_BUS

// =============================================================================
// PERSISTENCIA (NVS - Non-Volatile Storage)
// =============================================================================

// Namespace para almacenar datos en la flash del ESP32
#define NVS_NAMESPACE           "pke_system"

// Claves NVS
#define NVS_KEY_ROLLING_CODE    "roll_code"    // Ultimo rolling code sincronizado
#define NVS_KEY_PAIRED_DEVICE   "paired_dev"   // MAC address del telefono emparejado
#define NVS_KEY_SYSTEM_STATE    "sys_state"    // Estado del sistema antes de perder energia
#define NVS_KEY_AUTH_FAILURES   "auth_fail"    // Contador de fallos de autenticacion

// =============================================================================
// DEBUG Y LOGGING
// =============================================================================

#ifdef DEBUG_MODE
    #define LOG_LEVEL           ESP_LOG_VERBOSE
    #define SERIAL_DEBUG        true
    #define LOG_BLE_EVENTS      true
    #define LOG_RSSI_VALUES     true
    #define LOG_ZONE_CHANGES    true
    #define LOG_AUTH_EVENTS     true
    #define LOG_RELAY_ACTIONS   true
#else
    #define LOG_LEVEL           ESP_LOG_WARN
    #define SERIAL_DEBUG        true   // Mantener serial para diagnostico basico
    #define LOG_BLE_EVENTS      false
    #define LOG_RSSI_VALUES     false
    #define LOG_ZONE_CHANGES    true
    #define LOG_AUTH_EVENTS     true
    #define LOG_RELAY_ACTIONS   true
#endif

// Macro de logging condicional
#define PKE_LOG(tag, fmt, ...) \
    if (SERIAL_DEBUG) { Serial.printf("[%s] " fmt "\n", tag, ##__VA_ARGS__); }

// =============================================================================
// ESTADOS DEL SISTEMA
// =============================================================================

enum SystemState {
    STATE_IDLE = 0,             // Sistema en espera (nadie cerca)
    STATE_PHONE_DETECTED,       // Telefono detectado, verificando autenticacion
    STATE_AUTHENTICATED,        // Telefono autenticado exitosamente
    STATE_UNLOCKED,             // Puertas desbloqueadas (Zona 2 - cerca)
    STATE_READY_TO_START,       // Dentro del auto, boton habilitado (Zona 1)
    STATE_ENGINE_ACC,           // Accesorios encendidos
    STATE_ENGINE_ON,            // Motor encendido
    STATE_ENGINE_STARTING,      // Motor arrancando (starter activo)
    STATE_REMOTE_START,         // Arranque remoto activo
    STATE_LOCKING,              // Proceso de bloqueo (alejandose)
    STATE_LOCKED,               // Auto bloqueado y alarma armada
    STATE_ALARM_TRIGGERED,      // Alarma sonando
    STATE_AUTH_LOCKED_OUT,      // Bloqueado por intentos fallidos
    STATE_ERROR                 // Error del sistema
};

// =============================================================================
// COMANDOS BLE (telefono -> ESP32)
// =============================================================================

enum BleCommand {
    CMD_NONE = 0x00,
    CMD_LOCK = 0x01,            // Bloquear puertas manualmente
    CMD_UNLOCK = 0x02,          // Desbloquear puertas manualmente
    CMD_START_ENGINE = 0x03,    // Arranque remoto
    CMD_STOP_ENGINE = 0x04,     // Apagar motor (arranque remoto)
    CMD_TRUNK_OPEN = 0x05,      // Abrir maletero/cajuela
    CMD_PANIC = 0x06,           // Boton de panico (alarma manual)
    CMD_FIND_CAR = 0x07,        // Parpadear luces (encontrar auto)
    CMD_STATUS_REQ = 0x08,      // Solicitar estado actual
    CMD_WINDOW_CLOSE = 0x09,    // Cerrar ventanas
    CMD_PAIR_REQUEST = 0x10,    // Solicitud de emparejamiento
    CMD_UNPAIR = 0x11,          // Desemparejar dispositivo
};

// =============================================================================
// NOTIFICACIONES BLE (ESP32 -> telefono)
// =============================================================================

enum BleNotification {
    NOTIFY_STATE_CHANGE = 0x01,  // Cambio de estado del sistema
    NOTIFY_ZONE_CHANGE = 0x02,   // Cambio de zona de proximidad
    NOTIFY_AUTH_SUCCESS = 0x03,  // Autenticacion exitosa
    NOTIFY_AUTH_FAIL = 0x04,     // Autenticacion fallida
    NOTIFY_ALARM = 0x05,         // Alarma disparada
    NOTIFY_LOW_BATTERY = 0x06,   // Bateria baja del vehiculo
    NOTIFY_DOOR_OPEN = 0x07,     // Puerta abierta
    NOTIFY_ERROR = 0xFF,         // Error
};

#endif // CONFIG_H
