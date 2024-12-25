#include "../archdef.h"

#ifdef ULIB_PROCESS_LINUX
#include "process.h"

#include <unistd.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <fcntl.h>
#include <ulib/format.h>

#include "../../process_exceptions.h"

extern char **environ;

namespace ulib
{
    namespace detail
    {
        void MakeExecveArgs(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args) {}

        ulib::list<ulib::u8string> cmdline_to_args(ulib::u8string_view line)
        {
            bool inQuotes = false;
            ulib::u8string curr = line;
            for (auto &ch : curr)
            {
                if (inQuotes)
                {
                    if (ch == '\"')
                    {
                        inQuotes = false;
                        ch = 0;
                    }
                }
                else
                {
                    if (ch == '\"')
                    {
                        inQuotes = true;
                        ch = 0;
                    }
                    else if (ch == ' ')
                    {
                        ch = '\0';
                    }
                }
            }

            using ChT = typename ulib::u8string_view::value_type;

            ulib::list<ulib::u8string> result;
            const char *delim = "\0";
            auto spl = curr.split(ulib::u8string_view{(ChT *)delim, (ChT *)delim + 1});
            for (auto word : spl)
            {
                result.push_back(word);
            }

            return result;
        }

        ulib::u8string u8path_to_artifact_name(ulib::u8string_view path)
        {
            ulib::u8string firstArg;
            ulib::u8string callStr{path};
            size_t pos = callStr.rfind('/');
            if (pos == ulib::npos)
            {
                firstArg = callStr;
            }
            else
            {
                size_t idx = pos + 1;
                // printf("idx: %d, diff: %d\n", (int)idx, (int)(callStr.size() - idx));

                firstArg = callStr.substr(idx, callStr.size() - idx);
            }

            return firstArg;
        }

        void closefd(int fd)
        {
            if (fd == -1)
            {
                throw process_internal_error{"fd in closefd is invalid"};
            }

            if (::close(fd) == -1)
            {
                throw process_internal_error{"failed close fd"};
            }
        }

        void sdup2(int fd1, int fd2)
        {
            if (fd1 == -1)
            {
                throw process_internal_error{"fd1 in dup2 is invalid"};
            }

            if (fd2 == -1)
            {
                throw process_internal_error{"fd1 in dup2 is invalid"};
            }

            int rv = ::dup2(fd1, fd2);
            if (rv == -1)
            {
                throw process_internal_error{"failed dup2"};
            }
        }

        struct pipe_wrapper
        {
            pipe_wrapper()
            {
                fd[0] = -1;
                fd[1] = -1;
            }

            ~pipe_wrapper()
            {
                if (fd[0] != -1)
                {
                    detail::closefd(fd[0]);
                }

                if (fd[1] != -1)
                {
                    detail::closefd(fd[1]);
                }
            }

            void openfds()
            {
                if (::pipe(fd) == -1)
                {
                    throw process_internal_error{"failed create pipe"};
                }
            }

            void closefd(int idx)
            {
                if (fd[idx] != -1)
                {
                    detail::closefd(fd[idx]);
                    fd[idx] = -1;
                }
                else
                {
                    throw process_internal_error{"attempt to close closed fd"};
                }
            }

            int detachfd(int idx)
            {
                if (fd[idx] == -1)
                {
                    throw process_internal_error{"attempt to detach closed fd"};
                }
                else
                {
                    int rv = fd[idx];
                    fd[idx] = -1;
                    return rv;
                }
            }

            int fd[2];
        };
    } // namespace detail

    process::bpipe::~bpipe() { close(); }

    process::bpipe &process::bpipe::operator=(bpipe &&other)
    {
        close();

        mHandle = other.mHandle;
        other.mHandle = 0;

        return *this;
    }

    void process::bpipe::close()
    {
        if (mHandle)
        {
            ::close(mHandle);
            mHandle = 0;
        }
    }

    size_t process::rpipe::read(void *buf, size_t size) { return ::read(mHandle, buf, size); }
    ulib::string process::rpipe::read_all()
    {
        ulib::string result;
        char reading_buf[1];
        while (::read(mHandle, reading_buf, 1) > 0)
        {
            result.append(ulib::string_view{reading_buf, 1});
        }

        return result;
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
            char ch = getchar();
            if (ch == '\n')
                break;

            str.push_back(ch);
        }

