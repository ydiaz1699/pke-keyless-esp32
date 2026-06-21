# Protección Anti-Relay Attack - PKE + Keyless Start

## ¿Qué es un Relay Attack?

Es el método de robo **más común** en vehículos con sistemas PKE (Keyless Entry).
Dos ladrones trabajan juntos con amplificadores de señal:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        ATAQUE RELAY (Sin protección)                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   TU CASA                            ESTACIONAMIENTO                         │
│   ┌─────┐                            ┌────────────────┐                     │
│   │     │                            │                │                     │
│   │  📱 │  ← Tu teléfono            │   🚗 Tu Auto   │                     │
│   │     │    dormido en              │                │                     │
│   │     │    la mesa de noche        │                │                     │
│   └──┬──┘                            └───────┬────────┘                     │
│      │                                       │                              │
│      │ Señal BLE original                    │ Señal BLE original            │
│      │ (alcance ~5m)                         │ (alcance ~5m)                │
│      │                                       │                              │
│      ▼                                       ▼                              │
│   ┌──────┐         Radio/WiFi          ┌──────┐                            │
│   │LADRÓN│ ◄═══════════════════════════►│LADRÓN│                            │
│   │  #1  │    Amplifica y retransmite   │  #2  │                            │
│   │      │    la señal BLE a distancia  │      │                            │
│   └──────┘                              └──────┘                            │
│                                                                              │
│   El auto "cree" que tu teléfono está cerca                                 │
│   → Desbloquea puertas                                                      │
│   → Habilita botón de arranque                                              │
│   → Los ladrones se llevan tu auto                                          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Contramedidas Implementadas

Nuestro sistema implementa **5 capas de protección** que hacen
prácticamente imposible un relay attack exitoso:

```
╔══════════════════════════════════════════════════════════════════╗
║                  5 CAPAS DE DEFENSA ANTI-RELAY                   ║
╠══════════════════════════════════════════════════════════════════╣
║                                                                  ║
║   Capa 1: DETECCIÓN DE LATENCIA RTT                              ║
║           (Round-Trip Time)                                      ║
║           → Mide el tiempo que tarda la señal en ir y volver.    ║
║             Un relay agrega ~5-50ms de latencia extra.           ║
║                                                                  ║
║   Capa 2: DETECCIÓN DE VELOCIDAD DE CAMBIO DE RSSI              ║
║           (Rate of Change Analysis)                              ║
║           → Una persona caminando cambia el RSSI gradualmente.   ║
║             Un relay aparece "de golpe" con señal fuerte.        ║
║                                                                  ║
║   Capa 3: DETECCIÓN DE MOVIMIENTO DEL TELÉFONO                  ║
║           (Acelerómetro + Giroscopio)                            ║
║           → Si el teléfono está quieto (mesa de noche) y la     ║
║             señal dice que está "caminando", es un relay.        ║
║                                                                  ║
║   Capa 4: GEOFENCING (Perímetro virtual)                         ║
║           → Si el GPS del teléfono dice que estás a 200m del    ║
║             auto, pero el BLE dice que estás a 2m, es relay.    ║
║                                                                  ║
║   Capa 5: MODO NOCTURNO / MODO SEGURO                           ║
║           (Desactivación por horario)                            ║
║           → Entre 11pm y 6am, el PKE automático se desactiva.   ║
║             Solo funciona con confirmación manual en la app.     ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
```

---
