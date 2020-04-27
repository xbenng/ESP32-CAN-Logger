#pragma once
#include "duktape.h"

namespace Duk
{
    void setup();
    duk_context* get_context();
} // namespace Duk
