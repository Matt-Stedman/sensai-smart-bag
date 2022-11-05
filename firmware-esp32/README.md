# Subrepo - firmware-esp32

This repo contains all of the firmware for the ESP32 (and accelerometer).
This is a [platform.io project](https://platformio.org/), which is very similar to Arduino but is wayyy more scalable and powerful.

To get platformio on your computer:

1. [Download VSCode](https://code.visualstudio.com/Download) for your machine.
2. Install the [Platformio extension](https://platformio.org/install/ide?install=vscode) for VSCode. (You can do this in the extensions tab on the left of your VSCode window).
3. Once done, you will need to open VSCode within the `firmware-esp32` folder (i.e. not the top folder of the monorepo). VSCode should then automatically recognize this folder as a platformio project.

To use platformio to flash an ESP32:

4. Use the [Build and Run commands](https://docs.platformio.org/en/stable/_images/platformio-ide-vscode-build-project.png) to flash firmware
5. You can also use the [Serial Monitor](https://www.digikey.com/-/media/MakerIO/Images/blogs/2022/Get%20Started%20with%20PlatformIO%20for%20Your%20Next%20Arduino%20Project/buttons_6.jpg?la=en&ts=aa4d24a5-eff4-4895-a4e8-0e26b3895d05) to see the ESP32's output. 