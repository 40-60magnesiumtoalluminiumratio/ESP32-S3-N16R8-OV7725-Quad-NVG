# ESP32-S3-Quad+-NVG

An affordable, modular, and simple **ESP32-S3 Low-Light/Night Vision** solution utilizing an **OV7725** camera module and an **ST7789 1.54" TFT SPI** display driven by the `TFT_eSPI` library.

This project enables cost-effective, IR-illuminated night vision tailored for airsoft, educational experiments, and home surveillance. Its flagship feature is its **modular design**: you can operate a single node as a standalone monocular or link multiple nodes in a Master/Slave configuration via UART to create an expansive panoramic NVG viewing array.

---

## 🌐 System Architecture

```
     Master Node                      UART Broadcast Line
    ┌────────────┐ ─────────────────┬───────────────┬───────────────┐       
    │Master Board│   Settings       ▼               ▼               ▼       
    │┌──────────┐│     Data     Slave Node      Slave Node      Slave Node  
    ││          ││            ┌────────────┐  ┌────────────┐  ┌────────────┐
    ││ ESP32-S3 ││            │Slave  Board│  │Slave  Board│  │Slave  Board│
    ││  N16R8+  ││            │┌──────────┐│  │┌──────────┐│  │┌──────────┐│
    ││   CAM    ││            ││          ││  ││          ││  ││          ││
    ││          ││            ││ ESP32-S3 ││  ││ ESP32-S3 ││  ││ ESP32-S3 ││
    │└──┬───────┘│            ││  N16R8+  ││  ││  N16R8+  ││  ││  N16R8+  ││
    │   │ SPI    │            ││  OV7725  ││  ││  OV7725  ││  ││  OV7725  ││
    │   ▼        │            ││          ││  ││          ││  ││          ││
    │┌──────────┐│            │└──┬───────┘│  │└──┬───────┘│  │└──┬───────┘│
    ││          ││            │   │ SPI    │  │   │ SPI    │  │   │ SPI    │
    ││          ││            │   ▼        │  │   ▼        │  │   ▼        │
    ││  ST7789  ││            │┌──────────┐│  │┌──────────┐│  │┌──────────┐│
    ││          ││            ││          ││  ││          ││  ││          ││
    ││          ││            ││          ││  ││          ││  ││          ││
    │└──────────┘│            ││  ST7789  ││  ││  ST7789  ││  ││  ST7789  ││
    └────────────┘            ││          ││  ││          ││  ││          ││
                              ││          ││  ││          ││  ││          ││
                              │└──────────┘│  │└──────────┘│  │└──────────┘│
                              └────────────┘  └────────────┘  └────────────┘
```

---

## 🛠️ Hardware Requirements

### 1. Master Node Components
*   Microcontroller: ESP32-S3 N16R8 (A developer board with an integrated camera slot is highly recommended. 8MB Flash variants are compatible if they feature OPI PSRAM).
*   Display: 1.54" ST7789 SPI TFT display (Alternative displays like the round GC9A01 work. Screens exceeding 240x240 will require alternative frame sizes assigned in code).
*   Sensor: OV7725 22-pin ribbon connector camera module with its internal IR-cut filter physically removed.
*   Peripherals: 
    *   20k Ohm Potentiometer (for on-the-fly exposure and gain control)
    *   Push-Button (for toggling setting menus)

### 2. Slave Node Components (Per Node)
*   Microcontroller: ESP32-S3 N16R8
*   Display: 1.54" ST7789 SPI TFT display
*   Sensor: OV7725 22-pin ribbon camera (IR-cut filter removed)

Note: 24-pin OV7725 variants will require structural pin remapping. While the OV2640 sensor functions correctly, it cannot achieve the light-sensitivity performance metrics offered by the OV7725.

---

## 🔌 Pinout Configuration

### Display Connections (ST7789)
All nodes share the identical display pin-out array. 

| Display Pin | ESP32-S3 GPIO | Notes |
| :--- | :--- | :--- |
| MOSI | GPIO 19 | Also labeled as SDA |
| SCLK | GPIO 20 | Also labeled as SCL / CLK |
| CS | GPIO 21 | Chip Select |
| DC | GPIO 47 | Data / Command Selection |
| RST | GPIO 48 | Reset |
| BL | GPIO 45 | Backlight control. Set to -1 in software if your module lacks this pin. |

Warning: Modifying the display pin configurations blindly can create critical hardware resource conflicts with the parallel camera bus. Double-check your GPIO map before altering layouts.

### Controls & Communication Interconnect
| Peripheral | Source Pin | Target / Destination |
| :--- | :--- | :--- |
| Potentiometer Output | Wiper Pin | GPIO 1 (Must be supplied from onboard 5V rail) |
| Button | Standard Pin | GPIO 2 (Configured as Pull-Up; must short directly to GND) |
| UART Broadcast | GPIO 21 (TX) on Master | Connect to the RX pins of all configured Slave nodes |

---

## 💻 Software Setup & Flashing

[WARNING]
Most newer ESP32 Arduino Core board packages do not natively support the OV7725. This project has been only tested and found stable strictly on ESP32 Core v2.0.14. 

