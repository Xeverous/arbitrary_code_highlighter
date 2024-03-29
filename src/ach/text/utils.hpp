#pragma once

#include <string_view>

namespace ach::text {

constexpr bool is_non_newline_whitespace(char c) noexcept
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

constexpr bool is_whitespace(char c) noexcept
{
	return c == '\n' || is_non_newline_whitespace(c);
}

constexpr bool is_digit_binary(char c) noexcept
{
	return c == '0' || c == '1';
}

constexpr bool is_digit(char c) noexcept
{
	return '0' <= c && c <= '9';
}

constexpr bool is_digit_octal(char c) noexcept
{
	return '0' <= c && c <= '7';
}

constexpr bool is_digit_hex(char c) noexcept
{
	return is_digit(c) || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
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

// https://en.cppreference.com/w/cpp/language/charset#Basic_character_set
constexpr bool is_from_basic_character_set(char c) noexcept
{
	return
		(0x09 <= c && c <= 0x0c)
		|| (0x20 <= c && c <= 0x3f)
		|| (0x41 <= c && c <= 0x5f)
		|| (0x61 <= c && c <= 0x7e);
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
