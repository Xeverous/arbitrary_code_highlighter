#pragma once

#include <ach/text_extractor.hpp>

#include <string_view>

namespace ach::detail {

class code_tokenizer
{
public:
	code_tokenizer(std::string_view text)
	: extractor(text) {}

	text_location extract_identifier() {
		return extractor.extract_identifier();
	}

	text_location extract_number() {
		return extractor.extract_number();
	}

	text_location extract_n_characters(int n) {
		return extractor.extract_n_characters(n);
	}

	text_location extract_until_end_of_line() {
		return extractor.extract_until_end_of_line();
	}

	[[nodiscard]] bool has_reached_end() const noexcept { return extractor.has_reached_end(); }
	text_location current_location() const noexcept { return extractor.current_location(); }

private:
	text_extractor extractor;
};

}
