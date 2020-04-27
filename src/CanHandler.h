#pragma once

#include "duktape.h"

namespace CAN
{
    void start_log();
    void stop_log();
    bool is_logging();
    void setup();
}