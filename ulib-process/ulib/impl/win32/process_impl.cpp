#include "../archdef.h"

#ifdef ULIB_PROCESS_WINDOWS
#include "process.h"
#include "process_job.h"
#include "process_system_pipe.h"
#include "process_error.h"

namespace ulib
{
    process::bpipe &process::bpipe::operator=(bpipe &&other)
    {
        close();

        mHandle = other.mHandle;
        other.mHandle = nullptr;

        return *this;
    }

    process::bpipe::~bpipe() { close(); }

    void process::bpipe::close()
    {
        if (mHandle)
        {
            ::CloseHandle(mHandle);
            mHandle = 0;
        }
    }

    size_t process::rpipe::read(void *buf, size_t size)
    {
        DWORD readen = 0;
        if (!::ReadFile(mHandle, buf, DWORD(size), &readen, NULL))
        {
            throw process_internal_error(ulib::format("ReadFile failed: {}", win32::detail::GetLastErrorAsString()));
        }

        return size_t(readen);
    }

    ulib::string process::rpipe::read_all()
    {
        ulib::string output;

        char buf[2048];

        while (true)
        {
            DWORD readen = 0;
            if (!::ReadFile(mHandle, buf, sizeof(buf), &readen, NULL))
            {
                if (::GetLastError() == ERROR_BROKEN_PIPE)
                    break;

                throw process_internal_error(
                    ulib::format("ReadFile failed: {}", win32::detail::GetLastErrorAsString()));
            }

            if (readen == 0)
                break;

            output.Append(ulib::string_view{buf, size_t(readen)});
        }

        return output;
    }

    char process::rpipe::getchar()
    {
        char ch;
        this->read(&ch, 1);
        return ch;
    }

    ulib::string process::rpipe::getline()
    {
        ulib::string str;
        while (true)
        {
            char ch = this->getchar();
            if (ch == '\n')
                break;

            str.push_back(ch);
        }

        return str;
    }

    size_t process::wpipe::write(const void *data, size_t size)
    {
        DWORD written = 0;
        if (!WriteFile(mHandle, data, DWORD(size), &written, NULL))
        {
            throw process_internal_error(ulib::format("WriteFile failed: {}", win32::detail::GetLastErrorAsString()));
        }

        return size_t(written);
    }

    size_t process::wpipe::write(ulib::string_view str)
    {
        return this->write(str.data(), str.size());
    }

    process::process()
    {
        mHandle = 0;
        mWaited = false;
        mPid = 0;
    }
    process::process(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args, uint32 flags,
                     std::optional<std::filesystem::path> workingDirectory)
    {
        mHandle = 0;
        mWaited = false;
        mPid = 0;
        this->run(path, args, flags, workingDirectory);
    }
    process::process(ulib::u8string_view line, uint32 flags, std::optional<std::filesystem::path> workingDirectory)
    {
        mHandle = 0;
        mWaited = false;
        mPid = 0;
        this->run(line, flags, workingDirectory);
    }
    process::process(process &&other) { this->move_init(std::move(other)); }
    process::~process() { this->finish(); }

    process &process::operator=(process &&other)
    {
        this->finish();
        this->move_init(std::move(other));

        return *this;
    }

    void process::run(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args, uint32 flags,
                      std::optional<std::filesystem::path> workingDirectory)
    {
        ulib::wstring wline;
        wline += L"\"" + path.generic_wstring() + L"\" ";
        for (auto &arg : args)
            wline += L"\"" + ulib::swstr(arg) + L"\" ";
        wline.pop_back();

        this->run(wline, flags, workingDirectory);
    }

    void process::run(ulib::u8string_view line, uint32 flags, std::optional<std::filesystem::path> workingDirectory)
    {
        ulib::wstring wline = ulib::wstr(line);
        this->run(wline, flags, workingDirectory);
    }

    void check_flags(uint32 flags)
    {
        if (flags & process::pipe_output)
        {
            if (flags & process::pipe_stdout)
                throw process_invalid_flags_error{"pipe_stdout flag is incompatible with pipe_output flag"};

            if (flags & process::pipe_stderr)
                throw process_invalid_flags_error{"pipe_stderr flag is incompatible with pipe_output flag"};
        }
    }

