# 🍰 CAKE - The Desktop Companion Robot
![head (5)](https://github.com/user-attachments/assets/33881921-c660-4b10-8678-92d1e5b9c488)

**A desktop companion robot with expressive OLED eyes, WiFi control, and smart features.**  
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

![image](https://github.com/user-attachments/assets/622f118f-8433-42b4-a5ae-eb532373490c)

---

##  Repository Structure
### Key Files
| File | Description |
|------|-------------|
| [`esp1_6.ino`](./esp1_6.ino) | **ESP1 (Servo Controller)**: Latest firmware for motion control |
| [`esp2_6.ino`](./esp2_6.ino) | **ESP2 (Display/LEDs)**: OLED & NeoPixel control + Web UI |
| [`Gerber_CAKE_PCB_CAKE_2025-05-12.zip`](./Gerber_CAKE_PCB_CAKE_2025-05-12.zip) | PCB manufacturing files |
| [`Schematic_CAKE_2025-05-12.pdf`](./Schematic_CAKE_2025-05-12.pdf) | Full hardware schematic |

![head (4)](https://github.com/user-attachments/assets/2d7cbcfe-8d1f-413e-bc43-956577b27d5b)


### Older Versions
- `working1.ino` → `working3.ino`: Legacy code (deprecated)
- `disledfinal.ino`: Previous display/LED logic (now merged into `esp2_6.ino`)

---

## Features
- **Expressive OLED Eyes**: Angry/Happy/Sleepy moods with flicker effects
- **Dual Mobility**: Walk or roll modes (servo-controlled)
- **Web UI Control**:
  - Real-time joystick for movement
  - Set reminders/sticky notes
  - Adjust LED brightness & speed
- **Smart Functions**:
  - Auto-reconnect WiFi
  - Battery monitoring (servo feedback every 2s)
  - Power-saving standby mode

---

##  Hardware Setup
### Bill of Materials
- 2x ESP32 Dev Modules
- 2.4" SPI OLED (128x64)
- 4x SG90 Servos
- WS2812B NeoPixels (4x)
- Custom PCB ([Gerber files](./Gerber_CAKE_PCB_CAKE_2025-05-12.zip))

### Wiring
**ESP1 (Servos)**  
| Servo | ESP32 Pin |
|-------|-----------|
| Wheel L | GPIO 1 |
| Wheel R | GPIO 3 |
| Foot L | GPIO 4 |
| Foot R | GPIO 2 |

**ESP2 (Display/LEDs)**  
| Component | ESP32 Pin |
|-----------|-----------|
| OLED MOSI | GPIO 23 |
| OLED CLK | GPIO 18 |
| NeoPixels | GPIO 27 |

---



