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
	: remaining_text(text)
	{
		load_next_line();
	}

	std::optional<char> peek_next_char() const noexcept {
		auto const remaining = remaining_line_text();

		if (remaining.empty()) {
			return std::nullopt;
		}

		return remaining.front();
	}

	[[nodiscard]] bool has_reached_end() const noexcept {
		return remaining_text.empty() && current_line.empty();
	}

	text_location current_location() const noexcept {
		return text_location(line_number, current_line, column_number, 0);
	}

	// if any of these fails, a location with empty substring should be returned
	text_location extract_alphas_underscores();
	text_location extract_digits();
	text_location extract_n_characters(int n);
	text_location extract_until_end_of_line();
	text_location extract_quoted(char quote, char escape);

private:
	void skip(int n) {
		assert(n <= static_cast<int>(remaining_line_text().size()));
		column_number += n;
		if (remaining_line_text().empty())
			load_next_line();
	}

	void load_next_line();

	std::string_view remaining_line_text() const noexcept {
		auto result = std::string_view(current_line);
		assert(column_number <= static_cast<int>(result.size()));
		result.remove_prefix(column_number);
		return result;
	}

	// implemented and instantiated only in source
	template <typename Predicate>
	text_location extract_by(Predicate pred);

	std::string_view remaining_text;
	std::string_view current_line;
	int line_number = 0;
	int column_number = 0;
};

}
