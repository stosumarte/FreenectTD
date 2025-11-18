# FreenectTD
FreenectTD is an open-source TouchDesigner plugin aimed at macOS users who don't have a way to use the official Kinect OPs in TouchDesigner.

It leverages [libfreenect](https://github.com/OpenKinect/libfreenect) and [libfreenect2](https://github.com/OpenKinect/libfreenect2) to implement support for Kinect cameras.

**⚠️ Warning:** 
FreenectTD is an experimental project. While being thoroughly tested and confirmed to work on multiple platforms, it may still have some bugs or stability issues. Please be careful if using in a production environment. I don't take any responsibility.

### Requirements
* Apple Silicon Mac
* macOS 12.4+ (Monterey)
* TouchDesigner 2023+ (any license)
* Kinect V1 / Kinect V2

### Supported features
* RGB streaming
* Depth map streaming
* Point cloud map streaming (V2 only)
* IR streaming (currently V2 only)
* Tilt control (V1 only)
* Depth undistortion (V2 only)
* Depth registration (align depth map to color)
* Manual depth range threshold

## [RECOMMENDED] Installing using installer

1. [Download the latest installer build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Right click on `FreenectTOP_[version]_Installer.pkg` and select "Open"

3. If the Installer gets blocked from running, go to `System Settings > Privacy & Security` and click on `Run Anyway`

You should now find FreenectTOP under the "Custom" OPs panel.

## Installing Manually

### Global Installation

1. [Download the latest zip build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Unzip and copy `FreenectTOP.plugin` to TouchDesigner's plugin folder, which is located at `/Users/<username>/Library/Application Support/Derivative/TouchDesigner099/Plugins`. You might need to show hidden files by pressing `⌘⇧.`.

You should now find FreenectTOP under the "Custom" OPs panel.

### Project-specific Installation

1. [Download the latest zip build from the releases tab](https://github.com/stosumarte/FreenectTD/releases/latest) 

2. Unzip and copy `FreenectTOP.plugin` next to your .toe, in a folder named `Plugins`.

You should now be able to open your .toe and find FreenectTOP under the "Custom" OPs panel.

## Usage
By default, FreenectTOP outputs RGB data. To get other streams, you must use Render Select TOPs and reference indexes 1 (for a depth map), 2 (for a point cloud map) and 3 (for IR stream).

### Examples
Example .toe project files are provided in this repository, under the `toe_examples` directory.

### Known limitations

* Only one Kinect device per machine is supported.
* Only one FreenectTD OP can be active at a time.
* Skeleton tracking is currently impossible to implement.

## Uninstalling

To uninstall all FreenectTD related files, run the following command in terminal:
`sudo rm -rf ~/Library/Application\ Support/Derivative/TouchDesigner099/Plugins/Freenect*.plugin`

## Donations
If you like FreenectTD, please consider donating to support further development!

<a href='https://ko-fi.com/H2H318LODL' target='_blank'>
<img width='180' style='border:0px;height:auto;' src='https://storage.ko-fi.com/cdn/kofi4.png?v=6' border='0' alt='Buy Me a Coffee at ko-fi.com' />
</a>
</br>
<a href="https://www.paypal.com/donate/?hosted_button_id=PZXS4BCQJ9QMQ" target="_blank" style="text-decoration:none!important;">
<img width="180" alt="Donate with PayPal" src="https://github.com/user-attachments/assets/ff3b7328-7b8b-4a8a-b777-3c61b5d2a1bc" border="0" />
</a>

## Credits

A very big thank you goes to the OpenKinect project, who developed the libraries that made this plugin possible.

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

By downloading, using, modifying, or distributing this plugin, either as source-code or binary format, you agree to comply with the terms of both the **LGPL-2.1** license for the plugin itself and the respective licenses of the third-party libraries included.
