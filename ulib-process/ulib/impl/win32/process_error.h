#pragma once

#include <windows.h>
#include <ulib/string.h>
#include <ulib/format.h>

#include "../../process_exceptions.h"

namespace win32
{
    namespace detail
    {
        inline ulib::u8string GetLastErrorAsString()
        {
            DWORD errorMessageID = ::GetLastError();
            if (errorMessageID == 0)
                return {};

            LPWSTR messageBuffer = nullptr;
            size_t size = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

            if (size <= 2)
                throw ulib::process_internal_error("FormatMessageW failed");

            ulib::wstring message(messageBuffer, size - 2);
            LocalFree(messageBuffer);

            return ulib::format(u8"[0x{:X}] -> \"{}\"", errorMessageID, message);
        }
    } // namespace detail
}