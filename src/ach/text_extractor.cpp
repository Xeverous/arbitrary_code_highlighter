#include <ach/text_extractor.hpp>
#include <ach/detail/text_utils.hpp>

#include <algorithm>
#include <cassert>

namespace ach {

void text_extractor::load_next_line()
{
	auto const first = remaining_text.data();
	auto const last = remaining_text.data() + remaining_text.size();

	auto it = std::find(first, last, '\n');

	if (it != last) { // we want to include '\n' in the line if possible
		++it;
	}

	if (it != first) { // if it moved this means we started reading a new line
		++line_number;
		column_number = 0;
	}

	auto const line_length = it - first;
	current_line = std::string_view(first, line_length);
	remaining_text.remove_prefix(line_length);
}

template <typename Predicate>
text_location text_extractor::extract_by(Predicate pred)
{
	auto const text = remaining_line_text();
	auto const first = text.data();
	auto const last = text.data() + text.size();

	auto const it = std::find_if_not(first, last, pred);
	auto const length = it - first;

	auto const result = text_location(line_number, current_line, column_number, length);
	skip(length);
	return result;
}

text_location text_extractor::extract_identifier()
{
	std::optional<char> const next_char = peek_next_char();

	if (!next_char) {
		return text_location();
	}

	if (!detail::is_alpha_or_underscore(*next_char)) {
		return text_location();
	}

	return extract_by(detail::is_alnum_or_underscore);
}

text_location text_extractor::extract_alphas_underscores()
{
	return extract_by(detail::is_alpha_or_underscore);
}

text_location text_extractor::extract_digits()
{
	return extract_by(detail::is_digit);
}

text_location text_extractor::extract_n_characters(int n)
{
	if (n > static_cast<int>(remaining_line_text().size())) {
		n = 0;
	}

	auto const result = text_location(line_number, current_line, column_number, n);
	skip(n);
	return result;
}

text_location text_extractor::extract_until_end_of_line()
{
	return extract_by([](char c) { return c != '\n'; });
}

text_location text_extractor::extract_quoted(char quote, char escape)
{
	if (peek_next_char() != quote) {
		return text_location();
	}

	std::string_view const remaining = [this]() {
		std::string_view text = remaining_line_text();
		assert(text.size() >= 1u);
		text.remove_prefix(1u);
		return text;
	}();

	const auto first = remaining.begin();
	const auto last  = remaining.end();

	bool inside_escape = false;
	bool closing_quote_found = false;
	auto it = first;
	for (; it != last; ++it) {
		if (inside_escape) {
			inside_escape = false;
			continue; // we don't really care what is being escaped
		}

		if (*it == escape) {
			inside_escape = true;
			continue;
		}

		if (*it == quote) {
			++it;
			closing_quote_found = true;
			break;
		}
	}

	if (inside_escape) { // remaining text finished before escape was fullfilled - this is an error
		return text_location();
	}

	if (!closing_quote_found) {
		return text_location();
	}

	auto const length = 1 + (it - first); // 1 is the starting quote, rest is the loop
	auto const result = text_location(line_number, current_line, column_number, length);
	skip(length);
	return result;
}

}
