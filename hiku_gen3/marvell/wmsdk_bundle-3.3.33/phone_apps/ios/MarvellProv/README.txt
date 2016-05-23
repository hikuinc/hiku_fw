-----------------------------------------------------------------
Marvell Provisioning app for sniffer provisioning for iOS platform
-----------------------------------------------------------------

Introduction:
-------------
This README lists the steps necessary to build and load the Marvell
Provisioning application on all iOS devices viz. iPhone, iPAD, iPod.

Prerequisites:
--------------

Development environment:

1. A Mac computer running OS X 10.7 (Lion) or later
2. Xcode IDE
3. iOS SDK
4. Enrollment to the iOS Development Program 

Some information about setting up the development environment is given
at the end of this README.

Target environment:

1. Any Apple device having iOS 7.0 or later versions.

Testing the application:
------------------------

Steps to run the app on Simulator:

1. Double-click on Marvell.xcodeproj to open it in Xcode.
2. Choose an appropriate iOS device from the Scheme pop-up menu in the
   Xcode toolbar for e.g iPhone Retina (4-inch) 
3. Click the "Run" button, located on top-left corner of the
   Xcode toolbar, which will build, compile and launch the simulator.

When running app for the first time, Xcode will ask whether you would
like to enable developer mode on your mac.
Developer mode allows Xcode to certain debugging features which helps
in understanding the execution of app.

Steps to run the app on iOS device:

1. Connect your iOS device to Mac computer through USB
2. Double-click on Marvell.xcodeproj to open it in Xcode.
3. Choose iOS Device with your device name from the Scheme pop-up menu
   in the Xcode toolbar
4. To add provisioning profile of the physical iOS device connected 
   - Click on 'Show the Project Navigator' located below 'run' button
     and select our project 'Marvell'
   - In the editor area of the workspace window, Xcode displays the
     project editor. Go to build settings in that.
   - Under 'Code Signing', expand 'Provisioning Profile'
   - Choose newly created provisioning profile for debug and for release
5. Click the 'run' button, located on top-left corner of the Xcode
   toolbar, which will build, compile and launch the app on iOS device

Steps to create URL to download the app on iOS Device:

1. Disconnect your iOS device from Mac Computer
2. Choose iOS Device with your device name from the Scheme pop-up menu
   in the Xcode toolbar
3. Clean the project. Product->Clean
4. Build the app Product->Build
5. Click on 'Show the Project Navigator', select Provisioning.app
   located at the bottom of the hierarchy
6. Right click on Provisioning.app and locate in using 'Show in Finder'
7. Compress the Provisioning.app file
8. Go to http://www.diawi.com/, drag the compressed file and click send
9. Install the app on iOS device using the URL provided.

Note: Downloaded app will work only on devices which are added in
provisioning profile.

Customization for the app
-------------------------

1. To modify Marvell device uAP name or status messages,
   go to 'Marvell->Views&Controllers->StartProvision->MessageList.h'
2. To change app logo or other images, go to 'Marvell->Graphics
   Resources' and replace the images with new ones but keep name and
   dimensions unchanged.

Appendix:
---------
Xcode is Apple's integrated development environment (IDE) which
includes a source editor, a graphical user interface editor and many
other features to build, compile and run apps on simulators as well as
devices connected to Mac
To download latest version of Xcode:
1. Open the App Store app on your Mac
2. In the search field in the top-right corner, type Xcode and press
   the Return key
3. Click free
OR
Follow https://developer.apple.com/xcode/index.php link
Xcode is downloaded into your /Application directory

iOS Development Program
To run development code on physical device, follow the steps mentioned
in the following link
http://mobile.tutsplus.com/tutorials/iphone/how-to-test-your-apps-on-physical-ios-devices/

