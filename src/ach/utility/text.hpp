#pragma once

#include <string_view>

namespace ach::utility {

constexpr bool is_digit(char c) noexcept
{
	return '0' <= c && c <= '9';
}

constexpr bool is_alpha(char c) noexcept
{
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

constexpr bool is_alnum(char c) noexcept
{
	return is_alpha(c) || is_digit(c);
}

constexpr bool is_alpha_or_underscore(char c) noexcept
{
	return is_alpha(c) || c == '_';
}

constexpr bool is_alnum_or_underscore(char c) noexcept
{
	return is_alnum(c) || c == '_';
}

constexpr std::size_t count_lines(std::string_view str) noexcept
{
	if (str.empty())
		return 0;

	std::size_t result = 1;
	for (char c : str)
		if (c == '\n')
			++result;

	if (str.back() == '\n')
		--result;

	return result;
}

constexpr bool starts_with(std::string_view source, std::string_view fragment) noexcept
{
	if (source.size() < fragment.size())
		return false;

	for (std::string_view::size_type i = 0; i < fragment.size(); ++i)
		if (source[i] != fragment[i])
			return false;

	return true;
}

constexpr bool ends_with(std::string_view source, std::string_view fragment) noexcept
{
	if (source.size() < fragment.size())
		return false;

	for (std::string_view::size_type i = 0; i < fragment.size(); ++i)
		if (*(source.rbegin() + i) != *(fragment.rbegin() + i))
			return false;

	return true;
}

constexpr bool contains(std::string_view source, std::string_view fragment) noexcept
{
	return source.find(fragment) != std::string_view::npos;
}

}
