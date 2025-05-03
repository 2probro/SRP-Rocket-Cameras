# ESP32 Rocket Camera System

## Overview

This project uses two ESP32-Cam AI Thinker boards equipped with wide-angle lenses to capture onboard video during a model rocket flight. One board can optionally record audio via a GY-MAX4466 microphone, enabled by a configuration variable. The camera records at 15 fps. Video (and audio, if enabled) frames are saved as JPEG files on a FAT32-formatted MicroSD card within organized, timestamped folders to prevent overwriting. The boards operate independently from the flight computer (Kolibri).

## Hardware Components

- Two ESP32-Cam AI Thinker modules with wide-angle lenses.
- Audio recording via a GY-MAX4466 microphone on 1 board.
- Powered by the onboard battery through a step-down converter to 5V.
- Independent from the actual flight computer to ensure standalone operation.
- FAT32-formatted MicroSD card for storing image (and audio) files.

## Software Setup

1. Format the MicroSD card to FAT32.
2. Use the Arduino IDE or PlatformIO in VSCode
3. Use the webserver example sketch to determine optimal camera settings.
4. Upload the ACTUAL code to the ESP32-Cam boards.
5. Test the setup on bench:
   - Insert a FAT32-formatted MicroSD card into the slot.
   - Connect a USB-to-TTL adapter: TX → U0RXD (GPIO3), RX → U0TXD (GPIO1), 5V → 5V, GND → GND.
   - OR connect using the provided esp32 programming board.
   - Upload the ACTUAL code to the ESP32-Cam boards.
   - Open Serial Monitor at 115200 baud.
   - Press Reset; you should see "Waiting for launch (deep sleep)...".
   - To simulate launch, connect BREAK_PIN (GPIO13) to GND with a jumper wire.
   - You'll then see "Launch detected! Starting recording..." and frames being captured.
   - Eject the SD card and inspect the timestamped session folder for JPEG images.
6. Prepare for actual launch:
   - Upload the ACTUAL code to the ESP32-Cam boards, NO TEST CODE, MAKE SURE BOTH BOARDS HAVE DIFFERENT CODE.
   - Power the board with the stepdown converter.
   - Ensure a FAT32-formatted MicroSD card is inserted.
   - Mount the ESP32-CAM module securely in the electronics stack, ensure all wires are secured.
   - Connect the break-wire between the BREAK_PIN (GPIO13) and GND; at liftoff, it will break and wake the ESP32.

## Post-Processing Recorded Video (using FFmpeg)

The ESP32-CAM saves video as a raw MJPEG stream (a sequence of JPEG images) in a file named `video.MJPEG`. This raw stream lacks timing information and a standard container, so media players like VLC might only show the first frame or play it at the wrong speed.

To convert this raw stream into a standard, playable video file (like AVI) with the correct frame rate, you can use the free and powerful command-line tool `ffmpeg`.

**1. Download FFmpeg:**

