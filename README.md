# VapiKey - VapiCon 2025 Hardware Workshop

VapiKey is a hardware project that lets you talk to a VAPI AI assistant through a small physical device. It's got a mic, speaker, a tactical button and a little lcd screen. It also connects to your Wifi connection to communicate. You press the button, say a question, and the device responds with an answer.

## Hardware

These are the hardware components we used for this project

   - [AtomS3R](https://docs.m5stack.com/en/core/AtomS3R) or [Atomic Echo Base](https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base)
   - [Atomic Battery Base](https://shop.m5stack.com/products/atomic-battery-base-200mah?srsltid=AfmBOoqrr_zX2RVCksgfggWRR2F-8hTZ4asWs7_DuLn3MWEXH9JYPSqN)

## Getting Started

To get the project working, you need to:

1. [Install ESP-IDF on your machine](#install-esp-idf)
2. [Clone the repository and configure the settings: Wifi SSID, Password and your VAPI Bearer Token](#setup-project)
3. [Build the program and flash it onto the device](#building-the-project)

## Install ESP-IDF 

You will need v5.5.1 or later.

#### macOS/Linux

   ```bash
   # Install prerequisites (macOS)
   brew install cmake ninja dfu-util

   # Install prerequisites (Ubuntu/Debian)
   # sudo apt-get install git wget flex bison gperf python3 python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

   # Clone ESP-IDF repository
   mkdir -p ~/esp
   cd ~/esp
   git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git

   # Install ESP-IDF tools
   cd ~/esp/esp-idf
   ./install.sh esp32s3

   # Set up environment (run this each time you open a new terminal)
   source ~/esp/esp-idf/export.sh
   ```

#### Windows

   ```powershell
   # Download and run the ESP-IDF installer from:
   # https://dl.espressif.com/dl/esp-idf/

   # Or use the ESP-IDF command prompt installed by the installer
   # Then run:
   export.bat
   ```

#### Verify Installation

   ```bash
   idf.py --version
   # Should output something like: ESP-IDF v5.5.1
   ```

### Set up your Terminal (Required for each terminal session)

   ```bash
   # Add this to your ~/.zshrc to avoid running it manually:
   alias get_idf='. ~/esp/esp-idf/export.sh'

   # Open a new terminal to apply the changes

   # Then you can simply run:
   get_idf
   ```

**Full Documentation**: [Official ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

## Setup project

### 1. Clone Repo

```bash
git clone --recurse-submodules https://github.com/VapiAI/vapicon-2025-hardware-workshop.git
cd vapicon-2025-hardware-workshop
```

### 2. Configuration

Run the configuration menu to set up your WiFi credentials and Bearer Token:

```bash
idf.py menuconfig
```

Navigate to:

- `VapiCon 2025 Hardware Workshop Configuration`
- Set your **WiFi Name** and **WiFi Password**
- Set your **Bearer Token**. You can either setup your own VAPI assistant and copy the API key or use this value that we've already set up for you: `b3db41dc-f62e-4ca8-89dc-550eab564212`

Press `S` to save and `Q` to quit.

## Building the Project

To build the project, first build and then flash + monitor.

```bash
idf.py build #builds the program

idf.py flash monitor #flashes it onto the device and opens the serial monitor
```

To exit the monitor, press `Ctrl+]`.

## Troubleshooting

### Build Errors

If you encounter compiler warnings treated as errors:

1. **String truncation errors**: These may appear with GCC 13+ due to stricter checks

2. **Missing enum cases**: This typically happens when using a different ESP-IDF version
   - **Solution**: Ensure you're using ESP-IDF v5.5.1 as specified in the prerequisites
   - Check your current version: `idf.py --version`
   - If incorrect, reinstall ESP-IDF v5.5.1 following the installation steps above

### Missing Dependencies

If the build fails with missing `libpeer` files:

- Ensure submodules are properly initialized: `git submodule update --init --recursive`
- Check that `deps/libpeer/src/` directory contains source files

### Device Connection

If flashing fails:

- Check the USB connection
- Try different USB ports or cables
- Ensure the correct port is detected (it should auto-detect, or specify with `-p PORT`)

## Project Structure

```
vapicon-2025-hardware-workshop/
├── main/                  # Main application source
│   ├── http.h            # HTTP client for Vapi API
│   ├── webrtc.h          # WebRTC implementation
│   ├── wifi.h            # WiFi connection management
│   └── m5-atom-s3.h      # Hardware abstraction
├── deps/
│   └── libpeer/          # WebRTC library (git submodule)
├── components/
│   └── peer/             # Peer connection component
├── sdkconfig.defaults    # Default configuration values
└── partitions.csv        # Flash partition table
```

## Additional Resources

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [M5Stack AtomS3R Documentation](https://docs.m5stack.com/en/core/AtomS3R)
- [Vapi Documentation](https://docs.vapi.ai/)
