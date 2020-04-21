#pragma once
#include "WROVER_KIT_LCD.h"

extern WROVER_KIT_LCD tft;

namespace Display
{
    void setup();
    SemaphoreHandle_t& get_lock();
}