#include <ach/text/extractor.hpp>
#include <ach/text/utils.hpp>

#include <algorithm>
#include <cassert>

namespace ach::text {

bool extractor::load_next_line()
{
	const auto first = _remaining_text.data();
	const auto last = _remaining_text.data() + _remaining_text.size();

	auto it = std::find(first, last, '\n');

	if (it != last) { // we want to include '\n' in the line if possible
		++it;
	}

	if (it == first)
		return false;

	// if it moved this means we started reading a new line
	++_line_number;
	_column_number = 0;

	const auto line_length = it - first;
	_current_line = std::string_view(first, line_length);
	_remaining_text.remove_prefix(line_length);
	return true;
}

template <typename Predicate>
located_span extractor::extract_by(Predicate pred)
{
	const auto text = remaining_line_str();
	const auto first = text.data();
	const auto last = text.data() + text.size();

	const auto it = std::find_if_not(first, last, pred);
	const auto length = it - first;

	return consume_n_characters(length);
}

located_span extractor::extract_non_newline_whitespace()
{
	return extract_by(is_non_newline_whitespace);
}

located_span extractor::extract_identifier()
{
	std::optional<char> c = peek_next_char();
	if (!c || is_digit(*c))
		return no_match();

	return extract_by(is_alnum_or_underscore);
}

located_span extractor::extract_alphas_underscores()
{
	return extract_by(is_alpha_or_underscore);
}

located_span extractor::extract_digits()
{
	return extract_by(is_digit);
}

located_span extractor::extract_n_characters(std::size_t n)
{
	if (n > remaining_line_str().size()) {
		return no_match();
	}

	return consume_n_characters(n);
}

located_span extractor::extract_until_end_of_line()
{
	return extract_by([](char c) { return c != '\n'; });
}

located_span extractor::extract_quoted(char quote, char escape)
{
	if (peek_next_char() != quote)
		return no_match();

	const std::string_view remaining = [this]() {
		std::string_view text = remaining_line_str();
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

	if (inside_escape) // remaining text finished before escape was fullfilled - this is an error
		return no_match();

	if (!closing_quote_found)
		return no_match();

	const auto length = 1 + (it - first); // 1 is the starting quote, rest is the loop
	return consume_n_characters(length);
}

located_span extractor::extract_match(std::string_view text_to_match)
{
	if (starts_with(remaining_line_str(), text_to_match)) {
		return consume_n_characters(text_to_match.size());
	}

	return no_match();
}

}
