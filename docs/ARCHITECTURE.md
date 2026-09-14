# Architecture

## Core Distribution

### Core 0

- TinyUSB
- UVC Streaming
- Status Screen Management

### Core 1

- MLX90640 Acquisition
- Temperature Processing
- Frame Production

---

## Frame Buffer Ownership

| State | Description |
|---------|-------------|
| 0 | Free |
| 1 | Producer Owns |
| 2 | Ready |
| 3 | USB Owns |

---

## Processing Pipeline

MLX90640
    ↓
Temperature Acquisition
    ↓
Validation
    ↓
Range Scaling
    ↓
Palette Mapping
    ↓
Interpolation
    ↓
YUY2 Frame
    ↓
USB UVC

---

## Synchronization

Producer / Consumer model between
RP2040 Core 1 and Core 0.

Synchronization uses:

- __dmb()
- __sev()
- __wfe()

to coordinate frame ownership transitions.
