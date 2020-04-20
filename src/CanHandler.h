#pragma once

#include "duktape.h"

namespace CAN
{
    duk_ret_t start_log(duk_context *ctx);
    duk_ret_t stop_log(duk_context *ctx);
    void setup();
}