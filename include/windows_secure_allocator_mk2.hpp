#if !defined(__MAGIC_WINDOWS_SECURE_ALLOCATOR_MK2_HPP__)
#define __MAGIC_WINDOWS_SECURE_ALLOCATOR_MK2_HPP__

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

namespace magic
{
	class lock_descriptor
	{
	public:
		lock_descriptor() : _count(1) {}
		lock_descriptor(std::uint32_t count) : _count(count) {}

		std::mutex& get_guard()
		{
			return _lock;
		}

		std::atomic<std::uint32_t>& get_count()
		{
			return _count;
		}

	private:
		std::mutex _lock;
		std::atomic<std::uint32_t> _count;
	};

	class static_storage_mt2
	{
	public:
		static static_storage_mt2& get_instance()
		{
			static static_storage_mt2 instance;
			return instance;
		}

		DWORD get_page_size() const
		{
			return _page_size;
		}

		std::mutex& get_locks_guard()
		{
			return _locks_guard;
		}

		std::map<std::uintptr_t, lock_descriptor>& get_locks()
		{
			return _locks;
		}

	private:
		static_storage_mt2()
		{
			SYSTEM_INFO info;
			GetSystemInfo(&info);
			_page_size = info.dwPageSize;
		}

		static_storage_mt2(static_storage_mt2 const&) = delete;
		void operator=(static_storage_mt2 const&) = delete;

		DWORD _page_size;

		std::mutex _locks_guard;
		std::map<std::uintptr_t, lock_descriptor> _locks;
	};

	template <typename T>
	class windows_secure_allocator_mt2
	{
	public:
		typedef static_storage_mt2 static_storage;
		typedef T value_type;
		typedef std::true_type propagate_on_container_copy_assignment;
		typedef std::true_type propagate_on_container_move_assignment;
		typedef std::true_type propagate_on_container_swap;

		template <typename U>
		struct rebin
		{
			typedef windows_secure_allocator_mt2<U> other;
		};

		windows_secure_allocator_mt2() noexcept
		{
			//magic::write_line("windows_secure_allocator_mt2::windows_secure_allocator_mt2()");
		}

		windows_secure_allocator_mt2(const windows_secure_allocator_mt2<T>& other) noexcept
		{
			//magic::write_line("windows_secure_allocator_mt2::windows_secure_allocator_mt2(const windows_secure_allocator_mt2<T>&)");
		}

		template <typename U>
		windows_secure_allocator_mt2(const windows_secure_allocator_mt2<U>& other) noexcept
		{
			//magic::write_line("windows_secure_allocator_mt2::windows_secure_allocator_mt2(const windows_secure_allocator_mt2<U>&)");
		}

		~windows_secure_allocator_mt2()
		{
			//magic::write_line("windows_secure_allocator_mt2::~windows_secure_allocator_mt2()");
		}

		T* allocate(std::size_t n, const void* hint = 0)
		{
			//magic::write_line("windows_secure_allocator_mt2::allocate(%i, %i)", n, hint);

			auto& locks = static_storage::get_instance().get_locks();

			std::size_t size = n * sizeof(T);
			T* p = reinterpret_cast<T*>(::operator new(size));

			std::uintptr_t page_start = reinterpret_cast<std::uintptr_t>(p);
			std::uintptr_t page_size = static_storage::get_instance().get_page_size();
			std::uintptr_t page_address = (page_start / page_size) * page_size;
			std::uintptr_t page_end = page_start + size;

			while (page_address < page_end)
			{
				auto descriptor = static_storage::get_instance().get_locks().find(page_address);

				// Does the map already contains an entry
				if (descriptor == static_storage::get_instance().get_locks().end())
				{
					// The value does not seem to exist yet, we acquire the lock to create it
					// Note: The value might have been inserted in the map while waiting for the lock
					std::lock_guard<std::mutex> guard_locks(static_storage::get_instance().get_locks_guard());

					// Get a fresh descriptor
					descriptor = static_storage::get_instance().get_locks().find(page_address);
					if (descriptor != static_storage::get_instance().get_locks().end())
					{
						// Somebody already locked the page while we were waiting to acquire the lock
						// Increment the descriptor so the other know we are using this page as well3
						++descriptor->second.get_count();
						continue;
					}

					// The descriptor still does not exist, we can safely write to the map
					static_storage::get_instance().get_locks().emplace(page_address, 1);

					// FIXME: Do we need to acquire the lock for this page??
					magic::write_line("### Locking memory page at address '0x%08x'", page_address);
					if (VirtualLock(reinterpret_cast<void*>(page_address), page_size) == 0)
					{
						::operator delete(p);
						magic::write_line("Unable to lock memory region of '%i' bytes at address '0x%08x'. Windows Error: '0x%08x'", size, p, GetLastError());
						throw std::bad_alloc();
					}
				}
				else
				{
					// It seems like we are the only one using this page but we need to be sure
					std::lock_guard<std::mutex> guard_descriptor(descriptor->second.get_guard());

					// Okay we can proceed, ensure the page still need to be locked
					if (descriptor->second.get_count().load() == 0)
					{
						magic::write_line("### Locking memory page at address '0x%08x'", page_address);
						if (VirtualLock(reinterpret_cast<void*>(page_address), page_size) == 0)
						{
							::operator delete(p);
							magic::write_line("Unable to lock memory region of '%i' bytes at address '0x%08x'. Windows Error: '0x%08x'", size, p, GetLastError());
							throw std::bad_alloc();
						}

						++descriptor->second.get_count();
					}
				}

				page_address += page_size;
			}

			return p;
		}

		void deallocate(T* p, std::size_t n)
		{
			//magic::write_line("windows_secure_allocator_mt2::deallocate(0x%08x, %i)", p, n);

			auto& locks = static_storage::get_instance().get_locks();

			std::size_t size = n * sizeof(T);
			std::uintptr_t page_start = reinterpret_cast<std::uintptr_t>(p);
			std::uintptr_t page_size = static_storage::get_instance().get_page_size();
			std::uintptr_t page_address = (page_start / page_size) * page_size;
			std::uintptr_t page_end = page_start + size;

			while (page_address < page_end)
			{
				auto descriptor = static_storage::get_instance().get_locks().find(page_address);

				// Does the map already contains an entry
				if (descriptor == static_storage::get_instance().get_locks().end())
				{
					// This is not supposed to happen
					magic::write_line("Major fuckup detected ... aborting");
					throw std::bad_alloc();
				}

				if (descriptor->second.get_count().fetch_sub(1) == 1)
				{
					// It seems like we are the only one using this page but we need to be sure
					std::lock_guard<std::mutex> guard_descriptor(descriptor->second.get_guard());
					
					// Okay we can proceed, ensure the page still need to be unlocked
					if (descriptor->second.get_count().load() == 0)
					{
						magic::write_line("### Unlocking memory page at address '0x%08x'", page_address);
						// We are the last one using this page, it is time to unlock it
						if (VirtualUnlock(reinterpret_cast<void*>(page_address), page_size) == 0)
						{
							magic::write_line("Unable to unlock memory region of '%i' bytes at address '0x%08x'. Windows Error: '0x%08x'", size, p, GetLastError());
							throw std::bad_alloc();
						}

						// Erase the entry from the map to keep the map clean
						// Note: We might omit the additional cleaning to potentially increase performance at the cost of using more memory
						//std::lock_guard<std::mutex> guard_locks(static_storage::get_instance().get_locks_guard());
						//static_storage::get_instance().get_locks().erase(page_address);
					}
				}

				page_address += page_size;
			}

			::operator delete(p);
		}
	private:
		static DWORD _page_size;
	};
}

#endif