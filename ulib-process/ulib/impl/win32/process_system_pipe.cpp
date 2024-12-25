#include "../archdef.h"

#ifdef ULIB_PROCESS_WINDOWS

#include "process_system_pipe.h"
#include <windows.h>

#include <ulib/format.h>
#include "../../process_exceptions.h"
#include "process_error.h"

namespace win32::detail
{
    SystemPipe::SystemPipe()
    {
        mReadHandle = nullptr;
        mWriteHandle = nullptr;
    }

    SystemPipe::~SystemPipe() { Close(); }

    void SystemPipe::Close()
    {
        CloseReadHandle();
        CloseWriteHandle();
    }

    void SystemPipe::InitBase()
    {
        SECURITY_ATTRIBUTES saAttr = {};
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;

        if (!CreatePipe(&mReadHandle, &mWriteHandle, &saAttr, 0))
        {
            throw ulib::process_internal_error(ulib::format("Failed CreatePipe: {}", detail::GetLastErrorAsString()));
        }
    }

    // for stdin
    void *SystemPipe::InitForInput()
    {
        InitBase();

        if (!SetHandleInformation(mWriteHandle, HANDLE_FLAG_INHERIT, 0))
        {
            Close();
            throw ulib::process_internal_error(
                ulib::format("Failed SetHandleInformation for write handle: {}", detail::GetLastErrorAsString()));
        }

        return mReadHandle;
    }

    // for stdout and stderr
    void *SystemPipe::InitForOutput()
    {
        InitBase();

        if (!SetHandleInformation(mReadHandle, HANDLE_FLAG_INHERIT, 0))
        {
            Close();
            throw ulib::process_internal_error(
                ulib::format("Failed SetHandleInformation for read handle: {}", detail::GetLastErrorAsString()));
        }

        return mWriteHandle;
    }

    void *SystemPipe::DetachReadHandle()
    {
        void *handle = mReadHandle;
        mReadHandle = nullptr;
        return handle;
    }

    void *SystemPipe::DetachWriteHandle()
    {
        void *handle = mWriteHandle;
        mWriteHandle = nullptr;
        return handle;
    }

    void SystemPipe::CloseReadHandle()
    {
        if (mReadHandle)
        {
            ::CloseHandle(mReadHandle);
            mReadHandle = nullptr;
        }
    }

    void SystemPipe::CloseWriteHandle()
    {
        if (mWriteHandle)
        {
            ::CloseHandle(mWriteHandle);
            mWriteHandle = nullptr;
        }
    }

} // namespace win32::detail

#endif