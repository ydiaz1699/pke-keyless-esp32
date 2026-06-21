/**
 * =============================================================================
 * PKE + Keyless Start System - Modulo de Seguridad (Implementacion)
 * =============================================================================
 */

#include "security.h"
#include <Preferences.h>
#include <Crypto.h>
#include <AES.h>
#include <CBC.h>
#include <esp_system.h>

// Tag para logging
static const char* TAG_SEC = "SEC";

// =============================================================================
// Variables estaticas
// =============================================================================

SecurityStatus SecurityManager::_status = {};
uint8_t SecurityManager::_aesKey[16] = AES_KEY;
uint8_t SecurityManager::_aesIv[16] = AES_IV_DEFAULT;

// =============================================================================
// Inicializacion
// =============================================================================

bool SecurityManager::init() {
    PKE_LOG(TAG_SEC, "Inicializando modulo de seguridad...");
    
    // Inicializar generador de numeros aleatorios del ESP32
    // El ESP32 tiene un TRNG (True Random Number Generator) por hardware
    // que se alimenta de ruido termico - ideal para criptografia
    
    // Cargar estado persistente
    loadRollingCode();
    loadFailureCount();
    
    // Verificar si hay bloqueo activo desde una sesion anterior
    if (_status.authFailures >= MAX_AUTH_FAILURES) {
        _status.lockedOut = true;
        _status.lockoutStart = millis();
        PKE_LOG(TAG_SEC, "ADVERTENCIA: Sistema bloqueado por fallos previos");
    }
    
    _status.initialized = true;
    
    PKE_LOG(TAG_SEC, "Seguridad inicializada - Rolling Code: %lu", _status.rollingCode.counter);
    PKE_LOG(TAG_SEC, "Fallos acumulados: %d/%d", _status.authFailures, MAX_AUTH_FAILURES);
    
    return true;
}

// =============================================================================
// Generacion de Challenge
// =============================================================================

void SecurityManager::generateChallenge(uint8_t* buffer, size_t length) {
    // Usar el TRNG del ESP32 para generar bytes aleatorios criptograficamente seguros
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (uint8_t)esp_random();
    }
    
    PKE_LOG(TAG_SEC, "Challenge generado (%d bytes)", length);
    
    #ifdef DEBUG_MODE
    Serial.print("[SEC] Challenge: ");
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X", buffer[i]);
    }
    Serial.println();
    #endif
}

// =============================================================================
// Verificacion de Respuesta
// =============================================================================

bool SecurityManager::verifyResponse(const uint8_t* challenge, size_t challengeLen,
                                      const uint8_t* response, size_t responseLen) {
    // Verificar bloqueo
    if (isLockedOut()) {
        PKE_LOG(TAG_SEC, "Sistema bloqueado - quedan %lu ms", getLockoutRemaining());
        return false;
    }
    
    // La respuesta esperada del telefono es:
    // AES_CBC_ENCRYPT(challenge[16] + rolling_code[4]) = 32 bytes cifrados
    // (16 bytes challenge + 4 bytes rolling code = 20 bytes -> con padding PKCS7 = 32 bytes)
    
    size_t expectedResponseLen = 32;  // 2 bloques AES (con padding)
    
    if (responseLen < expectedResponseLen) {
        PKE_LOG(TAG_SEC, "Respuesta muy corta: %d bytes (esperados: %d)", responseLen, expectedResponseLen);
        registerFailure();
        return false;
    }
    
    // Descifrar la respuesta
    uint8_t decrypted[32];
    size_t decryptedLen = 0;
    
    if (!decrypt(response, expectedResponseLen, decrypted, &decryptedLen)) {
        PKE_LOG(TAG_SEC, "Error al descifrar respuesta");
        registerFailure();
        return false;
    }
    
    // Verificar que los primeros bytes coinciden con el challenge (comparacion en tiempo constante)
    if (decryptedLen < challengeLen) {
        PKE_LOG(TAG_SEC, "Datos descifrados insuficientes");
        registerFailure();
        return false;
    }
    
    if (!secureCompare(decrypted, challenge, challengeLen)) {
        PKE_LOG(TAG_SEC, "Challenge no coincide en la respuesta");
        registerFailure();
        return false;
    }
    
    // Verificar rolling code (si esta habilitado)
    #if ENABLE_ROLLING_CODES
    if (decryptedLen >= challengeLen + 4) {
        // Extraer rolling code (4 bytes, little-endian)
        uint32_t receivedCode = 0;
        receivedCode |= (uint32_t)decrypted[challengeLen + 0];
        receivedCode |= (uint32_t)decrypted[challengeLen + 1] << 8;
        receivedCode |= (uint32_t)decrypted[challengeLen + 2] << 16;
        receivedCode |= (uint32_t)decrypted[challengeLen + 3] << 24;
        
        if (!validateRollingCode(receivedCode)) {
            PKE_LOG(TAG_SEC, "Rolling code invalido: %lu (esperado: ~%lu)", 
                    receivedCode, _status.rollingCode.counter);
            registerFailure();
            return false;
        }
        
        PKE_LOG(TAG_SEC, "Rolling code valido: %lu", receivedCode);
    } else {
        PKE_LOG(TAG_SEC, "Rolling code no incluido en la respuesta");
        registerFailure();
        return false;
    }
    #endif
    
    // Autenticacion exitosa
    registerSuccess();
    
    // Limpiar datos sensibles de memoria
    memset(decrypted, 0, sizeof(decrypted));
    
    return true;
}

