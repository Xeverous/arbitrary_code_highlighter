#include "text_extractor.hpp"
#include "text_utils.hpp"

#include <algorithm>
#include <cassert>

void text_extractor::load_next_line()
{
	auto const first = remaining_text.data();
	auto const last = remaining_text.data() + remaining_text.size();

	auto it = std::find(first, last, '\n');
	if (it != last) { // we want to include '\n' in the line if possible
		++it;
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
	return extract_by(is_identifier_char);
}

text_location text_extractor::extract_number()
{
	return extract_by(is_digit);
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
