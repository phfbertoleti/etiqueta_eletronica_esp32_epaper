/* Header file com definições do display e-paper */

#ifndef DISPLAY_E_PAPER_DEFS
#define DISPLAY_E_PAPER_DEFS

#include <GxGDEW0213M21/GxGDEW0213M21.h>

#include GxEPD_BitmapExamples

/* Fontes utilizadas (da Adafruit_GFX) */
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeSerifBold24pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSerifItalic12pt7b.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>


#endif

/* Objetos de controle do display e-paper */
GxIO_Class io(SPI,  EPD_CS, EPD_DC,  EPD_RSET);
GxEPD_Class display(io, EPD_RSET, EPD_BUSY);

/* Objeto de controle do SD card */
#if defined(_HAS_SDCARD_) && !defined(_USE_SHARED_SPI_BUS_)
SPIClass SDSPI(VSPI);
#endif