// =============================================================================
// Cifrado AES-128-CBC
// =============================================================================

bool SecurityManager::encrypt(const uint8_t* plaintext, size_t plaintextLen,
                               uint8_t* ciphertext, size_t* ciphertextLen,
                               const uint8_t* iv) {
    if (plaintextLen == 0 || plaintextLen > 240) {  // Limitar tamano maximo
        return false;
    }
    
    const uint8_t* useIv = (iv != nullptr) ? iv : _aesIv;
    
    // Preparar buffer con padding PKCS7
    uint8_t padded[256];
    memcpy(padded, plaintext, plaintextLen);
    size_t paddedLen = applyPkcs7Padding(padded, plaintextLen, 16);
    
    // Cifrar con AES-128-CBC
    CBC<AES128> cbcAes;
    cbcAes.setKey(_aesKey, 16);
    cbcAes.setIV(useIv, 16);
    
    cbcAes.encrypt(ciphertext, padded, paddedLen);
    *ciphertextLen = paddedLen;
    
    // Limpiar buffer temporal
    memset(padded, 0, sizeof(padded));
    
    return true;
}

bool SecurityManager::decrypt(const uint8_t* ciphertext, size_t ciphertextLen,
                               uint8_t* plaintext, size_t* plaintextLen,
                               const uint8_t* iv) {
    if (ciphertextLen == 0 || ciphertextLen > 256 || (ciphertextLen % 16) != 0) {
        PKE_LOG(TAG_SEC, "Tamano de cifrado invalido: %d", ciphertextLen);
        return false;
    }
    
    const uint8_t* useIv = (iv != nullptr) ? iv : _aesIv;
    
    // Descifrar con AES-128-CBC
    CBC<AES128> cbcAes;
    cbcAes.setKey(_aesKey, 16);
    cbcAes.setIV(useIv, 16);
    
    cbcAes.decrypt(plaintext, ciphertext, ciphertextLen);
    
    // Remover padding PKCS7
    *plaintextLen = removePkcs7Padding(plaintext, ciphertextLen, 16);
    
    if (*plaintextLen == 0) {
        PKE_LOG(TAG_SEC, "Error en padding PKCS7");
        return false;
    }
    
    return true;
}

// =============================================================================
// Rolling Codes
// =============================================================================

bool SecurityManager::validateRollingCode(uint32_t receivedCode) {
    uint32_t expected = _status.rollingCode.counter;
    
    // Aceptar codigos dentro de una ventana hacia adelante
    // Esto maneja el caso donde el telefono envio comandos que no llegaron
    // y su contador avanzo mas que el del ESP32
    if (receivedCode >= expected && receivedCode < expected + ROLLING_CODE_WINDOW) {
        // Codigo valido - actualizar contador
        _status.rollingCode.counter = receivedCode + 1;
        _status.rollingCode.lastValidCounter = receivedCode;
        _status.rollingCode.synchronized = true;
        
        // Persistir nuevo estado
        saveRollingCode();
        
        return true;
    }
    
    // Codigo fuera de ventana o en el pasado (posible ataque replay)
    PKE_LOG(TAG_SEC, "Rolling code fuera de ventana: recibido=%lu, esperado=%lu-%lu",
            receivedCode, expected, expected + ROLLING_CODE_WINDOW - 1);
    
    return false;
}

uint32_t SecurityManager::getNextExpectedCode() {
    return _status.rollingCode.counter;
}

bool SecurityManager::syncRollingCode(uint32_t newCounter) {
    PKE_LOG(TAG_SEC, "Sincronizando rolling code a: %lu", newCounter);
    
    _status.rollingCode.counter = newCounter;
    _status.rollingCode.synchronized = true;
    saveRollingCode();
    
    return true;
}

void SecurityManager::resetRollingCode() {
    PKE_LOG(TAG_SEC, "Reseteando rolling code a 0");
    
    _status.rollingCode.counter = 0;
    _status.rollingCode.lastValidCounter = 0;
    _status.rollingCode.synchronized = false;
    saveRollingCode();
}

// =============================================================================
// Gestion de Intentos Fallidos
// =============================================================================

