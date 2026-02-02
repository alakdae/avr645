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

This component exposes a helper method:

```cpp
send_info(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
```

Each command consists of **4 bytes** (`1st`, `2nd`, `3rd`, `4th`) which are wrapped internally into a valid `PCSEND` frame.

---

## Command Reference

### Power & Audio

| Command     | 1st  | 2nd  | 3rd  | 4th  |
| ----------- | ---- | ---- | ---- | ---- |
| Power ON    | `80` | `70` | `C0` | `3F` |
| Power OFF   | `80` | `70` | `9F` | `60` |
| Mute        | `80` | `70` | `C1` | `3E` |
| Volume Up   | `80` | `70` | `C7` | `38` |
| Volume Down | `80` | `70` | `C8` | `37` |
| Sleep       | `80` | `70` | `DB` | `24` |
| Dimmer      | `80` | `70` | `DC` | `23` |

---

### Input Selection

| Input     | 1st  | 2nd  | 3rd  | 4th  |
| --------- | ---- | ---- | ---- | ---- |
| AVR       | `82` | `72` | `35` | `CA` |
| DVD       | `80` | `70` | `D0` | `2F` |
| CD        | `80` | `70` | `C4` | `3B` |
| Tape      | `80` | `70` | `CC` | `33` |
| VID1      | `80` | `70` | `CA` | `35` |
| VID2      | `80` | `70` | `CB` | `34` |
| VID3      | `80` | `70` | `CE` | `31` |
| VID4      | `80` | `70` | `D1` | `2E` |
| VID5      | `80` | `70` | `F0` | `0F` |
| AM / FM   | `80` | `70` | `81` | `7E` |
| 6CH / 8CH | `82` | `72` | `DB` | `24` |

---

### Surround & Sound Modes

| Mode      | 1st  | 2nd  | 3rd  | 4th  |
| --------- | ---- | ---- | ---- | ---- |
| Surround  | `82` | `72` | `58` | `A7` |
| Dolby     | `82` | `72` | `50` | `AF` |
| DTS       | `82` | `72` | `A0` | `5F` |
| DTS NEO:6 | `82` | `72` | `A1` | `5E` |
| Logic 7   | `82` | `72` | `A2` | `5D` |
| Stereo    | `82` | `72` | `9B` | `64` |
| Night     | `82` | `72` | `96` | `69` |
| Test Tone | `82` | `72` | `8C` | `73` |

---

### Tuner & Presets

| Command     | 1st  | 2nd  | 3rd  | 4th  |
| ----------- | ---- | ---- | ---- | ---- |
| Tune Up     | `80` | `70` | `84` | `7B` |
| Tune Down   | `80` | `70` | `85` | `7A` |
| Preset Up   | `82` | `72` | `D0` | `2F` |
| Preset Down | `82` | `72` | `D1` | `2E` |
| FM Mode     | `80` | `70` | `93` | `6C` |
| RDS         | `82` | `72` | `DD` | `22` |
| Prescan     | `80` | `70` | `96` | `69` |

---

### Speaker / Channel / Delay

| Command      | 1st  | 2nd  | 3rd  | 4th  |
| ------------ | ---- | ---- | ---- | ---- |
| Delay        | `82` | `72` | `52` | `AD` |
| Delay Up     | `82` | `72` | `8A` | `75` |
| Delay Down   | `82` | `72` | `8B` | `74` |
| Speaker      | `82` | `72` | `53` | `AC` |
| Speaker Up   | `82` | `72` | `8E` | `71` |
| Speaker Down | `82` | `72` | `8F` | `70` |
| Channel      | `82` | `72` | `5D` | `A2` |

---

### OSD / Menu

| Command   | 1st  | 2nd  | 3rd  | 4th  |
| --------- | ---- | ---- | ---- | ---- |
| OSD       | `82` | `72` | `5C` | `A3` |
| OSD Left  | `82` | `72` | `C1` | `3E` |
| OSD Right | `82` | `72` | `C2` | `3D` |
| Clear     | `82` | `72` | `D9` | `26` |
| Memory    | `80` | `70` | `86` | `79` |

---

### Multiroom

| Command        | 1st  | 2nd  | 3rd  | 4th  |
| -------------- | ---- | ---- | ---- | ---- |
| Multiroom      | `82` | `72` | `DF` | `20` |
| Multiroom Up   | `82` | `72` | `5E` | `A1` |
| Multiroom Down | `82` | `72` | `5F` | `A0` |

---

## ESPHome Button Examples

Each command can be exposed as a Home Assistant button using a simple `lambda`.

### Power Buttons

```yaml
button:
  - platform: template
    name: "AVR Power On"
    on_press:
      lambda: |-
        if (esphome::avr645::AVR645::instance)
          esphome::avr645::AVR645::instance->send_info(0x80,0x70,0xC0,0x3F);

  - platform: template
    name: "AVR Power Off"
    on_press:
      lambda: |-
        if (esphome::avr645::AVR645::instance)
          esphome::avr645::AVR645::instance->send_info(0x80,0x70,0x9F,0x60);
```

---

### Volume Control

```yaml
  - platform: template
    name: "AVR Volume Up"
    on_press:
      lambda: |-
        if (esphome::avr645::AVR645::instance)
          esphome::avr645::AVR645::instance->send_info(0x80,0x70,0xC7,0x38);

  - platform: template
    name: "AVR Volume Down"
    on_press:
      lambda: |-
        if (esphome::avr645::AVR645::instance)
          esphome::avr645::AVR645::instance->send_info(0x80,0x70,0xC8,0x37);
```

---

### Input Selection Example

```yaml
  - platform: template
    name: "AVR DVD Input"
    on_press:
      lambda: |-
        if (esphome::avr645::AVR645::instance)
          esphome::avr645::AVR645::instance->send_info(0x80,0x70,0xD0,0x2F);
```

---

## Notes

* Commands are **idempotent** (safe to resend)
* No response is expected for `PCSEND`
* Display & volume updates arrive asynchronously via `MPSEND`

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



