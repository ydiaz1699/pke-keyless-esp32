/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo de Seguridad (Header)
 * =============================================================================
 * 
 * Implementa toda la criptografia y seguridad del sistema:
 * - Cifrado AES-128-CBC para comunicacion segura
 * - Challenge-Response para autenticacion mutua
 * - Rolling Codes para proteccion anti-replay
 * - Generacion de numeros aleatorios criptograficos
 * - Gestion de intentos fallidos y bloqueo temporal
 * 
 * Protocolo de Autenticacion:
 * ┌──────────┐                    ┌──────────┐
 * │  ESP32   │                    │  Flutter  │
 * │ (Server) │                    │ (Client)  │
 * └────┬─────┘                    └─────┬────┘
 *      │                                │
 *      │  1. Challenge (16 bytes random) │
 *      │───────────────────────────────>│
 *      │                                │
 *      │                    [Cifra challenge con AES-128]
 *      │                    [Concatena rolling code]
 *      │                                │
 *      │  2. Response (AES(challenge) + rolling_code)
 *      │<───────────────────────────────│
 *      │                                │
 *      │  [Descifra response]           │
 *      │  [Verifica challenge]          │
 *      │  [Valida rolling code]         │
 *      │                                │
 *      │  3. AUTH_SUCCESS / AUTH_FAIL    │
 *      │───────────────────────────────>│
 *      │                                │
 * 
 * =============================================================================
 */

#ifndef SECURITY_H
#define SECURITY_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// Estructura de Rolling Code
// =============================================================================

struct RollingCodeState {
    uint32_t counter;           // Contador actual
    uint32_t lastValidCounter;  // Ultimo contador valido recibido
    bool synchronized;          // Si esta sincronizado con el telefono
};

// =============================================================================
// Estructura de Estado de Seguridad
// =============================================================================

struct SecurityStatus {
    bool initialized;           // Modulo inicializado
    uint8_t authFailures;       // Intentos fallidos consecutivos
    bool lockedOut;             // Bloqueado por exceso de fallos
    uint32_t lockoutStart;      // Timestamp de inicio del bloqueo
    uint32_t lastAuthAttempt;   // Timestamp del ultimo intento
    RollingCodeState rollingCode; // Estado del rolling code
};

// =============================================================================
// Clase SecurityManager (metodos estaticos para uso global)
// =============================================================================

class SecurityManager {
public:
    /**
     * Inicializa el modulo de seguridad
     * - Carga rolling code desde NVS
     * - Inicializa el generador de numeros aleatorios
     * - Resetea contadores de fallos
     * @return true si la inicializacion fue exitosa
     */
    static bool init();

    // =========================================================================
    // Generacion de Challenge
    // =========================================================================

    /**
     * Genera un challenge aleatorio criptograficamente seguro
     * @param buffer Buffer donde se almacenara el challenge
     * @param length Tamano del challenge en bytes (tipicamente 16)
     */
    static void generateChallenge(uint8_t* buffer, size_t length);

    // =========================================================================
    // Verificacion de Respuesta
    // =========================================================================

    /**
     * Verifica la respuesta del telefono al challenge
     * 
     * El telefono debe enviar: AES_ENCRYPT(challenge || rolling_code_bytes)
     * 
     * Proceso de verificacion:
     * 1. Descifrar la respuesta con AES-128-CBC
     * 2. Verificar que los primeros 16 bytes coinciden con el challenge enviado
     * 3. Extraer y validar el rolling code (si esta habilitado)
     * 4. Actualizar contadores
     * 
     * @param challenge El challenge original que se envio
     * @param challengeLen Tamano del challenge
     * @param response La respuesta cifrada del telefono
     * @param responseLen Tamano de la respuesta
     * @return true si la respuesta es valida
     */
    static bool verifyResponse(const uint8_t* challenge, size_t challengeLen,
                               const uint8_t* response, size_t responseLen);

    // =========================================================================
    // Cifrado / Descifrado AES
    // =========================================================================

