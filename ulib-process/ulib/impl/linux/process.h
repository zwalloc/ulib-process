#pragma once

#include "../archdef.h"
#ifdef ULIB_PROCESS_LINUX

#include <ulib/string.h>
#include <filesystem>
#include <optional>
#include <signal.h>

#include "../../process_exceptions.h"

namespace ulib
{
    class process
    {
    public:
        enum flag
        {
            noflags = 0,
            pipe_stdin = 1,
            pipe_stdout = 2,
            pipe_stderr = 4,
            pipe_output = 8,
            die_with_parent = 16,
            create_new_console = 32,
        };

        class bpipe
        {
        public:
            bpipe() { mHandle = 0; }
            bpipe(int handle) : mHandle(handle) {}
            bpipe(const bpipe &) = delete;
            bpipe(bpipe &&other)
            {
                mHandle = other.mHandle;
                other.mHandle = 0;
            }
            ~bpipe();

            bpipe &operator=(bpipe &&other);

            // inline void *native_handle() { return mHandle; }
            inline bool is_open() { return mHandle != 0; }
            void close();

        protected:
            int mHandle;
        };

        class rpipe : public bpipe
        {
        public:
            rpipe() : bpipe() {}
            rpipe(int handle) : bpipe(handle) {}
            rpipe(rpipe &&other) : bpipe(std::move(other)) {}
            ~rpipe() {}

            rpipe &operator=(rpipe &&other)
            {
                *static_cast<bpipe *>(this) = std::move(other);
                return *this;
            }

            size_t read(void *buf, size_t size);
            ulib::string read_all();

            char getchar();
            ulib::string getline();

        private:
        };

        class wpipe : public bpipe
        {
        public:
            wpipe() : bpipe() {}
            wpipe(int handle) : bpipe(handle) {}
            wpipe(wpipe &&other) : bpipe(std::move(other)) {}
            ~wpipe() {}

            wpipe &operator=(wpipe &&other)
            {
                *static_cast<bpipe *>(this) = std::move(other);
                return *this;
            }

            size_t write(const void *buf, size_t size);
            size_t write(ulib::string_view str);

        private:
        };

        process();
        process(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args, uint32 flags = noflags,
                std::optional<std::filesystem::path> workingDirectory = std::nullopt);
        process(ulib::u8string_view line, uint32 flags = noflags,
                std::optional<std::filesystem::path> workingDirectory = std::nullopt);
        process(const process &) = delete;
        process(process &&other);
        ~process();

        process &operator=(process &&other);

        void run(const std::filesystem::path &path, const ulib::list<ulib::u8string> &args, uint32 flags = noflags,
                 std::optional<std::filesystem::path> workingDirectory = std::nullopt);
        void run(ulib::u8string_view line, uint32 flags = noflags,
                 std::optional<std::filesystem::path> workingDirectory = std::nullopt);

        std::optional<int> wait(std::chrono::milliseconds ms);
        int wait();

        bool is_running();
        bool is_finished();
        void detach();
        void terminate();

        std::optional<int> check();
        inline bool is_bound() { return mHandle != 0; }
        inline int pid() { return mHandle; }

        inline wpipe &in() { return mInPipe; }
        inline rpipe &out() { return mOutPipe; }
        inline rpipe &err() { return mErrPipe; }

    private:
        void run(const char *path, char **argv, const char* workingDirectory, uint32 flags);
        void destroy_pipes();
        void destroy_handles();
        void finish();
        void move_init(process&& other);

        int mHandle;

        wpipe mInPipe;
        rpipe mOutPipe;
        rpipe mErrPipe;

        bool mWaited;
    };
} // namespace ulib

#endif