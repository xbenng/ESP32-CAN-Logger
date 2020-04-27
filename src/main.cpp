
  
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

#include "Duk.h"
#include "Display.h"



constexpr int pin_5v_sense {34};
constexpr int pin_button {35};
constexpr int voltage_threshold {3000};
void setup() {

  Duk::setup();

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

  pinMode(pin_5v_sense, INPUT);
  pinMode(pin_button, INPUT);

}




void loop(void) {

  static int button_last_time {0};
  
  if (!digitalRead(pin_button))
  {
    if (millis() - button_last_time > 200)
    {
      button_last_time = millis();
      if (CAN::is_logging())
      {
        CAN::stop_log();
      }
      else
      {
        CAN::start_log();
      }    
    }
  }
}