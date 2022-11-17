#pragma once

#include <ach/text/types.hpp>
#include <ach/text/utils.hpp>

#include <algorithm>
#include <string_view>

/**
 * @file splice utils
 *
 * https://eel.is/c++draft/lex.phases#1.2
 * Each sequence of a backslash character (\) immediately followed by zero or more whitespace characters other than
 * new-line followed by a new-line character is deleted, splicing physical source lines to form logical source lines.
 * Only the last backslash on any physical source line shall be eligible for being part of such a splice.
 */

namespace ach::clangd {

/**
 * @brief check if there is a splice at the beginning of the string
 * if so, return number of bytes it covers
 *
 * @details return 0 means no splice, 2+ means number of bytes it occupies
 * value of 1 is impossible to be returned
 *
 * '\\', 0+ non-newline whitespace, '\n' is a splice
 * '\\', 0+ non-newline whitespace, <end> is not
 */
constexpr std::size_t front_splice_length(std::string_view str)
{
	if (str.empty())
		return 0;

	std::size_t i = 0;
	if (str[i++] != '\\')
		return 0;

	while (i < str.size()) {
		if (text::is_non_newline_whitespace(str[i]))
			++i;
		else
			break;
	}

	if (i == str.size())
		return 0;

	if (str[i] == '\n')
		return ++i;
	else
		return 0;
}

constexpr void remove_front_splices(std::string_view& text, text::position& pos)
{
	while (true) {
		const auto length = front_splice_length(text);
		if (length == 0)
			return;

		text.remove_prefix(length);
		pos = pos.next_line();
	}
}

constexpr void remove_front_splices(std::string_view& text)
{
	while (true) {
		const auto length = front_splice_length(text);
		if (length == 0)
			return;

		text.remove_prefix(length);
	}
}

inline bool ends_with_backslash_whitespace(std::string_view text)
{
	const auto it = std::find_if_not(text.rbegin(), text.rend(), text::is_whitespace);
	if (it == text.rend())
		return false;

	return *it == '\\';
}

inline bool compare_potentially_spliced(text::fragment frag, std::string_view sv)
{
	std::string_view str = frag.str;

	for (char c : sv) {
		remove_front_splices(str);

		if (str.empty())
			return false;

		if (str.front() != c)
			return false;

		str.remove_prefix(1u);
	}

	remove_front_splices(str);
	return str.empty();
}

}
