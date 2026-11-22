# EyeTest

A small settings and preview app for the PlayStation Eye camera on Windows, in the spirit of the old CL-Eye Test.

## Use it

1. The camera must use the **WinUSB** driver (install it with Zadig).
2. Run `EyeTest.exe`. Keep `libusb-1.0.dll` in the same folder.
3. Close OpenTrack first. Only one program can use the camera at a time.

The left side shows the live picture, the frame rate the camera is actually delivering, and how many frames were dropped. The right side has:

- **Camera:** which camera, resolution (640×480 or 320×240) and frame rate (up to 83 Hz at 640×480, 205 Hz at 320×240).
- **Exposure:** exposure, gain, auto gain.
- **Image:** brightness, contrast, saturation, hue, sharpness.
- **White balance:** auto, or manual red/green/blue.
- **Other:** horizontal/vertical flip, test pattern.
- **Presets:** save and load named settings. They're stored as text files in a `presets` folder next to the exe.

Settings apply live and are not saved in the camera. The camera goes back to its defaults when it is unplugged or another program opens it. To use the same values in OpenTrack, set them in PointTracker's camera settings.

## Build

Needs Visual Studio 2022 (or its Build Tools) and CMake 3.20+. SDL2 2.30.9 and Dear ImGui 1.91.5 download automatically at configure time.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The app is built to `build/bin/Release/`.

## Licence

EyeTest is released under **GPL-2.0-or-later** (see `LICENSE`), because it includes GPL-licensed PS3EYEDriver code. Third-party notices are in `THIRD-PARTY-NOTICES.txt`.

## Credits

- `third_party/ps3eye`: PS3EYEDriver (inspirit, as maintained by the OpenTrack project), with a fix to its USB error handling. See its `LICENSE`.
- `third_party/libusb`: libusb 1.0.30 (LGPL-2.1), used as a separate DLL.
- SDL2 (zlib licence) and Dear ImGui (MIT).
