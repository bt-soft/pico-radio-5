// Stripped down User_Setup.h version for the Pico-Matrix-Touch-Keyboard //

#define ILI9488_DRIVER // WARNING: Do not connect ILI9488 display SDO to MISO if other devices share the SPI bus (TFT SDO does NOT tristate when CS is high)

#define TFT_MISO 0 // SPI0 RX
#define TFT_SCLK 2 // SPI0 SCK
#define TFT_MOSI 3 // SPI0 TX

#define TFT_CS 	 6   // Chip select control pin - sima PICO
#define TFT_DC   4   // Data Command control pin - sima PICO
#define TFT_RST  5  // Reset pin (could connect to Arduino RESET pin) - sima PICO

// Touch Support
#define TOUCH_CS 7 // Chip select pin (T_CS) of touch screen

// Nem kell programmatikus LED kapcsolgatás...
// #define TFT_BL   17            // LED back-light control pin
// #define TFT_BACKLIGHT_ON HIGH  // Level to turn ON back-light (HIGH or LOW)

#define LOAD_GLCD  // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2 // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4 // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6 // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
// #define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8 // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
// #define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// Comment out the #define below to stop the SPIFFS filing system and smooth font code being loaded
// this will save ~20kbytes of FLASH
#define SMOOTH_FONT

#define SPI_FREQUENCY 27000000

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY 20000000

// The XPT2046 requires a lower SPI clock rate of 2.5MHz so we define that here:
#define SPI_TOUCH_FREQUENCY 2500000