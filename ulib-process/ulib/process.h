#pragma once

#include "impl/archdef.h"


#ifdef ULIB_PROCESS_WINDOWS
#include "impl/win32/process.h"
#else
#include "impl/linux/process.h"
#endif