# FreenectTD
FreenectTD is an open-source TouchDesigner plugin aimed at macOS users who don't have a way to use the official Kinect OPs in TouchDesigner.

It leverages [libfreenect](https://github.com/OpenKinect/libfreenect) and [libfreenect2](https://github.com/OpenKinect/libfreenect2) to implement support for Kinect cameras.

## Requirements
* Apple Silicon Mac
* macOS 12.4+ (Monterey)
* TouchDesigner 2023+ (any license)
* Kinect V1 / Kinect V2 

## Installing

### [Recommended] Using installer

1. [Download the latest installer build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Right click on `FreenectTOP_[version]_Installer.pkg` and select "Open"

3. If the Installer gets blocked from running, go to `System Settings > Privacy & Security` and click on `Run Anyway`

You should now find FreenectTOP under the "Custom" OPs panel.

### Manual Installation
1. [Download the latest zip build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Unzip and copy `FreenectTOP.plugin` to TouchDesigner's plugin folder, which is located at `/Users/<username>/Library/Application Support/Derivative/TouchDesigner099/Plugins`. You might need to show hidden files by pressing `⌘⇧.`.

You should now find FreenectTOP under the "Custom" OPs panel.

### Project-specific Installation

1. [Download the latest zip build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Unzip and copy `FreenectTOP.plugin` next to your .toe, in a folder named `Plugins`.

You should now be able to open your .toe and find FreenectTOP under the "Custom" OPs panel.


### [Not recommended] CPlusPlus TOP

1. [Download the latest build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Unzip and copy `FreenectTOP.plugin` wherever you want on your machine.

3. Add a CPlusPlus TOP to your network, and select `FreenectTOP.plugin` under "Plugin Path" in the "Load" tab.

## Usage
By default, FreenectTOP outputs RGB data. To get a depth map, you must use a Render Select TOP and reference index 1.

### Examples
Example .toe project files are provided in this repository, under the `toe_examples` directory.

<img width="872" alt="image" src="https://github.com/user-attachments/assets/f23b02f7-640a-4a77-b22b-a8668c5dd161" />

### Known limitations

* Only one Kinect device per machine is supported.
* Only one FreenectTD OP can be active at a time.
* Skeleton tracking is currently impossible to implement.

## Support
Both Kinect V1 (Xbox 360) and Kinect V2 (Xbox One) are supported. Kinect for Windows devices of either generation are supported as well.

### Included features
* RGB streaming
* Depth map streaming
* Tilt control (V1 only)
* Depth undistortion (V2 only)

## Credits
A very big thank you goes to the OpenKinect project, who developed the libraries that made this plugin possible.

## Donations
If you like FreenectTD, please consider donating to support further development!
[You can donate here via PayPal.](https://www.paypal.com/donate/?hosted_button_id=PZXS4BCQJ9QMQ "You can donate here via PayPal.")

## Licensing

**FreenectTD** is licensed under the **GNU Lesser General Public License v2.1 (LGPL-2.1)**.  
This means you are free to use, modify, and distribute this plugin, including in closed-source applications, provided that any modifications to the plugin itself are released under the same LGPL v2.1 license.

This project also includes the following third-party libraries, each under their respective licenses:

- **libfreenect** – Apache 2.0 License  
  [Full License Text](https://raw.githubusercontent.com/OpenKinect/libfreenect/master/APACHE20)

- **libfreenect2** – Apache 2.0 License  
  [Full License Text](https://raw.githubusercontent.com/OpenKinect/libfreenect2/master/APACHE20)

- **libusb** – LGPL 2.1 License  
  [Full License Text](https://raw.githubusercontent.com/libusb/libusb/master/COPYING)

- **CImg** – CeCILL-C License  
  [Full License Text](https://cecill.info/licences/Licence_CeCILL-C_V1-en.txt)

By downloading, using, modifying, or distributing this plugin, either as source-code or binary format, you agree to comply with the terms of both the **LGPL-2.1** license for the plugin itself and the respective licenses of the third-party libraries included.
