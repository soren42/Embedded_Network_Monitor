# Luckfox Pico Build
You are being given indirect/remote access to a Luckfox Pico 86 Smart Panel, Model 1408, via ssh.  This unit is based on the Rockchip RV1106 G2/G3 chip. RV1106 is a highly integrated IPC (Intelligent Video Processor) vision processing SoC specifically designed for artificial intelligence applications. It is built on a single-core ARM Cortex-A7 32-bit core, integrated with NEON and FPU, and features a built-in NPU supporting mixed-precision operations of INT4/INT8/INT16, with computing capabilities of up to 0.5TOPs/1TOPs. The embedded 16-bit DRAM DDR3L maintains demanding memory bandwidth, while the integrated POR (Power-on Reset), audio codec, and MAC PHY further enhance its functionality.

**Luckfox-Pico-86-Panel-1408**
Address: 10.3.38.19
User: root
Password: luckfox 
OS: buildroot gd333950dc 2023.02.6

You are going to develop and deploy (using this system as your development, cross-compilation; and staging server) a comprehensive network monitoring system that takes advantage of the the system’s screen and touch panel interface. This application must be written in ANSI C, due to compatibility with the SDK, and the resource limitations of the target system.

First, create a plan for the software. This should include the overall navigation architecture, monitoring facilities, and code optimizations needed to operate within the embedded system’s limitations.

Then, identify the packages, tools, and libraries needed to build an application for this system on this machine.  While both utilize ARM CPUs, the Luckfox’s Cortex A7 is an armv7l 32-bit only architecture will require the installation of a cross-compiler, SDK, and toolchain.

Next, design the UI for the application, taking the best possible advantage of the Luckfox’s 720x720 touch display.  It should be attractive, clean, intuitive, and easy to get an overall status at a single glance, while still allowing for detail in-depth when drilling down into the displayed information using the touchscreen.

Once you’re set, obtain, install, and configure the dependencies, then create the application code.  Build and test the code, then deploy it to the target system.  Perform further testing on the production application to ensure functionality, completeness, and ensure it is free from defects.  Ensure that the application starts on boot, and that nothing else uses resources on the IoT device. (Including the demo application bundled with it.)

Finally, document the application.  This should include three separate documents: an end user manual, a maintenance and troubleshooting manual for system admins, and finally, a code standards and interfaces manual for potential contributors.



