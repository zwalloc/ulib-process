#pragma once

namespace win32
{
    namespace detail
    {
        class SystemPipe
        {
        public:
            SystemPipe();
            ~SystemPipe();

            void Close();

            void *InitForInput();  // returns read handle
            void *InitForOutput(); // returns write handle

            void CloseReadHandle();
            void CloseWriteHandle();

            inline void *GetReadHandle() { return mReadHandle; }
            inline void *GetWriteHandle() { return mWriteHandle; }

            void *DetachReadHandle();
            void *DetachWriteHandle();

        private:
            void InitBase();

            void *mReadHandle;
            void *mWriteHandle;
        };

        class WriteSystemPipe
        {
        public:
            void *Init() { return mSystemPipe.InitForInput(); }
            void CloseBackgroundHandle() { mSystemPipe.CloseReadHandle(); }
            void *DetachFrontendHandle() { return mSystemPipe.DetachWriteHandle(); }

            void *RedirectHandle()
            {
                CloseBackgroundHandle();
                return DetachFrontendHandle();
            }

        private:
            SystemPipe mSystemPipe;
        };

        class ReadSystemPipe
        {
        public:
            void *Init() { return mSystemPipe.InitForOutput(); }
            void CloseBackgroundHandle() { mSystemPipe.CloseWriteHandle(); }
            void *DetachFrontendHandle() { return mSystemPipe.DetachReadHandle(); }

            void *RedirectHandle()
            {
                CloseBackgroundHandle();
                return DetachFrontendHandle();
            }

        private:
            SystemPipe mSystemPipe;
        };

    } // namespace detail
}