    void process::run(ulib::wstring &line, uint32 flags, std::optional<std::filesystem::path> workingDirectory)
    {
        check_flags(flags);

        try
        {
            bool useRedirect =
                (flags & pipe_stdin) || (flags & pipe_stdout) || (flags & pipe_stderr) || (flags & pipe_output);

            bool useJob = (flags & die_with_parent);
            if (useJob)
            {
                if (win32::process::IsInJob())
                {
                    // printf("job disabled because already in job\n");
                    useJob = false;
                }
            }

            // ulib::wstring wline = ulib::wstr(line);
            DWORD dwCreationFlags = 0;

            const wchar_t *pwszWorkingDirectory = nullptr;
            if (workingDirectory)
                pwszWorkingDirectory = workingDirectory->c_str();

            ::STARTUPINFOW si = {0};
            si.cb = sizeof(si);

            win32::detail::WriteSystemPipe inputPipe;
            win32::detail::ReadSystemPipe outputPipe;
            win32::detail::ReadSystemPipe errorPipe;

            if (useRedirect)
            {
                si.dwFlags |= STARTF_USESTDHANDLES;
                si.hStdInput = (flags & pipe_stdin) ? inputPipe.Init() : GetStdHandle(STD_INPUT_HANDLE);

                if (flags & pipe_output)
                {
                    if ((flags & pipe_stdout) || (flags & pipe_stderr))
                        throw process_internal_error("Invalid process creation flags combo with the pipe_output flag");

                    auto h = outputPipe.Init();
                    si.hStdOutput = h;
                    si.hStdError = h;
                }
                else
                {
                    si.hStdOutput = (flags & pipe_stdout) ? outputPipe.Init() : GetStdHandle(STD_OUTPUT_HANDLE);
                    si.hStdError = (flags & pipe_stderr) ? errorPipe.Init() : GetStdHandle(STD_ERROR_HANDLE);
                }
            }

            if (useJob)
            {
                mJob.Init();
                dwCreationFlags |= CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB;
            }

            if (flags & create_new_console)
                dwCreationFlags |= CREATE_NEW_CONSOLE;

            line.MarkZeroEnd();
            PROCESS_INFORMATION pi = {};
            if (CreateProcessW(0, line.data(), 0, 0, useRedirect ? TRUE : FALSE, dwCreationFlags, 0,
                               pwszWorkingDirectory, &si, &pi))
            {
                mHandle = pi.hProcess;
                mPid = pi.dwProcessId;

                mInPipe = wpipe{inputPipe.RedirectHandle()};
                mOutPipe = rpipe{outputPipe.RedirectHandle()};
                mErrPipe = rpipe{errorPipe.RedirectHandle()};

                if (useJob)
                {
                    mJob.AssignToProcess(mHandle);
                    ResumeThread(pi.hThread);
                }

                ::CloseHandle(pi.hThread);
            }
            else
            {
                DWORD dwErrorCode = ::GetLastError();
                if (dwErrorCode == ERROR_PATH_NOT_FOUND || dwErrorCode == ERROR_FILE_NOT_FOUND)
                    throw process_file_not_found_error{win32::detail::GetLastErrorAsString()};

                if (dwErrorCode == ERROR_DIRECTORY)
                    throw process_invalid_working_directory_error{win32::detail::GetLastErrorAsString()};

                throw process_internal_error(
                    ulib::format("CreateProcessW failed: {}", win32::detail::GetLastErrorAsString()));
            }
        }
        catch (...)
        {
            destroy_handles();
            throw;
        }
    }

    std::optional<int> process::wait(std::chrono::milliseconds ms)
    {
        DWORD state = WaitForSingleObject(mHandle, DWORD(ms.count()));
        if (state == WAIT_TIMEOUT)
            return std::nullopt;

        if (state == WAIT_OBJECT_0)
        {
            DWORD exitCode = -1;
            if (!GetExitCodeProcess(mHandle, &exitCode))
                throw process_internal_error(
                    ulib::format("GetExitCodeProcess failed: {}", win32::detail::GetLastErrorAsString()));

            mWaited = true;
            return int(exitCode);
        }

        throw process_internal_error(
            ulib::format("WaitForSingleObject failed: {}", win32::detail::GetLastErrorAsString()));
    }

    int process::wait() { return wait(std::chrono::milliseconds{0xFFFFFFFF}).value(); }
    bool process::is_running() { return !is_finished(); }
    bool process::is_finished() { return wait(std::chrono::milliseconds(0)).has_value(); }
    void process::detach() { destroy_handles(); }
    void process::terminate()
    {
        if (!::TerminateProcess(mHandle, 260))
            throw process_internal_error(
                ulib::format("TerminateProcess failed: {}", win32::detail::GetLastErrorAsString()));
        
        mWaited = true;
    }

    std::optional<int> process::check()
    {
        return wait(std::chrono::milliseconds(0));
    }

    void process::destroy_pipes()
    {
        mInPipe.close();
        mOutPipe.close();
        mErrPipe.close();
    }

    void process::destroy_handles()
    {
        if (mHandle)
        {
            ::CloseHandle(mHandle);
            mHandle = nullptr;
        }

        destroy_pipes();
    }

    void process::finish()
    {
        try
        {
            if (this->is_bound())
                if (!mWaited)
                    this->terminate();

            destroy_handles();
        }
        catch (...)
        {
            std::terminate();
        }
    }

    void process::move_init(process &&other)
    {
        mHandle = other.mHandle;
        other.mHandle = nullptr;

        mJob = std::move(other.mJob);
        mInPipe = std::move(other.mInPipe);
        mOutPipe = std::move(other.mOutPipe);
        mErrPipe = std::move(other.mErrPipe);

        mWaited = other.mWaited;
        mPid = other.mPid;
    }

} // namespace ulib

#endif