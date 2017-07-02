#if !defined(__MAGIC_WINDOWS_SECURE_ALLOCATOR_MK1_HPP__)
#define __MAGIC_WINDOWS_SECURE_ALLOCATOR_MK1_HPP__

// Ours
#include "utils.hpp"
// WinAPI
#include <windows.h>
// STL
#include <exception>
#include <cstdint>
#include <mutex>
#include <map>
#include <atomic>
#include <fstream>
#include <ios>

namespace magic
{
	class static_storage_mk1
	{
	public:
		static static_storage_mk1& get_instance()
		{
			static static_storage_mk1 instance;
			return instance;
		}

		DWORD get_page_size() const
		{
			return _page_size;
		}

		std::mutex& get_allocation_guard()
		{
			return _allocation_guard;
		}

		std::map<std::uintptr_t, std::uint32_t>& get_locks()
		{
			return _locks;
		}

		std::ofstream& get_allocation_stream()
		{
			if (!_allocation_stream.is_open())
			{
				_allocation_stream.open("windows_secure_allocator_mk1.allocation.csv", std::ios_base::trunc);
			}
			return _allocation_stream;
		}

		std::ofstream& get_deallocation_stream()
		{
			if (!_deallocation_stream.is_open())
			{
				_deallocation_stream.open("windows_secure_allocator_mk1.deallocation.csv", std::ios_base::trunc);
			}
			return _deallocation_stream;
		}

	private:
		static_storage_mk1()
		{
			SYSTEM_INFO info;
			GetSystemInfo(&info);
			_page_size = info.dwPageSize;
		}

		static_storage_mk1(static_storage_mk1 const&) = delete;
		void operator=(static_storage_mk1 const&) = delete;

		DWORD _page_size;
		std::mutex _allocation_guard;
		std::map<std::uintptr_t, std::uint32_t> _locks;
		std::ofstream _allocation_stream;
		std::ofstream _deallocation_stream;
	};

	template <typename T>
	class windows_secure_allocator_mk1
	{
	public:
		typedef static_storage_mk1 static_storage;
		typedef T value_type;
		typedef std::true_type propagate_on_container_copy_assignment;
		typedef std::true_type propagate_on_container_move_assignment;
		typedef std::true_type propagate_on_container_swap;

		template <typename U>
		struct rebin
		{
			typedef windows_secure_allocator_mk1<U> other;
		};

		windows_secure_allocator_mk1() noexcept {}
		windows_secure_allocator_mk1(const windows_secure_allocator_mk1<T>& other) noexcept{}
		template <typename U>
		windows_secure_allocator_mk1(const windows_secure_allocator_mk1<U>& other) noexcept {}
		~windows_secure_allocator_mk1() {}

		T* allocate(std::size_t n, const void* hint = 0)
		{
			std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now();

#if defined(_DEBUG)
			auto& locks = static_storage::get_instance().get_locks();
#endif
			static_storage& ss = static_storage::get_instance();

			std::size_t region_size = n * sizeof(T);
			T* p = reinterpret_cast<T*>(::operator new(region_size));

			std::uintptr_t region_start = reinterpret_cast<std::uintptr_t>(p);
			std::uintptr_t region_end = region_start + region_size;
			std::uintptr_t page_size = ss.get_page_size();
			std::uintptr_t page_address = region_start / page_size * page_size;

			std::lock_guard<std::mutex> guard(ss.get_allocation_guard());
			while (page_address < region_end)
			{
				if (++ss.get_locks()[page_address] == 1)
				{
					if (VirtualLock(reinterpret_cast<void*>(page_address), page_size) == 0)
					{
						::operator delete(p);
						magic::write_line("Unable to lock memory region of '%i' bytes at address '0x%08x'. Windows Error: '0x%08x'", region_size, p, GetLastError());
						throw std::bad_alloc();
					}
				}

				page_address += page_size;
			}

			std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
			std::thread::id tid = std::this_thread::get_id();
			magic::write_line(ss.get_allocation_stream(), "allocate;%i;%i;%i", tid, region_size, std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());

			return p;
		}

		void deallocate(T* p, std::size_t n)
		{
			std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now();
#if defined(_DEBUG)
			auto& locks = static_storage::get_instance().get_locks();
#endif

			static_storage& ss = static_storage::get_instance();

			std::size_t region_size = n * sizeof(T);
			std::uintptr_t region_start = reinterpret_cast<std::uintptr_t>(p);
			std::uintptr_t region_end = region_start + region_size;
			std::uintptr_t page_size = ss.get_page_size();
			std::uintptr_t page_address = region_start / page_size * page_size;

			std::lock_guard<std::mutex> guard(ss.get_allocation_guard());
			while (page_address < region_end)
			{
				if (--ss.get_locks()[page_address] == 0)
				{
					if (VirtualUnlock(reinterpret_cast<void*>(page_address), page_size) == 0)
					{
						magic::write_line("Unable to unlock memory region of '%i' bytes at address '0x%08x'. Windows Error: '0x%08x'", region_size, p, GetLastError());
						throw std::bad_alloc();
					}

					ss.get_locks().erase(page_address);
				}

				page_address += page_size;
			}

			::operator delete(p);

			std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
			std::thread::id tid = std::this_thread::get_id();
			magic::write_line(ss.get_deallocation_stream(), "deallocate;%i;%i;%i", tid, region_size, std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
		}
	private:
		static DWORD _page_size;
	};
}

#endif