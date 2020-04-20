
  
/***************************************************
  This is our GFX example for the Adafruit ILI9341 Breakout and Shield
  ----> http://www.adafruit.com/products/1651
  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/


#include "WROVER_KIT_LCD.h"

#include "duktape.h"

#include "Web.h"
#include "CanHandler.h"

#include "SPIFFS.h"
#include "SD_MMC.h"


static void my_fatal(void *udata, const char *msg);

WROVER_KIT_LCD tft;
static duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, my_fatal);




static void my_fatal(void *udata, const char *msg) 
{
    (void) udata;  /* ignored in this case, silence warning */

    /* Note that 'msg' may be NULL. */
    Serial.print("*** FATAL ERROR: ");
    Serial.println(msg ? msg : "no message");
    tft.print("*** FATAL ERROR: ");
    tft.println(msg ? msg : "no message");
    // abort();
    // duk_destroy_heap(ctx);
    // ctx = duk_create_heap(NULL, NULL, NULL, NULL, my_fatal);
}

static duk_ret_t native_print(duk_context *ctx) {
  Serial.println(duk_to_string(ctx, 0));
  tft.println(duk_to_string(ctx, 0));
  return 0;  /* no return value (= undefined) */
}



void setup() {
  duk_push_c_function(ctx, native_print, 1 /*nargs*/);
  duk_put_global_string(ctx, "print");

  duk_push_c_function(ctx, CAN::start_log, 0 /*nargs*/);
  duk_put_global_string(ctx, "start_log");

  duk_push_c_function(ctx, CAN::stop_log, 0 /*nargs*/);
  duk_put_global_string(ctx, "stop_log");


  Serial.begin(115200);
  tft.begin();

  tft.fillScreen(WROVER_BLACK);
  tft.setRotation(0);
  tft.setCursor(0, 0);
  tft.setTextColor(WROVER_WHITE);
  tft.setTextSize(1);

  constexpr const char* shell_prefix {"> "};
  tft.print(shell_prefix);
  tft.setupScrollArea(40, 40); 
  
  CAN::setup();


  // setup sd card
  if(!SD_MMC.begin()){
      Serial.println("Card Mount Failed");
      return;
  }
  uint8_t cardType = SD_MMC.cardType();

  if(cardType == CARD_NONE){
      Serial.println("No SD card attached");
      return;
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  Serial.print("SD_MMC Card Type: ");
  if(cardType == CARD_MMC){
      Serial.println("MMC");
  } else if(cardType == CARD_SD){
      Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
      Serial.println("SDHC");
  } else {
      Serial.println("UNKNOWN");
  }

  SPIFFS.begin();

  Web::setup();

}

String expression;

void loop(void) {

  if (Serial.available() > 0)
  {
    char data { static_cast<char>(Serial.read()) };
    
    Serial.print(data);

    if (data == '\n')
    {
      tft.print(data);

      Serial.println("evaluating: " + expression);
      // expression = "try {" + expression + "} catch(err) {print(err.message)}";
      if (duk_peval_string_noresult(ctx, expression.c_str()))
      {
        Serial.println("eval unsuccessful");
        tft.println("eval unsuccessful");   
      }
      
      expression = "";
      tft.print("> ");

    }
    else if (data == '\b')
    {
      // backspace
      expression.remove(expression.length() - 1);
      if(tft.getCursorX() > 12)
      {
      tft.setCursor(tft.getCursorX() - 6, tft.getCursorY());
      tft.fillRect(tft.getCursorX(), tft.getCursorY(), 6, 8, WROVER_BLACK);
      }

    }
    else
    {
      tft.print(data);
      expression.concat(data);
    }
    
  }
}