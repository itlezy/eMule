#pragma once

// Local compiler site configuration for the Windows 10/11 build baseline.
// The current branch assumes a modern Visual Studio toolchain and Windows SDK.

#define HAVE_SAPI_H
#define HAVE_QEDIT_H
#define HAVE_WMSDK_H

#if _MSC_VER < 1937 // before VS2022 17.7
#define NOEXCEPT noexcept
#else
#define NOEXCEPT
#endif

#include <sdkddkver.h>
