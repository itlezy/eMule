#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <atlstr.h>
#include <crtdbg.h>

#ifndef ASSERT
#define ASSERT(expression) _ASSERTE(expression)
#endif

/**
 * Provides the minimal Windows, ATL and debug-assert prerequisites needed by Kad utility helpers.
 */
#include "types.h"
