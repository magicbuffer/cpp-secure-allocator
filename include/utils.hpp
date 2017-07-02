#if !defined(__MAGIC_UTILS_HPP__)
#define __MAGIC_UTILS_HPP__

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

namespace magic
{
	template <typename... Args>
	std::wstring wformat(const std::wstring& format, Args... args)
	{
		std::size_t size = _snwprintf(nullptr, 0, format.c_str(), args...) + 1;
		std::unique_ptr<wchar_t[]> buffer(new wchar_t[size]);
		_snwprintf(buffer.get(), size, format.c_str(), args...);
		return std::wstring(buffer.get(), buffer.get() + size - 1);
	}

	template <typename... Args>
	std::string format(const std::string& format, Args... args)
	{
		std::size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1;
		std::unique_ptr<char[]> buffer(new char[size]);
		snprintf(buffer.get(), size, format.c_str(), args...);
		return std::string(buffer.get(), buffer.get() + size - 1);
	}

	inline void write_line(std::ostream& stream)
	{
		stream << std::endl;
	}

	inline void write_line()
	{
		std::cout << std::endl;
	}

	template <typename... Args>
	inline void write_line(std::ostream& stream, const std::string& format, Args... args)
	{
		stream << magic::format(format, args...) << std::endl;
	}

	template <typename... Args>
	inline void write(std::ostream& stream, const std::string& format, Args... args)
	{
		stream << magic::format(format, args...);
	}

	template <typename... Args>
	inline void write_line(std::wostream& stream, const std::wstring& format, Args... args)
	{
		stream << magic::wformat(format, args...) << std::endl;
	}

	template <typename... Args>
	inline void write(std::wostream& stream, const std::wstring& format, Args... args)
	{
		stream << magic::wformat(format, args...);
	}

	template <typename... Args>
	inline void write_line(const std::string& format, Args... args)
	{
		std::cout << magic::format(format, args...) << std::endl;
	}

	template <typename... Args>
	inline void write(const std::string& format, Args... args)
	{
		std::cout << magic::format(format, args...);
	}

	template <typename... Args>
	inline void write_line(const std::wstring& format, Args... args)
	{
		std::wcout << magic::wformat(format, args...) << std::endl;
	}

	template <typename... Args>
	inline void write(const std::wstring& format, Args... args)
	{
		std::wcout << magic::wformat(format, args...);
	}
}

#endif