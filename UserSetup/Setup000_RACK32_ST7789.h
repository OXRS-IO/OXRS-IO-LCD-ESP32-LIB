//                            USER DEFINED SETTINGS
//   Set driver type, fonts to be loaded, pins used and SPI control method etc
//
//   See the User_Setup_Select.h file if you wish to be able to define multiple
//   setups and then easily select which setup file is used by the compiler.
//
//	 Setup for the OXRS system based on the RACK32 ESP32 MCU and a ST7789 240x240 pixel display
//	 perform the following steps to make this setup active 
//		copy this file to ...\libraries\TFT_eSPI\User_Setups\
//		edit ...\libraries\TFT_eSPI\User_Setup_Select.h 
//			comment out the NOT COMMENTED OUT #include ...
//			add the following #include to the list of user selects (leave it uncommented!)
//			#include <User_Setups/Setup000_RACK32_ST7789.h>  // Setup file configured for OXRS RACK32 with ST7789
//		save the changed file




// ##################################################################################
//
// Section 1. Call up the right driver file and any options for it
//
// ##################################################################################

#define ST7789_2_DRIVER    		// Minimal configuration option, define additional parameters below for this display

#define TFT_RGB_ORDER TFT_RGB  	// Colour order Red-Green-Blue

#define TFT_WIDTH  240 			// ST7789 240 x 240 and 240 x 320
#define TFT_HEIGHT 240 			// ST7789 240 x 240


// ##################################################################################
//
// Section 2. Define the pins that are used to interface with the display here
//
// ##################################################################################


#define TFT_MOSI 23 
#define TFT_SCLK 18
#define TFT_CS   25
#define TFT_DC    2
#define TFT_RST   4 
#define TFT_BL   14  


// ##################################################################################
//
// Section 3. Define the fonts that are to be used here
//
// ##################################################################################


#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// Comment out the #define below to stop the SPIFFS filing system and smooth font code being loaded
// this will save ~20kbytes of FLASH
// #define SMOOTH_FONT


// ##################################################################################
//
// Section 4. Other options
//
// ##################################################################################

 #define SPI_FREQUENCY  40000000

