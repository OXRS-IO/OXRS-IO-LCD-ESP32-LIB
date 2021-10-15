# ESP32 LCD library for [OXRS](https://oxrs.io)

See [here](https://oxrs.io/docs/libraries/esp32-lcd-library.html) for documentation.

### Prerequisites
Library is written to support a ST7789 based 240x240 pixel display.

Requires TFT_eSPI by Bodmer to be installed and configured for the target MCU.

Installation can be done with the Arduino Library Manager or download from Github : https://github.com/Bodmer/TFT_eSPI 

For your convenience an OXRS compatible configuration file "Setup000_RACK32_ST7789.h" is provided in the User_Setup folder of this library.
Instruction how to install and activate this setup can be found on the top of Setup000_RACK32_ST7789.h.




### Maker logo displayed on LCD

This lib has the ability to display a maker supplied logo in the top/left corner of the LCD.

The reserved space for a logo is 40 pixel high and 40 pixel wide at the top/left corner of the LCD.
* A larger logo will be cropped starting with the bottom/left corner of the .bmp.
* A smaller logo will be displayed starting at the bottom/left position of the reserved space.

#### Create your own logo
The first step to create your own logo is to prepare a 40x40 pixel 24-bit-bitmap file (.bmp). Use your choice of image processing tool. If you have an image file already from which you want to use an extract, you need to
* create/crop a quadratic extract of the desired content (width = height).
* change the image size to 40 x 40 pixel and save it as 24-bit-bitmap file named logo.bmp.

#### There are two different ways to deploy your logo
* logo from SPIFFS

To use this method upload the logo.bmp file to the SPIFFS of your target MCU.
If you are using the Arduino IDE, copy your logo.bmp to the data folder of your sketch and use `ESP32 Sketch Data Upload` from the tools menu to upload everything located in the data folder.

* logo embedded in FW binary

Using this method embeds your maker logo in the binary of your sketch. This guaranties the availability of your logo at runtime independent of the availability of the SPIFFS and without having to do the extra upload.
Following steps are required:

*
  * Convert the logo.bmp to a C header file.
You can use the convert.py script supplied in the tools folder of the lib. 
Type `convert.py -i logo.bmp -o logo.h -v FW_LOGO` to create the `logo.h` header file that contains the `logo.bmp` as an array stored in PROGMEM.
Any other method to convert the .bmp to an array can be used. Make sure the array is declared as `const uint8_t FW_LOGO[] PROGMEM = { ... };`
  * the `logo.h` file has to be copied into the sketch folder
  * `logo.h` has to be included in your FW.ino file `#include "logo.h"`

#### Logo selection at start up

At startup the LCD lib searches for a logo in the following order and stops after the first logo found
1. check if there is a valid `/logo.bmp` stored in SPIFFS
1. if 1. is not successful check if there is a `fwLogo reference` passed to the LCDlib by the `OXRS_lcd::draw_header()` member.
1. if 2. is not successful load the default `embedded OXRS_logo` from PROGMEM
 
  
