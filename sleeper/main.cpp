#include <thread>

int main()
{
    size_t counter = 0;
    while (counter < 600)
    {
        std::this_thread::sleep_for(std::chrono::seconds{1});
        counter++;
    }

    return 0;
}