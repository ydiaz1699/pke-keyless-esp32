# PKE + Keyless Start System (ESP32 + Android)

Sistema de **Entrada Pasiva Sin Llave (PKE)** + **Encendido por Botón (Keyless Start)** usando ESP32 y una app Android (Flutter) como llave digital.

## Arquitectura

```
┌──────────────┐        BLE        ┌──────────────┐
│   Android    │ ◄──────────────► │    ESP32     │
│   (Flutter)  │   Challenge/Resp  │  (Firmware)  │
│              │   RSSI Proximity  │              │
│  Llave BLE   │   Commands/Status │  Controlador │
└──────────────┘                   └──────┬───────┘
                                          │ GPIO / CAN
                                          ▼
                                   ┌──────────────┐
                                   │   Vehículo   │
                                   │  Cerraduras  │
                                   │  Encendido   │
                                   │  Alarma      │
                                   └──────────────┘
```

## Zonas de Proximidad PKE

| Zona | RSSI | Acción |
|------|------|--------|
| DENTRO | > -50 dBm | Botón START habilitado |
| CERCA | -70 a -50 dBm | Desbloqueo automático |
| LEJOS | < -80 dBm | Bloqueo + alarma |

## Modos de Vehículo Soportados

1. **Cierre Centralizado** - Pulso a la central existente
2. **Cable Directo** - Actuadores de motor en puertas
3. **CAN Bus** - Mensajes CAN al BCM/ECU

## Seguridad

- AES-128-CBC cifrado
- Challenge-Response autenticación mutua
- Rolling Codes anti-replay
- Bloqueo temporal por intentos fallidos

## Compilar

```bash
# Requiere PlatformIO CLI
pip install platformio

# Compilar
pio run

# Upload al ESP32
pio run --target upload

# Monitor serial
pio device monitor
```

## Configuración

Edita `include/config.h` para ajustar:
- Pines GPIO según tu instalación
- Umbrales RSSI para las zonas
- Modo de vehículo
- Clave AES compartida
- Funciones opcionales (ventanas, espejos, alarma)

## Hardware Necesario

- ESP32 DevKit (~$5-8 USD)
- Módulo relé 4-6 canales
- Buzzer piezoeléctrico
- Botón pulsador iluminado
- Regulador DC-DC 12V→3.3V/5V
- Sensores (puerta, freno, capó)

## Licencia

MIT
