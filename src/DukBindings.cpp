#include "Arduino.h"
#include "CanHandler.h"
#include "Display.h"

namespace DukBindings
{

static duk_ret_t native_print(duk_context *ctx) {
  Serial.println(duk_to_string(ctx, 0));
  tft.println(duk_to_string(ctx, 0));
  return 0;  /* no return value (= undefined) */
}

duk_ret_t start_log(duk_context *ctx)
{
    CAN::start_log();
    return 0;
}

duk_ret_t stop_log(duk_context *ctx)
{
    CAN::stop_log();
    return 0;
}

void setup(duk_context *ctx)
{
    duk_push_c_function(ctx, native_print, 1 /*nargs*/);
    duk_put_global_string(ctx, "print");

    duk_push_c_function(ctx, start_log, 0 /*nargs*/);
    duk_put_global_string(ctx, "start_log");

    duk_push_c_function(ctx, stop_log, 0 /*nargs*/);
    duk_put_global_string(ctx, "stop_log");

}

}