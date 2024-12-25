#pragma once

namespace win32
{
    namespace process
    {
        class KillOnCloseJob
        {
        public:
            KillOnCloseJob();
            ~KillOnCloseJob();

            void Init();
            void Close();
            void AssignToProcess(void* hProcess);

        private:
            void* mJob;
        };
    }
}