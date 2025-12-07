#include <thread>
#include <iostream>

void run_dataplane();
void run_mgmtplane();

int main()
{
    std::cout << "[MAIN] Starting uswitch...\n";

    std::thread mp_thread(run_mgmtplane);
    std::thread dp_thread(run_dataplane);

    mp_thread.join();
    dp_thread.join();

    return 0;
}
