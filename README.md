# ESP32-S3-N16R8-OV7725-Quad-NVG
Affordable and simple ESP32-S3 Night Vision solution using ESP32-S3 N16R8 with an OV7725 camera and ST7789 1.54" TFT SPI display powered by TFT_eSPI library                                                                          
                                                                            
     Master  Node     UART                                                  
    ┌────────────┐ ─────────────────┬───────────────┬───────────────┐       
    │Master Board│  Settings        ▼               ▼               ▼       
    │┌──────────┐│    Data      Slave Node      Slave Node      Slave Node  
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

This project is an attempt at IR-illuminated/Lowlight night vision suitable for airsoft/educational/home surveillance usage that allows users to easily combine 2 or more nodes into an array for increased viewing angle and other usages. It is also possible to use a single node as a monocular.

To assemble a single master node you will need:
- ESP32-S3 N16R8 (camera slot highly suggested)                   (8Mb flash boards also may work, provided they have OPI PSRAM) 
- 1.54" ST7789 SPI TFT display                                    (other displays such as the GC9A01 will work. However displays with resolution above 240x240 will need to use a different framesize)
- OV7725 22 pin ribbon connector camera with IR filter removed    (24pin OV7725's will require pin remapping may need additional workarounds. OV2460 works fine but you won't be able to achieve the same results as with the OV7725)
- 220k (or similar resistance) Ohm potentiometer
- Button
