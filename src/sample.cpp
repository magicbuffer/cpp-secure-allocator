// Ours
#include "utils.hpp"
#include "basic_allocator.hpp"
#include "windows_secure_allocator_mk1.hpp"
#include "windows_secure_allocator_mk2.hpp"
// STL
#include <exception>
#include <vector>
#include <thread>

template <typename T>
using allocator = magic::basic_allocator<T>;

void work()
{
	try
	{
		std::vector<int, magic::basic_allocator<int>> secure_vector;
		for (int i = 0; i < 100000; ++i)
		{
			secure_vector.push_back(i);
		}
	}
	catch (std::exception& ex)
	{
		magic::write_line("exception: %s", ex.what());
	}
	catch (...)
	{
		magic::write_line("unexpected exception");
	}
}

int main(int argc, char *argv[])
{
	magic::write_line("#################### starting ####################");
	
	std::thread t1(work);
	std::thread t2(work);
	std::thread t3(work);
	std::thread t4(work);
	std::thread t5(work);

	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();

	magic::write_line("#################### exiting ####################");

	//auto& locks_mt1 = allocator<void>::static_storage::get_instance().get_locks();
}  