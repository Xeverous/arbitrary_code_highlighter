#pragma once

#include <ach/text/extractor.hpp>

#include <string_view>

namespace ach::mirror {

class code_tokenizer
{
public:
	code_tokenizer(std::string_view text)
	: extractor(text) {}

	text::location extract_identifier() {
		return extractor.extract_identifier();
	}

	text::location extract_digits() {
		return extractor.extract_digits();
	}

	text::location extract_n_characters(std::size_t n) {
		return extractor.extract_n_characters(n);
	}

	text::location extract_until_end_of_line() {
		return extractor.extract_until_end_of_line();
	}

	text::location extract_quoted(char quote, char escape) {
		return extractor.extract_quoted(quote, escape);
	}

	[[nodiscard]] bool has_reached_end() const noexcept { return extractor.has_reached_end(); }
	text::location current_location() const noexcept { return extractor.current_location(); }

	[[nodiscard]] bool load_next_line() {
		return extractor.load_next_line();
	}

	text::location remaining_line_text() const noexcept {
		return extractor.remaining_line_text();
	}

private:
	text::extractor extractor;
};

}
