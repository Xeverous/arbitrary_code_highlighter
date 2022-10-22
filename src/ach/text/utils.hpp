#pragma once

#include <string_view>

namespace ach::text {

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

}