### 1. Preparing the **TFT_eSPI** Library
1. Install the TFT_eSPI library directly via the Arduino IDE Library Manager.
2. Locate your local Arduino library directory: `...Documents/Arduino/libraries/TFT_eSPI/User_Setups/`.
3. Copy the project's `customsetup.h` file and drop it into that `User_Setups` folder.
4. Open User_Setup_Select.h located in the root of the TFT_eSPI directory.
5. Comment out the default configuration line:
   `// #include <User_Setup.h>`
6. Add the include line pointing to your custom file:
   `#include <User_Setups/customsetup.h>`

### 2. IDE Compilation Parameters
Ensure your Arduino IDE settings match the target specs below before compiling and flashing your respective Master.ino and Slave.ino files to the boards:

*   Board: ESP32S3 Dev Module
*   USB CDC On Boot: "Enabled
*   Flash Size: 16MB (128Mb)
*   Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
*   PSRAM: OPI PSRAM (QSPI can work if your board supports it)
*   Flash Mode: QIO 120 MHz
*   CPU Frequency: 240 MHz

---

## 🔬 Technical Architecture: DMA Frame Buffer to TFT Pipeline

The high-speed camera rendering engine hinges entirely on a single optimized pipeline operation within the main execution loop:

```cpp
tft.pushImage(0, 0, 240, 240, (uint16_t *)fb->buf);
```

This acts as a dedicated hardware-accelerated memory bridge uniting two entirely isolated systems: The ESP32-S3 Camera Peripheral (DMA) and the Display Controller (SPI Bus).

### 1. Data Structure: The RGB565 Pixel Format
To circumvent heavy microcontroller memory overhead, the camera subsystem records image frames using PIXFORMAT_RGB565. 

Instead of deploying heavy 24-bit true color profiles (8 bits per channel), RGB565 packs a pixel cleanly into 16 bits (2 bytes):
*   Red: 5 bits
*   Green: 6 bits (Granted an extra bit since human eyes are significantly more sensitive to variations in green illumination)
*   Blue: 5 bits

#### Footprint Calculations:
Total Frame Array = 240 * 240 = 57,600 Pixels
Memory Allocation = 57,600 Pixels * 2 Bytes = 115,200 Bytes

### 2. The Step-by-Step Data Pipeline

#### Step 1: Direct Memory Access (DMA) Capture
When esp_camera_fb_get() handles a capture loop, the ESP32-S3's hardware camera peripheral streams raw pixel structures over its dedicated 8-bit parallel bus (Y2 through Y9). The timing sync is maintained entirely using the Pixel Clock (PCLK) and Vertical Sync (VSYNC) pins. 

The arriving stream gets pushed straight into local memory (CAMERA_FB_IN_DRAM) via Direct Memory Access (DMA). This bypasses the main CPU execution unit entirely, ensuring smooth background processing. Once a complete frame finishes loading, the pointer fb->buf is exposed, holding the start address of the 115,200-byte structure.

#### Step 2: 16-Bit Pointer Casting
The low-level camera stack reads memory natively as discrete 8-bit chunks (uint8_t *). However, display controllers digest graphics as solid 16-bit pixel entries. 

Applying the cast (uint16_t *)fb->buf shifts the evaluation stride. This forces the microcontroller compile routine to parse memory spaces in 2-byte jumps, parsing a correct, unbroken color block every cycle.

#### Step 3: Address Window Definition
When tft.pushImage(...) triggers, the library pushes configuration registers directly to the display controller IC (e.g., ST7789):
*   CASET (Column Address Set): Constrains the horizontal rendering boundary limits from X-coordinate 0 to 239.
*   RASET (Row Address Set): Constrains the vertical rendering boundary limits from Y-coordinate 0 to 239.

This config shifts the display driver's onboard draw coordinate indexing down to the origin point (0,0). The panel now locks into a continuous automated line-fill state.

#### Step 4: High-Speed Bus Transmission
With parameters fixed, the TFT_eSPI driver pushes pixel elements row-by-row out across the hardware interface lines. 
*   SPI Configuration: The data streams sequentially out over the single Master-Out-Slave-In (MOSI) trace line, requiring 16 clock cycles to complete a single pixel print.
*   Parallel Configuration (Optional): The data splits across 8 separate hardware data paths simultaneously, pulsing full byte combinations concurrently and effectively halving transmission overhead.

The internal graphic controller renders pixels left-to-right. When it hits the edge limit at column 239, it performs an automatic line-break jump down to index the next line until the window updates completely.

---

## ⚡ Troubleshooting & Performance Optimization

Note: Color Inversion / The Endianness Mismatch:
Microcontrollers and distinct display IC boards frequently utilize inverted Byte Orders (Big-Endian vs. Little-Endian formats). If your target screen image feeds inverse, unnatural palettes (e.g., skin tones rendering as fluorescent blue tones), your pixel high/low bytes are swapped. 

You can correct this without any CPU overhead by adding this initialization configuration parameter directly into your setup() loop:
tft.setSwapBytes(true);

### Hardware Latency Profile Benchmarking
The internal software logic employs native hardware timers (micros()) to benchmark pipeline processing efficiency:
```cpp
unsigned long start = micros();
tft.pushImage(0, 0, 240, 240, (uint16_t *)fb->buf);
unsigned long latency = micros() - start;
```
This latency window details the true real-time cost of executing display window allocations and manual bus shifts. Running higher SPI clocks (such as 40MHz or 80MHz) within your user-setup definitions will actively drive down latency figures, allowing for a higher frames-per-second (FPS) output.
