# FreenectTD
FreenectTD is an open source TouchDesigner implementation of libfreenect's C++ wrapper, aimed at macOS users who don't have a way to use Kinect v1 devices.

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

## Examples
Example .toe project files are provided in this repository, under the "toe_examples" directory.

<img width="872" alt="image" src="https://github.com/user-attachments/assets/f23b02f7-640a-4a77-b22b-a8668c5dd161" />

## FAQ
### Which features does FreenectTD support?
Currently, RGB and Depth image streaming are supported, as well as tilt control from the OP's properties panel. No more features are currently planned to be implemented.
### Which devices are supported?
Only 1st gen. (Xbox 360) Kinects are supported.
### Will this ever work on [insert later Kinect gen.]?
It's not planned, and it would require an almost complete rewrite since it would need to interface with libfreenect2. Feel free to fork this project and implement it on other versions, though.

## Credits
A very big thank you goes to libfreenect developers, who made a great work by creating a way to unofficially interface with Kinect devices on macOS and Linux.

## Donations
If you like FreenectTD, please consider donating to support further development!
[You can donate here via PayPal.](https://www.paypal.com/donate/?hosted_button_id=PZXS4BCQJ9QMQ "You can donate here via PayPal.")
