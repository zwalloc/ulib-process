#pragma once

#include "../archdef.h"
#ifdef ULIB_PROCESS_WINDOWS

#include <windows.h>

#include <ulib/string.h>
#include <ulib/format.h>

#include "../../process_exceptions.h"
#include "process_error.h"

namespace win32
{


    namespace process
    {
        inline bool IsInJob()
        {
            BOOL result;
            if (!IsProcessInJob(::GetCurrentProcess(), NULL, &result))
                throw ulib::process_internal_error(ulib::format("IsProcessInJob failed: {}", detail::GetLastErrorAsString()));

            return result;
        }

        inline bool CheckIsAutokillByJobEnabled()
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
            DWORD len = 0;
            if (!QueryInformationJobObject(NULL, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli), &len))
                return false;
            // throw ProcessError(ulib::format("QueryInformationJobObject failed: {}", detail::GetLastErrorAsString()));

            return jeli.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        }

        inline bool IsAutokillByJobEnabled()
        {
            if (!IsInJob())
                return false;

            return CheckIsAutokillByJobEnabled();
        }
    }
}

#endif