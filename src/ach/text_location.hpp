#pragma once

#include <string_view>
#include <cassert>

namespace ach {

class text_location
{
public:
	text_location(std::size_t line_number, std::string_view line, std::size_t first_column, std::size_t length)
	: _line_number(line_number), _line(line), _first_column(first_column), _length(length) {}

	std::string_view line() const noexcept { return _line; }
	std::size_t line_number() const noexcept { return _line_number; }

	std::string_view str() const noexcept {
		if (_length == 0)
			return std::string_view();

		return std::string_view(_line.data() + _first_column, _length);
	}

	// note: 0-based indexing, if the highlighted text starts at the beginning this will be 0
	std::size_t first_column() const noexcept { return _first_column; }
	std::size_t length() const noexcept { return _length; }

	static text_location merge(text_location lhs, text_location rhs) noexcept {
		assert(lhs.line_number() == rhs.line_number());
		assert(lhs.line() == rhs.line());
		assert(lhs.first_column() + lhs.length() == rhs.first_column());
		return text_location(lhs.line_number(), lhs.line(), lhs.first_column(), lhs.length() + rhs.length());
	}

private:
	std::size_t _line_number = 0;
	std::string_view _line;
	std::size_t _first_column = 0;
	std::size_t _length = 0;
};

}
