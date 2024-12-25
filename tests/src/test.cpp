#include <gtest/gtest.h>
#include <ulib/process.h>

TEST(Process, EchoArgs)
{
    ulib::process proc(u8"echo", {u8"test_text"}, ulib::process::pipe_stdout);
    int code = proc.wait();

    ulib::string out = (proc.out().read_all());
    if (out.ends_with('\n'))
        out.pop_back();

    ASSERT_EQ(out, "test_text");
}

TEST(Process, EchoLine)
{
    ulib::process proc(u8"echo test_text", ulib::process::pipe_stdout);
    int code = proc.wait();

    ulib::string out = (proc.out().read_all());
    if (out.ends_with('\n'))
        out.pop_back();

    ASSERT_EQ(out, "test_text");
}

TEST(Process, FileNotFoundError)
{
    ASSERT_THROW({ ulib::process proc(u8"ech111221ddo test_text"); }, ulib::process_file_not_found_error);
}

TEST(Process, InvalidWorkingDirectoryError)
{
    ASSERT_THROW({ ulib::process proc(u8"echo test_text", ulib::process::noflags, "shfjhsaflkasjfa1123d"); },
                 ulib::process_invalid_working_directory_error);
}

TEST(Process, InvalidFlagsError)
{
    ASSERT_THROW({ ulib::process proc(u8"echo test_text", ulib::process::pipe_output | ulib::process::pipe_stdout); },
                 ulib::process_invalid_flags_error);
    ASSERT_THROW({ ulib::process proc(u8"echo test_text", ulib::process::pipe_output | ulib::process::pipe_stderr); },
                 ulib::process_invalid_flags_error);
}

TEST(Process, Return5)
{
    ulib::process proc(u8"return5");
    int code = proc.wait();
    ASSERT_EQ(code, 5);
}

TEST(Process, Errout)
{
    ulib::process proc(u8"errout", ulib::process::pipe_stdout | ulib::process::pipe_stderr);
    proc.wait();

    ulib::string out = proc.out().read_all();
    ulib::string err = proc.err().read_all();

#ifdef ULIB_PROCESS_WINDOWS
    ASSERT_EQ(out, "cout\r\n");
    ASSERT_EQ(err, "cerr\r\n");
#else
    ASSERT_EQ(out, "cout\n");
    ASSERT_EQ(err, "cerr\n");
#endif
}

TEST(Process, ErroutDual)
{
    ulib::process proc(u8"errout", ulib::process::pipe_output);
    proc.wait();

    ulib::string out = proc.out().read_all();

#ifdef ULIB_PROCESS_WINDOWS
    ASSERT_EQ(out, "cout\r\ncerr\r\n");
#else
    ASSERT_EQ(out, "cout\ncerr\n");
#endif
}

TEST(Process, RetInput)
{
    ulib::process proc(u8"retinput", ulib::process::pipe_stdin);
    proc.in().write("22\n");
    ASSERT_EQ(proc.wait(), 22);
}

#ifdef _WIN32

#include <windows.h>
#include <tlhelp32.h>

bool is_pid_process_working(int pid)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(snapshot, &entry) == TRUE)
    {
        while (Process32Next(snapshot, &entry) == TRUE)
        {
            if(entry.th32ProcessID == pid)
            {
                CloseHandle(snapshot);
                return true;
            }
        }
    }

    CloseHandle(snapshot);
    return false;
}

void kill_pid(int pid)
{
    HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)pid);
    if (hProcess == NULL)
        return;

    ::TerminateProcess(hProcess, -1);
    ::CloseHandle(hProcess);
}

#else

#include <signal.h>

bool is_pid_process_working(int pid)
{
    if (kill(pid, 0) == -1)
    {
        if (errno == ESRCH)
        {
            // printf("process died: %d\n", pid);
            return false;
        }
            
    }

    return true;
}

void kill_pid(int pid)
{
    // kill(pid, SIGTERM);

    // int wstatus;
    // int result = waitpid(pid, &wstatus, 0);

    kill(pid, SIGKILL);

    // result = waitpid(pid, &wstatus, 0);
}

#endif

#include <thread>

TEST(Process, Detach)
{
    int childPid = -1;

    {
        ulib::process proc(u8"sleeper");
        childPid = proc.pid();
        proc.detach();
    }

    ASSERT_NE(childPid, -1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(is_pid_process_working(childPid));
    
    kill_pid(childPid);
}

#if defined(__linux__) || defined(ULIB_PROCESS_WINDOWS)

TEST(Process, WithDieWithParent)
{
    int childPid = -1;

    {
        ulib::process proc(u8"crashed_parent die_with_parent", ulib::process::pipe_stdout);
        ASSERT_EQ(proc.wait(), 0);
        proc.out().read(&childPid, sizeof(int));
    }

    ASSERT_NE(childPid, -1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_FALSE(is_pid_process_working(childPid));
}

#endif

TEST(Process, WithoutDieWithParent)
{
    int childPid = -1;

    {
        ulib::process proc(u8"crashed_parent", ulib::process::pipe_stdout);
        ASSERT_EQ(proc.wait(), 0);
        proc.out().read(&childPid, sizeof(int));
    }

    ASSERT_NE(childPid, -1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(is_pid_process_working(childPid));

    kill_pid(childPid);
}