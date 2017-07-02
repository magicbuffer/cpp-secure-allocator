#if !defined(__MAGIC_BASIC_ALLOCATOR_HPP_)
#define __MAGIC_BASIC_ALLOCATOR_HPP_

// Ours
#include "utils.hpp"
// STL
#include <exception>
#include <cstdint>
#include <fstream>
#include <ios>
#include <mutex>

namespace magic
{
	class static_storage
	{
	public:
		static static_storage& get_instance()
		{
			static static_storage instance;
			return instance;
		}

		std::mutex& get_allocation_guard()
		{
			return _allocation_guard;
		}

		std::ofstream& get_allocation_stream()
		{
			if (!_allocation_stream.is_open())
			{
				_allocation_stream.open("basic_allocator.allocation.csv", std::ios_base::trunc);
			}
			return _allocation_stream;
		}

		std::ofstream& get_deallocation_stream()
		{
			if (!_deallocation_stream.is_open())
			{
				_deallocation_stream.open("basic_allocator.deallocation.csv", std::ios_base::trunc);
			}
			return _deallocation_stream;
		}

	private:
		static_storage() {}

		static_storage(static_storage const&) = delete;
		void operator=(static_storage const&) = delete;

		std::mutex _allocation_guard;
		std::ofstream _allocation_stream;
		std::ofstream _deallocation_stream;
	};

	template <typename T>
	class basic_allocator
	{
	public:
		typedef static_storage static_storage;
		typedef T value_type;
		typedef std::true_type propagate_on_container_copy_assignment;
		typedef std::true_type propagate_on_container_move_assignment;
		typedef std::true_type propagate_on_container_swap;

		template <typename U>
		struct rebin
		{
			typedef basic_allocator<U> other;
		};

		basic_allocator() noexcept {}
		basic_allocator(const basic_allocator<T>& other) noexcept {}
		template <typename U>
		basic_allocator(const basic_allocator<U>& other) noexcept {}
		~basic_allocator() {}

		T* allocate(std::size_t n, const void* hint = 0)
		{
			std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now();
			auto p = reinterpret_cast<T*>(::operator new(n * sizeof(T)));
			std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

			static_storage& ss = static_storage::get_instance();
			std::lock_guard<std::mutex> guard(ss.get_allocation_guard());
			std::thread::id tid = std::this_thread::get_id();
			magic::write_line(ss.get_allocation_stream(), "allocate;%i;%i;%i", tid, n * sizeof(T), std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

			return p;
		}

		void deallocate(T* p, std::size_t n)
		{
			std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now();
			::operator delete(p);
			std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

			static_storage& ss = static_storage::get_instance();
			std::lock_guard<std::mutex> guard(ss.get_allocation_guard());
			std::thread::id tid = std::this_thread::get_id();
			magic::write_line(ss.get_deallocation_stream(), "deallocate;%i;%i;%i", tid, n * sizeof(T), std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
		}
	};
}

#endif