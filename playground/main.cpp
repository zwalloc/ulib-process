#include <iostream>
#include <ulib/process.h>

int main()
{
    try
    {
        ulib::process proc;
        proc.run(u8"echo", {u8"test_text"}, ulib::process::pipe_stdout, "./ebasadsjaha");
        proc.wait();
        printf("text: %s\n", proc.out().read_all().c_str());
    }
    catch (const std::exception &ex)
    {
        printf("exception: %s\n", ex.what());
    }

    return 0;
}
