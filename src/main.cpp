
  
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



#include "duktape.h"

#include "Web.h"
#include "CanHandler.h"

#include "SPIFFS.h"
#include "SD_MMC.h"

#include "DukBindings.h"
#include "Display.h"

static void my_fatal(void *udata, const char *msg);

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




void setup() {

  DukBindings::setup(ctx);

  Serial.begin(115200);

  
  CAN::setup();
  Display::setup();

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
  static int x_current {0};
  static int y_current {0};


    if (Serial.available() > 0)
    {
      SemaphoreHandle_t& display_lock { Display::get_lock() };

      if (display_lock != NULL)
      {
        if (xSemaphoreTake( display_lock, ( TickType_t ) 10 ) == pdTRUE)
        {
        tft.setCursor(x_current, y_current);

        char data { static_cast<char>(Serial.read()) };
        
        Serial.print(data);


        if (data == '\b')
        {
          constexpr int char_width {6};
          constexpr int char_height {8};
          // backspace
          if (expression.length() > 0)
          {
            expression.remove(expression.length() - 1);
            int x {tft.getCursorX()};
            int y {tft.getCursorY()};
            if (x == 0)
            {
              // reverse wrap
              tft.setCursor(tft.width() - char_width, y - char_height);
            }
            else
            {
              tft.setCursor(x - char_width, y);
            }

            tft.fillRect(tft.getCursorX(), tft.getCursorY(), char_width, char_height, WROVER_BLACK);
          }

        }
        else
        {
          tft.print(data);

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
          else
          {
            expression.concat(data);
          }
          
        }
        
        x_current = tft.getCursorX();
        y_current = tft.getCursorY();
        xSemaphoreGive( display_lock );
      }

    }
  }
}