# MLX90640 RP2040 UVC Thermal Camera

## Version 3.3

## Improvements
- Documentation updates



This firmware is verified against **Raspberry Pi Pico SDK 2.3.1** and **TinyUSB 0.21.0**. CMake rejects Pico SDK versions older than 2.3.1. Use the TinyUSB checkout integrated with that SDK or configure `PICO_TINYUSB_PATH` to a TinyUSB 0.21.0 checkout. Delete and recreate `build/` after changing either dependency.

## USB identity and licensing
## TEMPORARILY DISABLED 
The development descriptor uses VID `0x2E8A`, PID `0x0FFF`, product `MLX90640 Thermal Camera`, and the Pico unique board ID as the serial number. Raspberry Pi documents its VID and approved PID allocation process in the [Raspberry Pi USB PID repository](https://github.com/raspberrypi/usb-pid). An unlisted PID is not an allocation. Obtain Raspberry Pi approval before distributing a product with this VID/PID.

---

## Status screens

- `LOADING`: color-bar test background, shown for at least 10 seconds.
- `NO SENSOR`: black text on solid white.
- `RANGE/ERR`: black text on solid white when fewer than 75 percent of readings are valid.


---

## Build

```sh
git clone --branch 2.3.1 --recursive https://github.com/raspberrypi/pico-sdk.git
 git clone https://github.com/melexis/mlx90640-library.git lib/mlx90640-library
export PICO_SDK_PATH=/absolute/path/to/pico-sdk
git submodule update --init --recursive
cmake -S . -B build -DPICO_BOARD=pico -DCMAKE_C_COMPILER=/usr/bin/arm-none-eabi-gcc -DCMAKE_CXX_COMPILER=/usr/bin/arm-none-eabi-g++ -DCMAKE_ASM_COMPILER=/usr/bin/arm-none-eabi-gcc
cmake --build build -j

```

For a standalone TinyUSB 0.21.0 checkout, add `-DPICO_TINYUSB_PATH=/absolute/path/to/tinyusb` at CMake configure time.

---

## Melexis linkage issues changes

Melexis headers use C linkage in C++ files; the HAL includes the official driver declaration; `MLX90640_I2CGeneralReset()` returns `int`; subpage identity uses `MLX90640_GetSubPageNumber()`
Strict warnings and a linker map are enabled.

---

## Overview

V3.3 is built on the validated V3.2 firmware baseline.

No functional changes were introduced.

This release improves:

- Code readability
- Architecture notes
- Long-term maintainability

---

## Core Responsibilities

### Core 0

- TinyUSB
- UVC Streaming
- Status Screens

### Core 1

- MLX90640 Acquisition
- Thermal Processing
- Frame Generation

---

## Data Flow

MLX90640
    ↓
Temperature Frame
    ↓
Validation
    ↓
Scaling
    ↓
Palette Mapping
    ↓
Interpolation
    ↓
YUY2 Frame
    ↓
USB UVC

---

## Hardware Validation status

Inherited from V3.2:

- Firmware Build Success
- Startup Screen Verified
- Sensor Error Screen Verified
- Thermal Streaming Verified
- Hardware Tested

## TODO
- Add temperature reading (numeric) to display, not just heat map.
- Test burn-in to ensure device stays stable
- Plan for debugging?

---
