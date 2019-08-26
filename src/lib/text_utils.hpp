#pragma once

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
	return is_digit(c) || is_alpha(c);
}

constexpr bool is_identifier_char(char c) noexcept
{
	return is_alpha(c) || c == '_';
}
