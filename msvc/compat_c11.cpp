#include <chrono>
#include <thread>

extern "C" void usleep(int64_t usec)
{
	std::this_thread::sleep_for(std::chrono::microseconds(usec));
}
