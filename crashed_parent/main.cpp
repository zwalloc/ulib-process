#include <iostream>
#include <ulib/process.h>
#include <stdio.h>

int main(int argc, const char** argv)
{
    uint flags = ulib::process::noflags;
    if (argc == 2)
    {
        // printf("die_with_parent\n");
        flags = ulib::process::die_with_parent;
    }

    try
    {
        auto leakingProcess = ulib::process{u8"sleeper", flags};
        int pid = leakingProcess.pid();
        
        ::fwrite(&pid, sizeof(int), 1, stdout);

        leakingProcess.detach();
        return 0;
    }
    catch (const std::exception& ex)
    {
        printf("[crashed_parent] exception: %s\n", ex.what());
        return 1;
    }
}