void SecurityManager::registerFailure() {
    _status.authFailures++;
    _status.lastAuthAttempt = millis();
    
    PKE_LOG(TAG_SEC, "Fallo de autenticacion #%d/%d", _status.authFailures, MAX_AUTH_FAILURES);
    
    if (_status.authFailures >= MAX_AUTH_FAILURES) {
        _status.lockedOut = true;
        _status.lockoutStart = millis();
        PKE_LOG(TAG_SEC, "*** SISTEMA BLOQUEADO por %d ms ***", AUTH_LOCKOUT_TIME_MS);
    }
    
    saveFailureCount();
}

void SecurityManager::registerSuccess() {
    if (_status.authFailures > 0) {
        PKE_LOG(TAG_SEC, "Autenticacion exitosa - reseteando contador de fallos");
    }
    
    _status.authFailures = 0;
    _status.lockedOut = false;
    _status.lastAuthAttempt = millis();
    
    saveFailureCount();
}

bool SecurityManager::isLockedOut() {
    if (!_status.lockedOut) return false;
    
    // Verificar si el tiempo de bloqueo ya expiro
    if (millis() - _status.lockoutStart >= AUTH_LOCKOUT_TIME_MS) {
        PKE_LOG(TAG_SEC, "Periodo de bloqueo expirado - desbloqueando");
        _status.lockedOut = false;
        _status.authFailures = 0;
        saveFailureCount();
        return false;
    }
    
    return true;
}

uint32_t SecurityManager::getLockoutRemaining() {
    if (!_status.lockedOut) return 0;
    
    uint32_t elapsed = millis() - _status.lockoutStart;
    if (elapsed >= AUTH_LOCKOUT_TIME_MS) return 0;
    
    return AUTH_LOCKOUT_TIME_MS - elapsed;
}

void SecurityManager::resetLockout() {
    PKE_LOG(TAG_SEC, "Reseteo manual de bloqueo");
    _status.lockedOut = false;
    _status.authFailures = 0;
    _status.lockoutStart = 0;
    saveFailureCount();
}

// =============================================================================
// Estado
// =============================================================================

SecurityStatus SecurityManager::getStatus() {
    return _status;
}

uint8_t SecurityManager::getFailureCount() {
    return _status.authFailures;
}

// =============================================================================
// Utilidades Privadas
// =============================================================================

size_t SecurityManager::applyPkcs7Padding(uint8_t* data, size_t dataLen, size_t blockSize) {
    // PKCS7: agregar N bytes donde cada byte tiene valor N
    // N = blockSize - (dataLen % blockSize)
    size_t paddingLen = blockSize - (dataLen % blockSize);
    
    for (size_t i = 0; i < paddingLen; i++) {
        data[dataLen + i] = (uint8_t)paddingLen;
    }
    
    return dataLen + paddingLen;
}

size_t SecurityManager::removePkcs7Padding(uint8_t* data, size_t dataLen, size_t blockSize) {
    if (dataLen == 0 || (dataLen % blockSize) != 0) return 0;
    
    // El ultimo byte indica cuantos bytes de padding hay
    uint8_t paddingLen = data[dataLen - 1];
    
    // Validar padding
    if (paddingLen == 0 || paddingLen > blockSize) return 0;
    if (paddingLen > dataLen) return 0;
    
    // Verificar que todos los bytes de padding son correctos
    for (size_t i = dataLen - paddingLen; i < dataLen; i++) {
        if (data[i] != paddingLen) return 0;
    }
    
    return dataLen - paddingLen;
}

bool SecurityManager::secureCompare(const uint8_t* a, const uint8_t* b, size_t length) {
    // Comparacion en tiempo constante para evitar ataques de timing
    // No sale temprano en caso de diferencia - siempre recorre todos los bytes
    volatile uint8_t result = 0;
    
    for (size_t i = 0; i < length; i++) {
        result |= a[i] ^ b[i];
    }
    
    return (result == 0);
}

// =============================================================================
// Persistencia NVS
// =============================================================================

void SecurityManager::loadRollingCode() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);  // Read-only
    
    _status.rollingCode.counter = prefs.getULong(NVS_KEY_ROLLING_CODE, 0);
    _status.rollingCode.lastValidCounter = _status.rollingCode.counter > 0 ? 
                                            _status.rollingCode.counter - 1 : 0;
    _status.rollingCode.synchronized = (_status.rollingCode.counter > 0);
    
    prefs.end();
    
    PKE_LOG(TAG_SEC, "Rolling code cargado desde NVS: %lu", _status.rollingCode.counter);
}

void SecurityManager::saveRollingCode() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);  // Read-write
    prefs.putULong(NVS_KEY_ROLLING_CODE, _status.rollingCode.counter);
    prefs.end();
}

void SecurityManager::loadFailureCount() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    _status.authFailures = prefs.getUChar(NVS_KEY_AUTH_FAILURES, 0);
    prefs.end();
}

void SecurityManager::saveFailureCount() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar(NVS_KEY_AUTH_FAILURES, _status.authFailures);
    prefs.end();
}