- Get a Windows build from the official site or recommended providers:
  - [FFmpeg Official Download Page](https://ffmpeg.org/download.html)
  - [gyan.dev Builds (Recommended)](https://www.gyan.dev/ffmpeg/builds/)
  - [BtbN Builds](https://github.com/BtbN/FFmpeg-Builds/releases)
- Download a static build (e.g., `ffmpeg-release-full-static.7z`) and extract the archive using a tool like 7-Zip.

**2. Run the Conversion Command:**

- Open Command Prompt (cmd.exe) or PowerShell.
- **Navigate to the directory containing your `video.MJPEG` file** using the `cd` command. For example:
  ```cmd
  cd E:\1265
  ```
  (Replace `E:\1265` with the actual path to the folder containing your video file from the SD card).
- Run the `ffmpeg` command, providing the **full path to the `ffmpeg.exe` executable** (located inside the `bin` folder of the extracted archive). You also need to specify the **input framerate** (which should match the `FRAME_INTERVAL` target, e.g., 15 fps):

  ```cmd
  "C:\path\to\your\extracted\ffmpeg\bin\ffmpeg.exe" -framerate 15 -f mjpeg -i video.MJPEG -c copy video_output.avi
  ```

  - **Replace `"C:\path\to\your\extracted\ffmpeg\bin\ffmpeg.exe"`** with the actual path where you extracted ffmpeg. You can get this path by navigating to the `bin` folder in File Explorer, holding `Shift`, right-clicking `ffmpeg.exe`, and selecting "Copy as path".
  - `-framerate 15`: Tells ffmpeg the input stream has 15 frames per second. **Adjust this value** if you change the target `FRAME_INTERVAL` in the Arduino code.
  - `-f mjpeg`: Specifies the input **f**ormat is raw MJPEG.
  - `-i video.MJPEG`: Specifies the **i**nput file name.
  - `-c copy`: **C**opies the video stream without re-encoding, preserving quality and ensuring speed.
  - `video_output.avi`: The name of the playable output file that will be created.

**3. Play the Output:**

- After the command completes, you will find `video_output.avi` in the same directory. This file should play correctly in VLC or other media players at the intended speed.

**Alternative (Adding FFmpeg to PATH):** For easier use, you can add the `bin` directory of your extracted FFmpeg folder to your Windows System PATH environment variable. This allows you to just type `ffmpeg` instead of the full path in the command prompt, regardless of your current directory. Search Windows settings for "Edit the system environment variables" to find this option.

## System Configuration

- Configure camera pinouts and parameters (resolution, frame size, quality).
- Initialize the camera and set up a frame buffer.
- Initialize the MicroSD card and mount it once in `setup()`.
- Keep the SD card mounted for the duration of the flight.

## File and Directory Management

- Create unique directories for each session or board (e.g., timestamped or board-specific folders).
- Generate sequential or timestamped filenames to prevent overwriting.

## Storage Strategy

**Option A: Separate JPEG Images**

- For each frame, build a unique filename and open a new file (e.g., `SD.open(name, FILE_WRITE)`).
- Write the JPEG buffer, then call `file.close()` to commit and free resources.

**Option B: MJPEG Container**

- Open a single file (e.g., `VIDEO.MJPEG`) once in `setup()`.
- Append each JPEG buffer directly (`file.write(buf, len)`).
- Close the file after all frames are written.

_Note:_ At 15 fps, per-frame open/close overhead can lead to dropped frames; the MJPEG approach minimizes FAT updates.

## Launch Detection & Power Management

- Define a break wire pin (pulled HIGH, goes LOW on liftoff) as the ESP32 wake-up source.
- Enter deep sleep in `setup()` until launch is detected.
- Upon wake-up, flash the onboard LED twice to signal power-up.
- Start the recording timer for a fixed duration (e.g., 2 minutes).
- After recording, close files and optionally return to a low-power state.

## Code Logic Requirements

- Select board mode via a configuration variable to enable or disable audio recording, uploading different code to both boards.
- Define and configure camera pinouts and parameters.
- Initialize hardware and handle errors for camera, SD mounting, file operations, and directory creation.
- Capture image frames and save to the SD card.
- Optionally record audio alongside images.
- Implement deep sleep and wake-up logic.

## Operation Flow

1. Define and configure hardware (camera, microphone, power).
2. Initialize camera, frame buffer, and SD card.
3. Wait in deep sleep for launch detection.
4. On wake-up, signal power-up then record for the specified duration.
5. Save files and return to low-power mode.

## IMPORTANT NOTES

- SD card AND camera use some of the GPIOs, so don't use those pins for anything else. You can see which ones by looking at the esp32cam-pinout image in the repo.
- SD/MMC Bus Modes & Pin Usage:

  - By default the ESP32-CAM's SD_MMC library mounts in **4-bit SDMMC mode** (`SD_MMC.begin()`), which consumes **GPIO2, 4, 12, 13 (DATA0–3)** plus **GPIO14 (CLK)** and **GPIO15 (CMD)**. That leaves **no** free RTC‐GPIO pin for deep‐sleep wakeup (EXT0).
  - We instead mount in **1-bit SDMMC mode** (`SD_MMC.begin("/sdcard", true)`), using only **DATA0 (GPIO2)**, and freeing up:
    - **GPIO4** → used for the on-board flash LED
    - **GPIO12** → used for the break-wire (EXT0 wake source, active LOW)
    - **GPIO13** → available as general-purpose I/O if needed
  - SPI‐SD mode is a 3rd option (wire card to VSPI and call `SD.begin(CS)`), but our 1-bit approach keeps wiring simple.

- Break-Wire Pin Choice:

  - We attach the launch-detection break-wire to **GPIO12** because it is an RTC‐GPIO that can wake the ESP32 from deep sleep via EXT0 when it sees a LOW edge.
  - The pin is pulled HIGH in `setup()` (`pinMode(BREAK_PIN, INPUT_PULLUP)`), so the liftoff break to GND triggers the wakeup.

- Flash LED on GPIO4:
  - After switching to 1-bit SD, **GPIO4** is free (SD no longer drives DATA1), so we restore it to drive the on-board flashlight LED as a status indicator.

## Unanswered Questions

- How to setup the audio recording?
- Will the stepdown converter handle the current draw of the 2 ESP32-CAMs and the microphone?
- Is the voltage stable enough for the ESP32-CAMs and the microphone throughout the flight and idle in between launches?

## Links

- Microphone GY-MAX4466: https://nl.aliexpress.com/item/1005004019287254.html
- ESP32-CAM Documentation: https://docs.sunfounder.com/projects/galaxy-rvr/en/latest/hardware/cpn_esp_32_cam.html
- ESP32-CAM MicroSD: https://dronebotworkshop.com/esp32-cam-microsd/
- VERY GOOD TUTORIAL FOR AUDIO INPUT: https://www.youtube.com/watch?v=pPh3_ciEmzs
- ESP32-CAM WEBSERVER EXAMPLE SKETCH: https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Camera/CameraWebServer
