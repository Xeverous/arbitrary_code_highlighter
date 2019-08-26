#include "color_tokenizer.hpp"
#include "text_utils.hpp"

#include <charconv>

color_token color_tokenizer::next_token()
{
	std::optional<char> c = extractor.peek_next_char();
	if (!c) {
		return color_token{end_of_input{}, extractor.get_current_location()};
	}

	char const next_char = *c;
	if (is_identifier_char(next_char)) {
		text_location const loc = extractor.extract_identifier();
		return color_token{identifier_span{loc.str()}, loc};
	}
	else if (is_digit(next_char)) {
		text_location const loc_num = extractor.extract_number();
		text_location const loc_id = extractor.extract_identifier();

		auto const num_str = loc_num.str();
		int num = 0;
		if (auto const [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), num); ec != std::errc()) {
			return color_token{invalid_token{ "failed to parse number" }, loc_num};
		}

		if (num == 0) {
			return color_token{
				line_delimited_span{loc_id.str()},
				text_location::merge(loc_num, loc_id)};
		}

		return color_token{
			fixed_length_span{loc_id.str(), num, loc_id, loc_num},
			text_location::merge(loc_num, loc_id)};
	}
	else if (next_char == '\n') {
		return color_token{end_of_line{}, extractor.extract_n_characters(1)};
	}

	// unknown character - treat as a symbol token
	text_location const loc = extractor.extract_n_characters(1);
	return color_token{symbol{loc.str().front()}, loc};
}
