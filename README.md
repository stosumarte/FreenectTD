# FreenectTD
FreenectTD is an open source TouchDesigner implementation of the [libfreenect](https://github.com/OpenKinect/libfreenect) C++ wrapper, aimed at macOS users who don't have a way to use the official Kinect OPs in TouchDesigner.

## Requirements
* Apple Silicon
* macOS 12.4+
* TouchDesigner 2023+ (any license)
* Kinect v1 (Xbox 360 / Windows)

## Installing

### Option 1 — Global Installation (Recommended)

1. [Download the latest build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Unzip and copy `FreenectTD.plugin` to TouchDesigner's plugin folder, which is located at `/Users/<username>/Library/Application Support/Derivative/TouchDesigner099/Plugins`. You might need to show hidden files by pressing `⌘⇧.`.

Now, you should be able to run TouchDesigner and find FreenectTOP under the "Custom" OPs panel.

⚠️ macOS may prevent the plugin from loading after download due to security quarantine flags. To fix this, open Terminal and type (without running):

`xattr -d -r com.apple.quarantine ~/Library/Application\ Support/Derivative/TouchDesigner099/Plugins/FreenectTD.plugin`

### Option 2 — Project-specific Installation

1. [Download the latest build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Unzip and copy `FreenectTD.plugin` next to your .toe, in a folder named `Plugins`.

You should now be able to open your .toe and find FreenectTOP under the "Custom" OPs panel.

### Option 3 — CPlusPlus TOP

1. [Download the latest build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Unzip and copy `FreenectTD.plugin` wherever you want on your machine.

3. Add a CPlusPlus TOP to your network, and select `FreenectTD.plugin` under "Plugin Path" in the "Load" tab.

## Usage
By default, FreenectTOP outputs RGB data. To get a depth map, you must use a Render Select TOP and reference index 1 instead of index 0.

### Examples
Example .toe project files are provided in this repository, under the "toe_examples" directory.

<img width="872" alt="image" src="https://github.com/user-attachments/assets/f23b02f7-640a-4a77-b22b-a8668c5dd161" />

### Known limitations

* Only one Kinect device per machine is supported.

* Skeleton tracking is currently impossible to implement.

## FAQ
### Which features does FreenectTD support?
Currently, RGB and Depth image streaming are supported, as well as tilt control from the TOP's properties panel. No more features (such as microphone or LED control) are currently planned to be implemented.

### Which devices does FreenectTD support?
Only 1st gen Kinects are supported. Model numbers are 1414, 1473 and 1517.

![Kinect for Xbox 360 (1414/1473)](https://github.com/user-attachments/assets/b2e3090d-9e72-45d2-9e9c-8439cfc2b3a8)
![Kinect for Windows (1517)](https://github.com/user-attachments/assets/cb58beb9-3e5e-49be-8a4a-f5074fd8f723)

FreenectTD has currently been tested only with a 1414 model (Xbox 360), but it should work regardless since all these models should be compatible with libfreenect.


### Will this ever work on Kinect V2?
It's in the works, but currently there's no ETA for it.
This is a side project, and it requires lots of time and money to maintain (I've already fried two Kinects in the process of modding them to work on USB).

Please consider donating!

## Credits
A very big thank you goes to libfreenect developers, who made a great work by creating a way to unofficially interface with Kinect devices on macOS and Linux.

This project contains code from both libfreenect and libusb, licensed under GPL2.

## Donations
If you like FreenectTD, please consider donating to support further development!
[You can donate here via PayPal.](https://www.paypal.com/donate/?hosted_button_id=PZXS4BCQJ9QMQ "You can donate here via PayPal.")
