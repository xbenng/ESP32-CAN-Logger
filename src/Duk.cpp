#include "Arduino.h"
#include "CanHandler.h"
#include "Display.h"

namespace Duk
{

static void my_fatal(void *udata, const char *msg)
{
    (void)udata; /* ignored in this case, silence warning */

    /* Note that 'msg' may be NULL. */
    Serial.print("*** FATAL ERROR: ");
    Serial.println(msg ? msg : "no message");
    tft.print("*** FATAL ERROR: ");
    tft.println(msg ? msg : "no message");
    // abort();
    // duk_destroy_heap(ctx);
    // ctx = duk_create_heap(NULL, NULL, NULL, NULL, my_fatal);
}

static duk_context *ctx = duk_create_heap(NULL, NULL, NULL, NULL, my_fatal);

static duk_ret_t native_print(duk_context *ctx)
{
    Serial.println(duk_to_string(ctx, 0));
    tft.println(duk_to_string(ctx, 0));
    return 0; /* no return value (= undefined) */
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

void task_repl(void *pvParameters)
{
    static int x_current{0};
    static int y_current{0};
    static String expression;

    for (;;)
    {
        if (Serial.available() > 0)
        {
            SemaphoreHandle_t &display_lock{Display::get_lock()};

            if (display_lock != NULL)
            {
                if (xSemaphoreTake(display_lock, (TickType_t)10) == pdTRUE)
                {
                    tft.setCursor(x_current, y_current);

                    char data{static_cast<char>(Serial.read())};

                    Serial.print(data);

                    if (data == '\b')
                    {
                        constexpr int char_width{6};
                        constexpr int char_height{8};
                        // backspace
                        if (expression.length() > 0)
                        {
                            expression.remove(expression.length() - 1);
                            int x{tft.getCursorX()};
                            int y{tft.getCursorY()};
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
                    xSemaphoreGive(display_lock);
                }
            }
        }
    }
}

void setup()
{
    duk_push_c_function(ctx, native_print, 1 /*nargs*/);
    duk_put_global_string(ctx, "print");

    duk_push_c_function(ctx, start_log, 0 /*nargs*/);
    duk_put_global_string(ctx, "start_log");

    duk_push_c_function(ctx, stop_log, 0 /*nargs*/);
    duk_put_global_string(ctx, "stop_log");

    xTaskCreate(task_repl, "duk_repl", 8192, 0, 20, NULL);
}

duk_context *get_context()
{
    return ctx;
}

} // namespace Duk