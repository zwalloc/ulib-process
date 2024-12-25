#include "../archdef.h"

#ifdef ULIB_PROCESS_WINDOWS

#include <ulib/format.h>
#include <windows.h>

#include "process_killonclosejob.h"
#include "process_error.h"

namespace win32
{
    namespace process
    {
        KillOnCloseJob::KillOnCloseJob()
        {
            mJob = nullptr;
            // Init();
        }

        KillOnCloseJob::~KillOnCloseJob() { Close(); }

        void KillOnCloseJob::Init()
        {
            mJob = CreateJobObjectW(NULL, NULL);
            if (!mJob)
                throw ulib::process_internal_error(ulib::format("CreateJobObjectW failed: {}", detail::GetLastErrorAsString()));

            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;

            if (!SetInformationJobObject(mJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
            {
                Close();
                throw ulib::process_internal_error(ulib::format("SetInformationJobObject failed: {}", detail::GetLastErrorAsString()));
            }
        }

        void KillOnCloseJob::Close()
        {
            if (mJob)
            {
                ::CloseHandle(mJob);
                mJob = nullptr;
            }
        }

        void KillOnCloseJob::AssignToProcess(void *hProcess)
        {
            if (!AssignProcessToJobObject(mJob, hProcess))
            {
                throw ulib::process_internal_error(ulib::format("AssignProcessToJobObject failed: {}", detail::GetLastErrorAsString()));
            }
        }
    } // namespace process
} // namespace win32

#endif