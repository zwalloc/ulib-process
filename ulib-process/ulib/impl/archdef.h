#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define ULIB_PROCESS_WINDOWS
#else
#define ULIB_PROCESS_LINUX
#endif