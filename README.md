# ESP32 LCD library for Open eXtensible Rack System projects

Library to show status information and port animation for OXRS based devices.

Library is written to support a ST7789 based 240x240 pixel display.

requires TFT_eSPI by Bodmer to be installed and configured for the target MCU.

Example settings required for RACK32 (...\libraries\TFT_eSPI\User_Setup.h)

	#define ST7789_2_DRIVER 
	#define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
	#define TFT_WIDTH  240 // ST7789 240 x 240
	#define TFT_HEIGHT 240 // ST7789 240 x 240

	#define TFT_MOSI 23
	#define TFT_SCLK 18
	#define TFT_CS   25
	#define TFT_DC    2
	#define TFT_RST   4
	#define TFT_BL   14

	#define SPI_FREQUENCY  40000000

 
