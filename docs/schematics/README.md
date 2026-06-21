# Esquemáticos de Conexión - PKE + Keyless Start (ESP32)

## Contenido

1. [Diagrama Base (común a los 3 modos)](#diagrama-base)
2. [Modo 1: Cierre Centralizado](#modo-1-cierre-centralizado)
3. [Modo 2: Cable Directo](#modo-2-cable-directo)
4. [Modo 3: CAN Bus](#modo-3-can-bus)
5. [Lista de Materiales (BOM)](#lista-de-materiales)
6. [Notas de Seguridad](#notas-de-seguridad)

---

## Diagrama Base

Este es el circuito central que es COMÚN a los 3 modos de instalación.
Solo cambia la sección de "interfaz con el vehículo".


```
╔══════════════════════════════════════════════════════════════════════════════╗
║                    DIAGRAMA BASE - CIRCUITO CENTRAL                         ║
╚══════════════════════════════════════════════════════════════════════════════╝

                         ┌─────────────────────────┐
     Batería 12V ───────┤  Regulador DC-DC         │
     del Vehículo       │  (LM2596 o MP1584)       │
     (Permanente)       │  12V → 5V / 3.3V        │
                         │                         │
                         │  VIN ──── 12V Bat       │
                         │  GND ──── Chasis        │
                         │  5V  ──── Módulo Relés  │
                         │  3.3V ─── ESP32 (3V3)   │
                         └────────┬────────────────┘
                                  │
                    ┌─────────────┴─────────────────┐
                    │                               │
                    ▼                               ▼
    ┌───────────────────────────┐    ┌──────────────────────────┐
    │       ESP32 DevKit        │    │   Módulo 6 Relés (5V)    │
    │                           │    │   (Optoacoplados)        │
    │  3V3 ◄── 3.3V Regulador  │    │                          │
    │  GND ◄── GND Común       │    │  IN1 ◄── GPIO25 (Unlock) │
    │                           │    │  IN2 ◄── GPIO26 (Lock)   │
    │  GPIO25 ──► Relay Unlock  │    │  IN3 ◄── GPIO27 (ACC)    │
    │  GPIO26 ──► Relay Lock    │    │  IN4 ◄── GPIO14 (IGN)    │
    │  GPIO27 ──► Relay ACC     │    │  IN5 ◄── GPIO12 (START)  │
    │  GPIO14 ──► Relay IGN     │    │  IN6 ◄── (Reserva)       │
    │  GPIO12 ──► Relay START   │    │                          │
    │                           │    │  VCC ◄── 5V              │
    │  GPIO33 ◄── Botón START   │    │  GND ◄── GND             │
    │  GPIO32 ──► LED Botón     │    │                          │
    │  GPIO34 ◄── Sensor Freno  │    │  COM/NO/NC en cada relé  │
    │  GPIO35 ◄── Sensor Puerta │    │  (ver conexiones abajo)  │
    │  GPIO36 ◄── Sensor Capó   │    └──────────────────────────┘
    │                           │
    │  GPIO13 ──► Buzzer        │
    │  GPIO2  ──► LED Estado    │
    │                           │
    │  (CAN Bus - solo Modo 3)  │
    │  GPIO21 ──► CAN TX        │
    │  GPIO22 ◄── CAN RX        │
    └───────────────────────────┘
```


### Conexión del Botón Push-to-Start

```
                    ┌─────────────────────┐
                    │  BOTÓN PUSH-TO-START │
                    │  (Iluminado, N.O.)   │
                    │                     │
    GPIO33 ─────────┤ Terminal 1 (Switch)  │
    GND ────────────┤ Terminal 2 (Switch)  │
                    │                     │
    GPIO32 ──[330Ω]─┤ LED + (Ánodo)       │
    GND ────────────┤ LED - (Cátodo)      │
                    └─────────────────────┘

    Nota: GPIO33 tiene pull-up interno habilitado en el firmware.
    Cuando se presiona el botón, GPIO33 va a LOW.
    GPIO32 enciende el LED cuando el sistema habilita el arranque.
```

### Conexión del Buzzer

```
    GPIO13 ──[100Ω]──┤ Base (2N2222A)
                      │
    5V ───────────────┤ Colector ──── Buzzer (+)
                      │
    GND ──────────────┤ Emisor
    GND ──────────────┤ Buzzer (-)

    Nota: El transistor NPN 2N2222A amplifica la señal del GPIO
    para manejar el buzzer piezoeléctrico a 5V.
    Resistencia de base: 100-220Ω.
```

### Conexión de Sensores

```
    ┌─── SENSOR DE FRENO ──────────────────────────────────┐
    │                                                       │
    │   Cable de señal de freno ──[10kΩ pull-down]── GND    │
    │   del vehículo (12V cuando         │                  │
    │   se pisa el freno)                 │                  │
    │                            ┌────────┤                  │
    │                            │  Divisor de voltaje       │
    │   12V freno ──[10kΩ]───────┤                          │
    │                            ├──── GPIO34 (ADC)          │
    │                   GND ──[4.7kΩ]──┘                    │
    │                                                       │
    │   Resultado: 12V → ~3.8V (seguro para ESP32)          │
    └───────────────────────────────────────────────────────┘

    ┌─── SENSOR DE PUERTA (Reed Switch o Pin Switch) ───────┐
    │                                                       │
    │   GPIO35 ──── Sensor (N.C.) ──── GND                  │
    │      │                                                │
    │      └── Pull-up interno activado                     │
    │                                                       │
    │   Puerta cerrada: GPIO35 = LOW (circuito cerrado)     │
    │   Puerta abierta: GPIO35 = HIGH (circuito abierto)    │
    └───────────────────────────────────────────────────────┘

    ┌─── SENSOR DE CAPÓ (mismo principio) ──────────────────┐
    │                                                       │
    │   GPIO36 ──── Sensor (N.C.) ──── GND                  │
    │      │                                                │
    │      └── Pull-up interno activado                     │
    │                                                       │
    │   Capó cerrado: GPIO36 = LOW                          │
    │   Capó abierto: GPIO36 = HIGH                         │
    └───────────────────────────────────────────────────────┘
```


### Alimentación del Sistema

```
╔══════════════════════════════════════════════════════════════════╗
║                    CIRCUITO DE ALIMENTACIÓN                       ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║   Batería         Fusible        Regulador         Cargas        ║
║   12V Auto        10A            DC-DC                           ║
║                                                                  ║
║   (+) ──[FUSIBLE]──┬──────── VIN                                ║
║                    │           │                                 ║
║                    │      ┌────┴────┐                            ║
║                    │      │ LM2596  │──── 5V ──► Módulo Relés   ║
║                    │      │ o       │                            ║
║                    │      │ MP1584  │──── 3.3V ─► ESP32 (3V3)   ║
║                    │      └────┬────┘                            ║
║                    │           │                                 ║
║   (-) ─────────────┴───────── GND ──────────────► GND Común     ║
║   (Chasis)                                                       ║
║                                                                  ║
║   IMPORTANTE:                                                    ║
║   • Conectar a cable PERMANENTE (no se corta con la llave)       ║
║   • El fusible protege todo el circuito                          ║
║   • Usar cable AWG 16-18 para la línea de 12V                   ║
║   • El ESP32 consume ~80mA en standby BLE                       ║
║   • Los relés consumen ~70mA cada uno cuando activos             ║
║   • Consumo total máximo: ~600mA (todos los relés activos)       ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---

## Modo 1: Cierre Centralizado

**Para vehículos que YA tienen cierre centralizado de fábrica.**

Este es el modo más sencillo. Se conecta EN PARALELO al control
remoto original del auto. Un pulso corto en el cable correcto
activa/desactiva las cerraduras.


```
╔══════════════════════════════════════════════════════════════════╗
║           MODO 1: CIERRE CENTRALIZADO (Pulso)                    ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║   ┌──────────────┐                    ┌──────────────────────┐  ║
║   │  Módulo de   │                    │  Central de Cierre   │  ║
║   │  Relés ESP32 │                    │  del Vehículo        │  ║
║   │              │                    │                      │  ║
║   │  Relay 1     │    Pulso 300ms     │  Cable UNLOCK        │  ║
║   │  (Unlock)    ├───── COM ──────────┤  (buscar en el       │  ║
║   │  GPIO25      │    NO ── 12V       │   conector del       │  ║
║   │              │                    │   control remoto     │  ║
║   │  Relay 2     │    Pulso 300ms     │   original)          │  ║
║   │  (Lock)      ├───── COM ──────────┤  Cable LOCK          │  ║
║   │  GPIO26      │    NO ── 12V       │                      │  ║
║   │              │                    │                      │  ║
║   └──────────────┘                    └──────────────────────┘  ║
║                                                                  ║
║   CÓMO ENCONTRAR LOS CABLES:                                    ║
║                                                                  ║
║   1. Localiza el módulo receptor del control remoto original     ║
║      (generalmente detrás del tablero o bajo el asiento)         ║
║                                                                  ║
║   2. Con un multímetro, identifica los cables que reciben        ║
║      un pulso positivo (+12V) cuando presionas Lock/Unlock       ║
║      en el control original                                      ║
║                                                                  ║
║   3. Conecta el COM del relé al cable identificado              ║
║      y el NO del relé a +12V de la batería                       ║
║                                                                  ║
║   4. Cuando el ESP32 activa el relé por 300ms, simula            ║
║      exactamente lo que hace el control original                  ║
║                                                                  ║
║   TIPOS DE SEÑAL (depende del fabricante):                       ║
║                                                                  ║
║   • Pulso POSITIVO (+12V): Lo más común                          ║
║     → COM al cable de señal, NO a +12V                           ║
║                                                                  ║
║   • Pulso NEGATIVO (GND): Algunos Toyota, Honda                  ║
║     → COM al cable de señal, NO a GND (chasis)                   ║
║                                                                  ║
║   • Pulso a un solo cable (alternante): Algunos VW, Audi         ║
║     → Un solo relé con timer que alterna duración del pulso      ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

### Diagrama de Conexión Detallado - Modo 1

```
    BATERÍA 12V (+)
         │
         │    ┌────── Relé 1 (UNLOCK) ──────┐
         ├────┤ NO (Normally Open)           │
         │    │                              │
         │    │ COM ─────────────────────────┼──► Cable UNLOCK
         │    │                              │    del módulo de
         │    │ NC (No conectar)             │    cierre central
         │    │                              │
         │    │ Bobina: GPIO25 → IN1         │
         │    └──────────────────────────────┘
         │
         │    ┌────── Relé 2 (LOCK) ────────┐
         ├────┤ NO (Normally Open)           │
         │    │                              │
         │    │ COM ─────────────────────────┼──► Cable LOCK
         │    │                              │    del módulo de
         │    │ NC (No conectar)             │    cierre central
         │    │                              │
         │    │ Bobina: GPIO26 → IN2         │
         │    └──────────────────────────────┘
         │
    GND (Chasis) ──── GND Común
```


---

## Modo 2: Cable Directo

**Para vehículos SIN cierre centralizado (manuales, clásicos, Hot Rods).**

Se instalan actuadores de motor (solenoides) en cada puerta.
El ESP32 los controla directamente con los relés.

```
╔══════════════════════════════════════════════════════════════════╗
║           MODO 2: CABLE DIRECTO (Actuadores de Motor)            ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║   Los actuadores de puerta son motores DC reversibles.           ║
║   Cambian de dirección según la polaridad (+/-) aplicada.        ║
║   Se usa un circuito "puente H" con 2 relés por puerta.         ║
║                                                                  ║
║   SIMPLIFICADO: Usamos los 2 relés principales para             ║
║   controlar TODOS los actuadores en paralelo.                    ║
║                                                                  ║
║                                                                  ║
║         Relé UNLOCK (GPIO25)          Relé LOCK (GPIO26)         ║
║         ┌─────────────┐              ┌─────────────┐            ║
║   12V ──┤ NO       COM├──┐     12V ──┤ NO       COM├──┐         ║
║         └─────────────┘  │           └─────────────┘  │         ║
║                          │                            │         ║
║                          │    ┌────────────────┐      │         ║
║                          └────┤ Motor (+)      ├──────┘         ║
║                               │ Actuador       │                ║
║                               │ Puerta 1       │                ║
║                          ┌────┤ Motor (-)      ├──────┐         ║
║                          │    └────────────────┘      │         ║
║                          │                            │         ║
║                          │    ┌────────────────┐      │         ║
║                          ├────┤ Motor (+)      ├──────┤         ║
║                          │    │ Actuador       │      │         ║
║                          │    │ Puerta 2       │      │         ║
║                          │    └────────────────┘      │         ║
║                          │                            │         ║
║                          │    ┌────────────────┐      │         ║
║                          ├────┤ Motor (+)      ├──────┤         ║
║                          │    │ Actuador       │      │         ║
║                          │    │ Puerta 3       │      │         ║
║                          │    └────────────────┘      │         ║
║                          │                            │         ║
║                          │    ┌────────────────┐      │         ║
║                          └────┤ Motor (+)      ├──────┘         ║
║                               │ Actuador       │                ║
║                               │ Puerta 4       │                ║
║                               └────────────────┘                ║
║                                                                  ║
║   FUNCIONAMIENTO:                                                ║
║   • UNLOCK: Relé 1 activa → +12V en terminal (+) → Motor gira   ║
║     en sentido horario → Pestillo se abre (800ms)               ║
║   • LOCK: Relé 2 activa → +12V en terminal (-) → Motor gira     ║
║     en sentido antihorario → Pestillo se cierra (800ms)          ║
║                                                                  ║
║   IMPORTANTE:                                                    ║
║   • NUNCA activar ambos relés al mismo tiempo (cortocircuito)    ║
║   • El firmware tiene delay de 50ms entre desactivar uno y       ║
║     activar otro                                                 ║
║   • Usar cable AWG 14-16 para los actuadores (alto amperaje)     ║
║   • Cada actuador consume 3-5A brevemente                        ║
║   • Fusible de 20A en la línea de 12V a los actuadores           ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```


### Instalación del Actuador en la Puerta

```
    ┌──────────────────────────────────────────────────┐
    │                 PUERTA DEL AUTO                    │
    │                                                   │
    │   ┌─────────────────────┐                         │
    │   │  Actuador de Motor  │                         │
    │   │  (Universal 12V)    │                         │
    │   │                     │                         │
    │   │  ┌───┐  ┌───────┐  │        ┌──────────┐    │
    │   │  │ M │──│Varilla│──┼────────►│ Pestillo │    │
    │   │  └───┘  └───────┘  │        │ de puerta│    │
    │   │                     │        └──────────┘    │
    │   │  Cable 1: Rojo ─────┼──── → Al relé UNLOCK   │
    │   │  Cable 2: Azul ─────┼──── → Al relé LOCK     │
    │   │                     │                         │
    │   └─────────────────────┘                         │
    │                                                   │
    │   Montaje: Atornillar al metal de la puerta       │
    │   con la varilla conectada al mecanismo del       │
    │   pestillo existente.                             │
    │                                                   │
    └──────────────────────────────────────────────────┘
```

---

## Modo 3: CAN Bus

**Para vehículos modernos (2008+) con red CAN Bus.**

Se envían mensajes directamente al BCM (Body Control Module)
simulando los comandos del módulo de llave original.
Es la integración más limpia pero requiere conocer los IDs CAN
específicos del vehículo.

```
╔══════════════════════════════════════════════════════════════════╗
║              MODO 3: CAN BUS (Vehículos Modernos)                ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║                  ┌──────────────┐                                ║
║                  │    ESP32     │                                ║
║                  │              │                                ║
║                  │  GPIO21 (TX) ├────────┐                       ║
║                  │  GPIO22 (RX) ├─────┐  │                       ║
║                  │              │     │  │                       ║
║                  └──────────────┘     │  │                       ║
║                                       │  │                       ║
║                  ┌────────────────────┴──┴───────────────┐      ║
║                  │     Módulo CAN Transceiver             │      ║
║                  │     (MCP2515 + TJA1050)                │      ║
║                  │     o (SN65HVD230)                     │      ║
║                  │                                        │      ║
║                  │  CTX ◄── GPIO21 (TX del ESP32)         │      ║
║                  │  CRX ──► GPIO22 (RX del ESP32)         │      ║
║                  │  VCC ◄── 5V                            │      ║
║                  │  GND ◄── GND                           │      ║
║                  │                                        │      ║
║                  │  CANH ──────────────────────────┐      │      ║
║                  │  CANL ─────────────────────┐    │      │      ║
║                  │                            │    │      │      ║
║                  └────────────────────────────┼────┼──────┘      ║
║                                               │    │             ║
║                          ┌────────────────────┼────┼──────┐      ║
║                          │  CONECTOR OBD-II   │    │      │      ║
║                          │  (bajo el tablero) │    │      │      ║
║                          │                    │    │      │      ║
║                          │  Pin 6: CAN-H ─────┘    │      │      ║
║                          │  Pin 14: CAN-L ──────────┘      │      ║
║                          │  Pin 4: Chasis GND              │      ║
║                          │  Pin 16: Bat +12V               │      ║
║                          │                                 │      ║
║                          └─────────────────────────────────┘      ║
║                                                                  ║
║                                                                  ║
║   ALTERNATIVA: Conectar directo a los cables CAN-H y CAN-L      ║
║   que van al BCM (Body Control Module) sin usar el OBD.          ║
║                                                                  ║
║   RESISTENCIA DE TERMINACIÓN:                                    ║
║   • El bus CAN necesita 120Ω entre CAN-H y CAN-L en cada        ║
║     extremo. El OBD ya tiene una. Si te conectas en medio        ║
║     del bus, NO necesitas agregar otra.                           ║
║   • Si te conectas directo al final del cable, agrega 120Ω.      ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```



### Circuito del Encendido (Común a los 3 modos)

```
╔══════════════════════════════════════════════════════════════════╗
║        CIRCUITO DE ENCENDIDO (Keyless Start)                     ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║   El sistema replica la secuencia del cilindro de encendido:     ║
║   OFF → ACC → ON (IGN) → START                                  ║
║                                                                  ║
║   CONECTOR DEL CILINDRO DE ENCENDIDO (Switch de llave):          ║
║                                                                  ║
║        ┌─────────────────────────────────────────────────┐       ║
║        │                                                 │       ║
║   BAT ─┤  Cable grueso rojo (12V permanente)             │       ║
║        │    │                                            │       ║
║        │    ├──── Relé ACC (GPIO27) ──────► ACC          │       ║
║        │    │     (Accesorios: radio, luces tablero)     │       ║
║        │    │                                            │       ║
║        │    ├──── Relé IGN (GPIO14) ──────► IGN          │       ║
║        │    │     (Ignición: inyectores, bomba, ECU)     │       ║
║        │    │                                            │       ║
║        │    └──── Relé START (GPIO12) ────► ST           │       ║
║        │          (Starter: motor de arranque)           │       ║
║        │          ⚠️ SOLO activar max 3 segundos        │       ║
║        │                                                 │       ║
║        └─────────────────────────────────────────────────┘       ║
║                                                                  ║
║   CONEXIÓN FÍSICA:                                               ║
║                                                                  ║
║   1. Localiza el conector del switch de encendido                ║
║      (detrás del cilindro de llave en la columna de dirección)   ║
║                                                                  ║
║   2. Identifica los cables:                                      ║
║      • BAT: Siempre tiene 12V (cable grueso, usualmente rojo)    ║
║      • ACC: Tiene 12V en posición ACC y ON                       ║
║      • IGN: Tiene 12V solo en posición ON y START                ║
║      • ST:  Tiene 12V solo en posición START (momentáneo)        ║
║                                                                  ║
║   3. Corta cada cable y conecta "en serie" con los relés:        ║
║                                                                  ║
║      BAT ──────── COM del Relé ──── NO ──────── ACC/IGN/ST      ║
║                                                                  ║
║      Cuando el relé se activa, pasa corriente como si            ║
║      giraras la llave a esa posición.                            ║
║                                                                  ║
║   SECUENCIA DEL FIRMWARE:                                        ║
║                                                                  ║
║      Relé ACC ON → espera 500ms →                                ║
║      Relé IGN ON → espera 500ms →                                ║
║      Relé START ON → máximo 3s → Relé START OFF                  ║
║      (ACC e IGN se mantienen activos con motor encendido)        ║
║                                                                  ║
║   ⚠️ SEGURIDAD:                                                  ║
║   • El relé START tiene timeout de hardware (3 seg máximo)       ║
║   • Se requiere freno pisado para arrancar (GPIO34)              ║
║   • No arranca con capó abierto (GPIO36)                         ║
║   • Arranque remoto: no requiere freno pero sí puertas cerradas  ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```



---

## Lista de Materiales (BOM)

### Componentes Esenciales (todos los modos)

| # | Componente | Especificación | Cantidad | Precio USD |
|---|-----------|----------------|----------|------------|
| 1 | ESP32 DevKit V1 | WROOM-32, 38 pines | 1 | $5-8 |
| 2 | Módulo relé 6 canales | 5V, optoacoplado, 10A | 1 | $6-9 |
| 3 | Regulador DC-DC | LM2596 o MP1584, 12V→5V, 3A | 1 | $2-3 |
| 4 | Regulador 3.3V | AMS1117-3.3 (si el DC-DC no tiene salida dual) | 1 | $0.50 |
| 5 | Botón pulsador iluminado | 16mm o 19mm, LED 12V, N.O. | 1 | $3-5 |
| 6 | Buzzer piezoeléctrico | 5V activo, 85dB+ | 1 | $1 |
| 7 | Transistor NPN | 2N2222A (para buzzer) | 1 | $0.20 |
| 8 | LED indicador | 5mm, verde o azul | 1 | $0.10 |
| 9 | Fusible + portafusible | 10A, tipo blade | 1 | $1 |
| 10 | Resistencias | 100Ω, 330Ω, 4.7kΩ, 10kΩ (surtido) | 10 | $0.50 |
| 11 | Diodos 1N4007 | Protección flyback para relés | 6 | $0.30 |
| 12 | Conectores Dupont | Macho-hembra, surtido | 40 | $2 |
| 13 | Cable automotriz | AWG 18 (señal), AWG 14 (potencia) | 10m | $3-5 |
| 14 | Caja plástica | Para encapsular el ESP32 + relés | 1 | $2-3 |
| 15 | Bridas/amarres | Nylon, para fijar cables | 20 | $1 |

### Adicionales según el modo

| Modo | Componente Extra | Cantidad | Precio USD |
|------|-----------------|----------|------------|
| Modo 2 | Actuadores de motor 12V (por puerta) | 2-4 | $8-15 c/u |
| Modo 2 | Cable AWG 14 extra (alto amperaje) | 5m | $3 |
| Modo 2 | Fusible 20A | 1 | $0.50 |
| Modo 3 | MCP2515 + TJA1050 (módulo CAN) | 1 | $3-5 |
| Modo 3 | Cable par trenzado (CAN-H, CAN-L) | 2m | $1 |

### Sensores (opcionales pero recomendados)

| # | Sensor | Uso | Precio USD |
|---|--------|-----|------------|
| 1 | Pin switch de puerta | Detectar puerta abierta | $1-2 |
| 2 | Pin switch de capó | Seguridad ante manipulación | $1-2 |
| 3 | Señal de freno (ya existe) | Seguridad para arranque | $0 (cable) |

### Costo Total Estimado

| Modo | Costo Total |
|------|-------------|
| Modo 1 (Centralizado) | **$25-40 USD** |
| Modo 2 (Cable Directo) | **$55-80 USD** |
| Modo 3 (CAN Bus) | **$30-45 USD** |



---

## Notas de Seguridad

### Precauciones Eléctricas

```
⚠️  IMPORTANTE - LEE ANTES DE INSTALAR:

1. DESCONECTA el terminal negativo de la batería ANTES de trabajar
   en el sistema eléctrico del vehículo.

2. NUNCA sueldes cables con la batería conectada.
   Riesgo de cortocircuito, chispas y daño a la ECU.

3. USA SIEMPRE fusibles en las líneas de alimentación.
   Un cortocircuito sin fusible puede causar un incendio.

4. Los cables del STARTER manejan corrientes de 100-200A.
   NUNCA toques ni modifiques el cable grueso que va
   directamente al motor de arranque.
   Solo corta el cable DELGADO de señal que activa el solenoide.

5. VERIFICA la polaridad del relé antes de conectar.
   Invertir la polaridad puede dañar el módulo de relés o el ESP32.

6. USA diodos de protección (flyback) en los relés.
   Los módulos optoacoplados ya los incluyen, pero verifica.

7. AÍSLA todas las conexiones con termocontraíble o cinta aislante
   de calidad automotriz (3M Super 33+).

8. MONTA el ESP32 y relés en un lugar SECO y VENTILADO.
   Evita: cerca del motor, zonas de calor extremo, donde entre agua.
   Ideal: detrás del tablero, bajo el asiento, en el maletero.
```

### Precauciones de Software

```
⚠️  SEGURIDAD DEL FIRMWARE:

1. CAMBIA la clave AES por defecto antes de usar el sistema.
   La clave en config.h es solo un ejemplo.
   Genera una clave aleatoria de 16 bytes única para tu instalación.

2. El arranque remoto tiene TIMEOUT de 10 minutos.
   Pasado ese tiempo, el motor se apaga automáticamente.

3. El STARTER tiene timeout de 3 segundos máximo.
   Esto protege al motor de arranque de sobrecalentamiento.

4. Si el ESP32 pierde energía durante un arranque,
   TODOS los relés se desactivan (seguro por diseño).
   Los relés son "normalmente abiertos" = sin energía, todo OFF.

5. El sistema NUNCA apaga el motor si detecta que estás conduciendo
   (si pierde la señal BLE con el motor encendido, mantiene los relés
   activos hasta que se presione el botón manualmente).
```

### Dónde Instalar el ESP32

```
UBICACIONES RECOMENDADAS:

✅ Detrás del tablero (cerca del conector OBD)
   - Protegido del clima
   - Acceso a cables de ignición
   - Buena cobertura BLE (plástico no bloquea señal)

✅ Bajo el asiento del conductor
   - Fácil acceso para mantenimiento
   - Cerca de los cables de puerta

✅ En la consola central
   - Protegido y centrado para BLE uniforme

❌ NO instalar en:
   - Compartimiento del motor (calor, agua, vibraciones)
   - Maletero (BLE no llega bien a las puertas delanteras)
   - Cerca del amplificador de audio (interferencia)
   - Sobre superficies metálicas (atenúan BLE)
```

---

## Pinout Completo del ESP32

```
┌─────────────────────────────────────────────────────┐
│                    ESP32 DevKit V1                    │
│                     (38 pines)                       │
├──────────────┬───────────────────────────────────────┤
│    Pin       │   Función en este proyecto            │
├──────────────┼───────────────────────────────────────┤
│  GPIO2       │   LED de Estado (integrado)           │
│  GPIO4       │   Ventanas (opcional)                 │
│  GPIO12      │   Relé STARTER                        │
│  GPIO13      │   Buzzer (vía transistor)             │
│  GPIO14      │   Relé IGNICIÓN                       │
│  GPIO16      │   Espejos Plegar (opcional)           │
│  GPIO17      │   Espejos Desplegar (opcional)        │
│  GPIO21      │   CAN TX (solo modo 3)               │
│  GPIO22      │   CAN RX (solo modo 3)               │
│  GPIO25      │   Relé UNLOCK (cerraduras)            │
│  GPIO26      │   Relé LOCK (cerraduras)              │
│  GPIO27      │   Relé ACC (accesorios)               │
│  GPIO32      │   LED del Botón START                 │
│  GPIO33      │   Botón START (entrada, pull-up)      │
│  GPIO34      │   Sensor de Freno (solo entrada)      │
│  GPIO35      │   Sensor de Puerta (solo entrada)     │
│  GPIO36 (VP) │   Sensor de Capó (solo entrada)       │
│  3V3         │   Alimentación desde regulador        │
│  GND         │   Tierra común                        │
│  VIN (5V)    │   Alternativa alimentación 5V USB     │
├──────────────┼───────────────────────────────────────┤
│  Libres:     │   GPIO0, 5, 15, 18, 19, 23           │
│              │   (disponibles para expansiones)       │
└──────────────┴───────────────────────────────────────┘

Notas sobre pines:
• GPIO34, 35, 36, 39 son SOLO ENTRADA (no tienen pull-up interno,
  usar resistencias externas de pull-up si es necesario)
• GPIO0, 2, 5, 12, 15 tienen funciones especiales en el boot
  (evitar conectar cosas que los pongan en HIGH/LOW al encender)
• GPIO12 DEBE estar en LOW al boot (el módulo de relé con
  optoacoplador garantiza esto con la resistencia pull-down)
```
