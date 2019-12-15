----------------------
Getting started
----------------------

Welcome to the hiku open source project!

To get started you will need a hiku device and an Electric Imp account to load new firmware onto the device. 
Create an Electric Imp account if you don’t already have one, https://developer.electricimp.com/gettingstarted/explorer/account
Download the Electric Imp mobile app for iOS https://apps.apple.com/us/app/electric-imp/id547133856, or Android https://play.google.com/store/apps/details?id=com.electricimp.electricimp
Unlock your devices. Out of the box, hiku devices are tied to hiku production firmware, and need to be unlocked to load your own firmware (‘unblessed’ in Electric Imp parlance). To do that, please send your device serial numbers to rob at hiku dot us with the subject line ‘Device unlock request’. 

Your device serial number is located on the box label as ‘MAC address’. You can also find it in the hiku mobile app if you already connected hiku to Wi-Fi using the hiku mobile app. 

After your device is unlocked you are ready to load your own firmware. Sign into your Electric Imp account, and follow their instructions for loading new firmware onto a device using BlinkUp using the Electric Imp mobile app or SDK. 
https://impcentral.electricimp.com/login#

----------------------
Code overview
----------------------

The hiku device code base is split into the following repositories. 

https://github.com/hikuinc/hiku_fw, language: Squirrel 
Performs all device control and data flow. 
Runs on the Electric Imp platform in two parts, device and agent. The device code runs locally on the Electric Imp chipset, while the agent runs in the Electric Imp cloud.

https://github.com/hikuinc/hiku_stm32_fw, language: C
Performs data capture for voice and barcode scans. 
Runs natively on the STM32 microcontroller.
The hiku_stm32_fw binary is loaded onto the STM32 microntroller via device code in hiku_fw. 


----------------------
Learn more
----------------------

Slyce purchased the hiku mobile app and server software in October 2018. You can read more about the hiku server API here 
https://github.com/Slyce-Inc/hiku_shared


----------------------
MIT open-source license
----------------------

Copyright 2019 hiku labs, inc

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

