//------------------------------------------------------------------------------
// Android / POSIX compatibility shims for the CSPSP + JGE port.
//
// This header is force-included into every translation unit (via
// CMake `-include`) so that legacy MSVC/PSP-isms compile on Android without
// touching every source file.
//------------------------------------------------------------------------------
#pragma once

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>

#ifndef stricmp
#define stricmp strcasecmp
#endif

#ifndef strcmpi
#define strcmpi strcasecmp
#endif

#ifndef strnicmp
#define strnicmp strncasecmp
#endif

#ifndef _itoa
#define _itoa(v, b, r) snprintf((b), sizeof(b), "%d", (int)(v))
#endif

#ifndef _itoa_s
#define _itoa_s(v, b, sz, r) snprintf((b), (sz), "%d", (int)(v))
#endif

#ifndef Sleep
#define Sleep(ms) usleep((ms) * 1000)
#endif

#ifndef _snprintf
#define _snprintf snprintf
#endif

// bionic may not expose these under the non-standard names
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
