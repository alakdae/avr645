# AVR645 ESPHome Component

An **ESPHome external component** for integrating **Harman Kardon AVR-645** receivers via **RS-232 (UART)**.

The component:

* reads `MPSEND` status frames from the AVR
* exposes the **display lines** and **volume** to Home Assistant
* allows sending **control commands** back to the AVR
* optionally mirrors raw UART data to TCP (for debugging / TCP control)

---

## Features

* ✅ Reliable **state-machine parser** for `MPSEND` frames
* ✅ Upper & lower display text as `text_sensor`
* ✅ Volume parsed from `VOL -xxdB` as numeric `sensor`
* ✅ UART-safe (handles split packets, noise, timing issues)
* ✅ ESPHome-native (no deprecated `custom` platform)
* ✅ Optional TCP server for raw AVR traffic

---

## Supported Device

* **Harman Kardon AVR-645**

> Other HK models using the same `MPSEND / PCSEND` protocol may work, but are untested.

---

## Wiring

The AVR-645 uses **RS-232 (38 400 baud, 8N1)**.

You need:

* ESP32 / ESP8266
* RS-232 ↔ TTL level converter (MAX3232 or similar)

**Important:**
Do **not** connect RS-232 directly to ESP GPIOs.

---

## Installation (ESPHome External Component)

Add this to your ESPHome YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/YOUR_GITHUB/esphome-avr645
      ref: main   # or a tag like v1.0.0
    components: [avr645]
```

---

## ESPHome Configuration Example

### UART

```yaml
uart:
  id: uart0
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 38400
```

---

### Display Sensors (from MPSEND)

```yaml
text_sensor:
  - platform: template
    id: avr_upper
    name: "AVR Upper Display"

  - platform: template
    id: avr_lower
    name: "AVR Lower Display"
```

---

### Volume Sensor

```yaml
sensor:
  - platform: template
    id: avr_volume
    name: "AVR Volume"
    unit_of_measurement: "dB"
    accuracy_decimals: 0
```

---

### AVR645 Component

```yaml
avr645:
  uart_id: uart0
  upper_id: avr_upper
  lower_id: avr_lower
  volume_id: avr_volume
  port: 4001   # optional TCP server port
```

---

## Exposed Entities

| Entity            | Type          | Description               |
| ----------------- | ------------- | ------------------------- |
| AVR Upper Display | `text_sensor` | First display line        |
| AVR Lower Display | `text_sensor` | Second display line       |
| AVR Volume        | `sensor`      | Volume in dB (e.g. `-43`) |

Volume is parsed from lines like:

```
VOL -43dB
```

---

## Control Commands (PCSEND)

The component supports sending 4-byte **PCSEND** commands to the AVR
(e.g. volume up/down, input select, power).

These are exposed internally and can be wired to:

* buttons
* scripts
* automations

(Exact command mappings depend on AVR protocol documentation.)

---

## TCP Debug Server (Optional)

If `port` is set:

* the component opens a **non-blocking TCP server**
* raw UART traffic is forwarded to connected clients

Useful for:

* protocol inspection
* legacy control software
* debugging new commands

---

## Protocol Notes

* Parsing is done via a **byte-level state machine**
* Data is only published after a **complete, validated frame**
* Partial frames and noise are ignored safely

Frame structure (simplified):

```
MPSEND
03 32
F0 <14 bytes>   upper line
F1 <14 bytes>   lower line
F2 <status>
```

---

## Repository Structure

```
custom_components/avr645/
├── __init__.py
├── avr645.py   # ESPHome codegen & config
└── avr645.h    # C++ implementation
```

---

## Compatibility

* ESPHome 2023.12+
* ESP32 / ESP8266
* Home Assistant via ESPHome integration

---

## License

MIT

---

## Support

If you find this project helpful, you can support me here:

[![Buy Me a Coffee](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://buymeacoffee.com/alakdae)



