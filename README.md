# FreenectTD
FreenectTD is an open source TouchDesigner implementation of libfreenect's C++ wrapper, aimed at macOS users who don't have a way to use Kinect devices.

## Installing
FreenectTD relies on libfreenect and libusb to run. 
**You must install them with brew before continuing.**
If you don't have brew, run this command to install it:
`/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`

Once brew is installed, you can install libfreenect and libusb by running `brew install libfreenect libusb`.

Copy `FreenectTD.plugin` to TouchDesigner's plugin folder, which is located at `/Users/<username>/Library/Application Support/Derivative/TouchDesigner099/Plugins`.

Now, you should be able to run TouchDesigner and find FreenectTOP under the "Custom" OPs panel.

## Usage
By default, FreenectTOP outputs RGB data. To get a depth map, you must use a Render Select TOP and reference index 1 instead of index 0.