        return str;
    }

    size_t process::wpipe::write(const void *buf, size_t size) { return ::write(mHandle, buf, size); }
    size_t process::wpipe::write(ulib::string_view str) { return ::write(mHandle, str.data(), str.size()); }

    process::process()
    {
        mHandle = 0;
        mWaited = false;
    }
    process::process(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args, uint32 flags,
                     std::optional<std::filesystem::path> workingDirectory)
    {
        mHandle = 0;
        mWaited = false;
        this->run(path, args, flags, workingDirectory);
    }
    process::process(ulib::u8string_view line, uint32 flags, std::optional<std::filesystem::path> workingDirectory)
    {
        mHandle = 0;
        mWaited = false;
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
        ulib::u8string callStr{path.u8string()};
        ulib::u8string firstArg = detail::u8path_to_artifact_name(callStr);
        firstArg.MarkZeroEnd();

        ulib::list<ulib::u8string> zargs = args;
        for (auto &zarg : zargs)
            zarg.MarkZeroEnd();

        ulib::list<const char *> argvList;
        argvList.push_back((char *)firstArg.data());
        for (auto &arg : zargs)
            argvList.push_back((char *)arg.data());
        argvList.push_back(NULL);

        if (workingDirectory)
        {
            this->run((const char *)callStr.c_str(), (char **)argvList.data(),
                      (const char *)workingDirectory->u8string().c_str(), flags);
        }
        else
        {
            this->run((const char *)callStr.c_str(), (char **)argvList.data(), nullptr, flags);
        }
    }

    void process::run(ulib::u8string_view line, uint32 flags, std::optional<std::filesystem::path> workingDirectory)
    {
        auto args = detail::cmdline_to_args(line);
        if (args.size() == 0)
            throw process_internal_error{"invalid command line"};

        ulib::u8string callStr = args.front();
        ulib::u8string firstArg = detail::u8path_to_artifact_name(callStr);
        args.front() = firstArg;

        for (auto &arg : args)
            arg.MarkZeroEnd();

        callStr.MarkZeroEnd();

        ulib::list<const char *> argvList;
        // argvList.push_back((char *)firstArg.data());
        for (auto &arg : args)
            argvList.push_back((char *)arg.data());
        argvList.push_back(NULL);

        if (workingDirectory)
        {
            this->run((const char *)callStr.c_str(), (char **)argvList.data(),
                      (const char *)workingDirectory->u8string().c_str(), flags);
        }
        else
        {
            this->run((const char *)callStr.c_str(), (char **)argvList.data(), nullptr, flags);
        }
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

    void process::run(const char *path, char **argv, const char *workingDirectory, uint32 flags)
    {
        struct errdata
        {
            int type, code;
        };

        detail::pipe_wrapper p_sink, p_stdin, p_stdout, p_stderr;

        check_flags(flags);

        if (flags & pipe_stdin)
        {
            p_stdin.openfds();
        }

        if (flags & pipe_output)
        {
            p_stdout.openfds();
        }
        else
        {
            if (flags & pipe_stdout)
            {
                p_stdout.openfds();
            }

            if (flags & pipe_stderr)
            {
                p_stderr.openfds();
            }
        }

        p_sink.openfds();

        if (::fcntl(p_sink.fd[1], F_SETFD, FD_CLOEXEC) == -1)
        {
            throw process_internal_error{"fcntl failed"};
        }

        int pid_before_fork = getpid();
        int pid = fork();
        if (pid == 0)
        {
            try
            {
#ifdef __linux__
                if (flags & die_with_parent)
                {
                    // setsid();

                    int r = prctl(PR_SET_PDEATHSIG, SIGKILL);
                    if (r == -1)
                    {
                        throw process_internal_error{"prctl failed"};
                    }

                    if (getppid() != pid_before_fork)
                    {
                        throw process_internal_error{"getppid() != pid_before_fork"};
                    }
                }
#endif
                if (flags & pipe_stdin)
                {
                    int fn = fileno(stdin);
                    detail::closefd(fn);
                    detail::sdup2(p_stdin.fd[0], fn);
                }

                if (flags & pipe_output)
                {
                    int fn_out = fileno(stdout);
                    int fn_err = fileno(stderr);

                    detail::closefd(fn_out);
                    detail::sdup2(p_stdout.fd[1], fn_out);

                    detail::closefd(fn_err);
                    detail::sdup2(p_stdout.fd[1], fn_err);
                }
                else
                {
                    if (flags & pipe_stdout)
                    {
                        int fn = fileno(stdout);
                        detail::closefd(fn);
                        detail::sdup2(p_stdout.fd[1], fn);
                    }

                    if (flags & pipe_stderr)
                    {
                        int fn = fileno(stderr);
                        detail::closefd(fn);
                        detail::sdup2(p_stderr.fd[1], fn);
                    }
                }

                p_sink.closefd(0);

                if (workingDirectory)
                {
                    if (chdir(workingDirectory) == -1)
                    {
                        throw errdata{1, errno};
                    }
                }

                // child
                // char *argv[] = {(char *)firstArg.c_str(), NULL};
                // char *envp[] = {(char *)"some", NULL};

                execve(path, argv, environ);
                execvp(path, argv);

                throw errdata{0, errno};
            }
            catch (const errdata &ed)
            {
                ::write(p_sink.fd[1], &ed, sizeof(errdata));
                p_sink.closefd(1);
                ::_exit(EXIT_FAILURE);
            }
            catch (const std::exception &ex)
            {
                perror(ex.what());
                errdata ed{-1, errno};
                ::write(p_sink.fd[1], &ed, sizeof(errdata));
                p_sink.closefd(1);
                ::_exit(EXIT_FAILURE);
            }
            catch (...)
            {
                perror("something unexpected was catched");

                errdata ed{-2, errno};
                ::write(p_sink.fd[1], &ed, sizeof(errdata));
                p_sink.closefd(1);
                ::_exit(EXIT_FAILURE);
            }
        }
        else if (pid == -1)
        {
            // fork error
            throw process_internal_error{"fork failed"};
        }
        else
        {
            {
                p_sink.closefd(1);

                errdata ed;
                int rv = ::read(p_sink.fd[0], &ed, sizeof(errdata));
                if (rv == 0)
                {
                    // no error
                }
                else if (rv == sizeof(errdata))
                {
                    if (ed.type == 0 && ed.code == ENOENT)
                    {
                        // execv
                        throw process_file_not_found_error{std::strerror(ed.code)};
                    }
                    else if (ed.type == 1 && ed.code == ENOENT)
                    {
                        // chdir
                        throw process_invalid_working_directory_error{std::strerror(ed.code)};
                    }
                    else
                    {
                        throw process_internal_error{
                            ulib::format("({}) errno [{}]: {}", ed.type, ed.code, std::strerror(ed.code))};
                    }
                }
                else if (rv == -1)
                {
                    throw process_internal_error{"read sink pipe failed"};
                }
                else
                {
                    throw process_internal_error{"read sink pipe is invalid"};
                }
            }

            if (flags & pipe_stdin)
            {
                mInPipe = std::move(wpipe{p_stdin.detachfd(1)});
            }

            if (flags & pipe_output)
            {
                mOutPipe = std::move(rpipe{p_stdout.detachfd(0)});
            }
            else
            {
                if (flags & pipe_stdout)
                {
                    mOutPipe = std::move(rpipe{p_stdout.detachfd(0)});
                }

                if (flags & pipe_stderr)
                {
                    mErrPipe = std::move(rpipe{p_stderr.detachfd(0)});
                }
            }

            mHandle = pid;
        }
    }

    std::optional<int> process::wait(std::chrono::milliseconds ms) {}

    int process::wait()
    {
        int wstatus;
        int result = waitpid(mHandle, &wstatus, 0);
        if (result == -1)
        {
            throw ulib::RuntimeError{"waitpid failed"};
        }

        mWaited = true;
        return WEXITSTATUS(wstatus);
    }

    bool process::is_running()
    {
        int wstatus;
        int result = waitpid(mHandle, &wstatus, WNOHANG);
        if (result == -1)
        {
            throw ulib::RuntimeError{"waitpid failed"};
        }
        else if (result == 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    bool process::is_finished() { return !is_running(); }
    void process::detach() { destroy_handles(); } // already detached
    void process::terminate()
    {
        if (::kill(mHandle, SIGKILL) == -1)
            throw process_internal_error{std::strerror(errno)};
    }

    std::optional<int> process::check()
    {
        int wstatus;
        int result = waitpid(mHandle, &wstatus, WNOHANG);
        if (result == -1)
        {
            throw ulib::RuntimeError{"waitpid failed"};
        }
        else if (result == 0)
        {
            return std::nullopt;
        }
        else
        {
            return WEXITSTATUS(wstatus);
        }
    }

    void process::destroy_pipes()
    {
        mInPipe.close();
        mOutPipe.close();
        mErrPipe.close();
    }

    void process::destroy_handles()
    {
        mHandle = 0;
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
        other.mHandle = 0;

        mInPipe = std::move(other.mInPipe);
        mOutPipe = std::move(other.mOutPipe);
        mErrPipe = std::move(other.mErrPipe);

        mWaited = other.mWaited;
    }

} // namespace ulib

#endif