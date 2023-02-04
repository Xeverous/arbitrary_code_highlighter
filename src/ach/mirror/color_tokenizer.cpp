#include <ach/mirror/color_tokenizer.hpp>
#include <ach/mirror/errors.hpp>
#include <ach/text/utils.hpp>

#include <charconv>

namespace ach::mirror {

color_token color_tokenizer::next_token(color_options options)
{
	std::optional<char> c = extractor.peek_next_char();
	if (!c) {
		if (!extractor.load_next_line())
			return color_token{end_of_input{}, extractor.current_location()};

		c = extractor.peek_next_char();
		if (!c)
			return color_token{end_of_input{}, extractor.current_location()};
	}

	const char next_char = *c;
	if (text::is_alpha_or_underscore(next_char)) {
		const text::located_span extracted_text = extractor.extract_alphas_underscores();
		const std::string_view identifier = extracted_text.str();

		if (identifier == options.num_keyword) {
			return color_token{number{options.num_class}, extracted_text};
		}

		if (identifier == options.str_keyword) {
			return color_token{
				quoted_span{{options.str_class}, {options.str_esc_class}, '"', options.escape_char},
				extracted_text};
		}

		if (identifier == options.chr_keyword) {
			return color_token{
				quoted_span{{options.chr_class}, {options.chr_esc_class}, '\'', options.escape_char},
				extracted_text};
		}

		return color_token{identifier_span{identifier}, extracted_text};
	}
	else if (text::is_digit(next_char)) {
		const text::located_span extracted_digits = extractor.extract_digits();
		text::located_span extracted_name = extractor.extract_alphas_underscores();

		const auto num_str = extracted_digits.str();
		std::size_t num = 0;
		if (std::from_chars(num_str.data(), num_str.data() + num_str.size(), num).ec != std::errc()) {
			return color_token{invalid_token{errors::invalid_number}, extracted_digits};
		}

		std::optional<web::css_class> class_ = web::css_class{extracted_name.str()};
		if (extracted_name.str().empty()) {
			if (extractor.peek_next_char() == options.empty_token_char) {
				extracted_name = extractor.extract_n_characters(1);
				class_ = std::nullopt;
			}
			else {
				return color_token{invalid_token{errors::expected_span_class}, extracted_digits};
			}
		}

		if (num == 0) {
			return color_token{
				line_delimited_span{class_},
				text::located_span::merge(extracted_digits, extracted_name)};
		}

		return color_token{
			fixed_length_span{class_, num, extracted_name, extracted_digits},
			text::located_span::merge(extracted_digits, extracted_name)};
	}
	else if (next_char == '\n') {
		return color_token{end_of_line{}, extractor.extract_n_characters(1)};
	}
	else if (next_char == options.empty_token_char) {
		return color_token{empty_token{}, extractor.extract_n_characters(1)};
	}

	// unknown character - treat as a symbol token
	const text::located_span extracted_symbol = extractor.extract_n_characters(1);
	return color_token{symbol{extracted_symbol.str().front()}, extracted_symbol};
}

}
