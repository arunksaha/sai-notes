#include <thread>
#include <iostream>

void run_dataplane();

int main()
{
    std::cout << "[MAIN] Starting uswitch...\n";

    std::thread dp_thread(run_dataplane);

    dp_thread.join();

    return 0;
}