    /**
     * Cifra datos con AES-128-CBC
     * @param plaintext Datos en texto plano
     * @param plaintextLen Tamano de los datos
     * @param ciphertext Buffer de salida (debe ser >= plaintextLen + 16 para padding)
     * @param ciphertextLen Tamano real del cifrado (salida)
     * @param iv Vector de inicializacion (16 bytes) - si NULL, usa IV por defecto
     * @return true si el cifrado fue exitoso
     */
    static bool encrypt(const uint8_t* plaintext, size_t plaintextLen,
                        uint8_t* ciphertext, size_t* ciphertextLen,
                        const uint8_t* iv = nullptr);

    /**
     * Descifra datos con AES-128-CBC
     * @param ciphertext Datos cifrados
     * @param ciphertextLen Tamano de los datos cifrados
     * @param plaintext Buffer de salida para texto plano
     * @param plaintextLen Tamano real del texto plano (salida)
     * @param iv Vector de inicializacion (16 bytes) - si NULL, usa IV por defecto
     * @return true si el descifrado fue exitoso
     */
    static bool decrypt(const uint8_t* ciphertext, size_t ciphertextLen,
                        uint8_t* plaintext, size_t* plaintextLen,
                        const uint8_t* iv = nullptr);

    // =========================================================================
    // Rolling Codes
    // =========================================================================

    /**
     * Valida un rolling code recibido
     * Acepta codigos dentro de una ventana de ROLLING_CODE_WINDOW posiciones
     * @param receivedCode El codigo recibido del telefono
     * @return true si el codigo es valido y esta dentro de la ventana
     */
    static bool validateRollingCode(uint32_t receivedCode);

    /**
     * Obtiene el proximo rolling code esperado
     * @return El siguiente codigo valido
     */
    static uint32_t getNextExpectedCode();

    /**
     * Sincroniza el rolling code con el telefono
     * @param newCounter El nuevo valor del contador
     * @return true si la sincronizacion fue exitosa
     */
    static bool syncRollingCode(uint32_t newCounter);

    /**
     * Resetea el rolling code (requiere re-emparejamiento)
     */
    static void resetRollingCode();

    // =========================================================================
    // Gestion de Intentos Fallidos
    // =========================================================================

    /**
     * Registra un intento de autenticacion fallido
     * Si se excede MAX_AUTH_FAILURES, activa el bloqueo temporal
     */
    static void registerFailure();

    /**
     * Registra una autenticacion exitosa
     * Resetea el contador de fallos
     */
    static void registerSuccess();

    /**
     * Verifica si el sistema esta bloqueado por fallos
     * @return true si esta en periodo de bloqueo
     */
    static bool isLockedOut();

    /**
     * Obtiene el tiempo restante de bloqueo (ms)
     * @return Milisegundos restantes (0 si no esta bloqueado)
     */
    static uint32_t getLockoutRemaining();

    /**
     * Resetea manualmente el bloqueo (uso de emergencia)
     */
    static void resetLockout();

    // =========================================================================
    // Estado
    // =========================================================================

    /**
     * Obtiene el estado completo de seguridad
     * @return Estructura con todos los indicadores de seguridad
     */
    static SecurityStatus getStatus();

    /**
     * Obtiene el numero de intentos fallidos
     * @return Numero de fallos consecutivos
     */
    static uint8_t getFailureCount();

private:
    // Estado interno (estatico)
    static SecurityStatus _status;
    static uint8_t _aesKey[16];
    static uint8_t _aesIv[16];

    // --- Metodos privados ---

    /** Aplica padding PKCS7 al buffer */
    static size_t applyPkcs7Padding(uint8_t* data, size_t dataLen, size_t blockSize);

    /** Remueve padding PKCS7 del buffer */
    static size_t removePkcs7Padding(uint8_t* data, size_t dataLen, size_t blockSize);

    /** Carga el rolling code desde NVS */
    static void loadRollingCode();

    /** Guarda el rolling code en NVS */
    static void saveRollingCode();

    /** Carga contador de fallos desde NVS */
    static void loadFailureCount();

    /** Guarda contador de fallos en NVS */
    static void saveFailureCount();

    /** Compara dos buffers de forma segura (tiempo constante) */
    static bool secureCompare(const uint8_t* a, const uint8_t* b, size_t length);
};

#endif // SECURITY_H
