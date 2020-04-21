#include "Display.h"

WROVER_KIT_LCD tft;

namespace Display
{
    static SemaphoreHandle_t display_lock { NULL };

    void setup()
    {
        tft.begin();

        tft.fillScreen(WROVER_BLACK);
        tft.setRotation(0);
        tft.setCursor(0, 0);
        tft.setTextColor(WROVER_WHITE);
        tft.setTextSize(1);

        constexpr const char* shell_prefix {"> "};
        tft.print(shell_prefix);
        tft.setupScrollArea(0, tft.height() - 40); 

        display_lock = xSemaphoreCreateMutex();
    }

    SemaphoreHandle_t& get_lock()
    {
        return display_lock;
    }

}