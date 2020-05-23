#pragma once

#include <ach/text_location.hpp>

#include <string_view>
#include <optional>
#include <cassert>

namespace ach {

class text_extractor
{
public:
	text_extractor(std::string_view text)
	: _remaining_text(text)
	{
		// ignore if loading fails, invariants will be preserved
		(void) load_next_line();
	}

	std::optional<char> peek_next_char() const noexcept {
		auto const remaining = remaining_line_str();

		if (remaining.empty()) {
			return std::nullopt;
		}

		return remaining.front();
	}

	[[nodiscard]] bool has_reached_end() const noexcept {
		return _remaining_text.empty() && remaining_line_str().empty();
	}

	text_location remaining_line_text() const noexcept {
		return text_location(_line_number, _current_line, _column_number, _current_line.size() - _column_number);
	}

	// 0-length match, also returned as an error when extracting fails
	text_location current_location() const noexcept {
		return text_location(_line_number, _current_line, _column_number, 0);
	}

	// if any of these fails, a location with length() == 0 should be returned
	text_location extract_identifier();
	text_location extract_alphas_underscores();
	text_location extract_digits();
	text_location extract_n_characters(std::size_t n);
	text_location extract_until_end_of_line();
	text_location extract_quoted(char quote, char escape);

	[[nodiscard]] bool load_next_line();

private:
	void skip(std::size_t n) {
		assert(n <= remaining_line_str().size());
		_column_number += n;
	}

	std::string_view remaining_line_str() const noexcept {
		std::string_view result = _current_line;
		assert(_column_number <= result.size());
		result.remove_prefix(_column_number);
		return result;
	}

	// implemented and instantiated only in source
	template <typename Predicate>
	text_location extract_by(Predicate pred);

	std::string_view _remaining_text;
	std::string_view _current_line;
	std::size_t _line_number = 0;   // 1-based index, incremented on each line load (first loaded line is 1st)
	std::size_t _column_number = 0; // number of columns consumed, 0-based index
};

}
