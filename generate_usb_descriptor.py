#!/usr/bin/env python3
# NOTE - This doesn't actually work.   Currently is just using the default from the TinyUSB example
from pathlib import Path
import re,sys
src=Path(sys.argv[1]).read_text();out=Path(sys.argv[2])
src=re.sub(r'#define USB_VID\s+0x[0-9A-Fa-f]+','#define USB_VID 0x2E8A',src)
src=re.sub(r'#define USB_PID\s+[^\n]+','#define USB_PID 0x0FFF',src)
src=src.replace('"TinyUSB Device"','"MLX90640 Thermal Camera"')
# TinyUSB example obtains dimensions/rate through usb_descriptors.h.
out.write_text(src)
