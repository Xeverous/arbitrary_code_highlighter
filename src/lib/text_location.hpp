#pragma once

#include <string_view>
#include <cassert>

class text_location
{
public:
	text_location(int line_number, std::string_view line, int first_column, int length)
	: line_number(line_number), whole_line(line), str_first_column(first_column), length(length) {}

	std::string_view line() const noexcept { return whole_line; }

	std::string_view str() const noexcept {
		return std::string_view(whole_line.data() + str_first_column, length);
	}

	int first_column() const noexcept { return str_first_column; }

	static text_location merge(text_location lhs, text_location rhs) noexcept {
		assert(lhs.line_number == rhs.line_number);
		assert(lhs.line == rhs.line);
		assert(lhs.str_first_column + lhs.length == rhs.str_first_column);
		return text_location(lhs.line_number, lhs.whole_line, lhs.str_first_column, lhs.length + rhs.length);
	}

private:
	int line_number = 0;
	std::string_view whole_line;
	int str_first_column = 0;
	int length = 0;
};
