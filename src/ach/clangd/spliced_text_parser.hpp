#pragma once

#include <ach/clangd/spliced_text_iterator.hpp>
#include <ach/text/types.hpp>

#include <string_view>

namespace ach::clangd {

class spliced_text_parser
{
public:
	spliced_text_parser(std::string_view text)
	: m_iterator(text) {}

	bool has_reached_end() const noexcept
	{
		return m_iterator == spliced_text_iterator();
	}

	text::position current_position() const noexcept
	{
		return m_iterator.position();
	}

	text::fragment empty_match() const noexcept
	{
		return {{}, {current_position(), current_position()}};
	}

	text::fragment parse_exactly(char c);
	text::fragment parse_exactly(std::string_view str);
	text::fragment parse_newline()
	{
		return parse_exactly('\n');
	}

	text::fragment parse_non_newline_whitespace();
	text::fragment parse_digits();
	text::fragment parse_identifier();
	text::fragment parse_numeric_literal();
	text::fragment parse_text_literal_prefix(char quote);
	text::fragment parse_char_literal_prefix() { return parse_text_literal_prefix('\''); }
	text::fragment parse_string_literal_prefix() { return parse_text_literal_prefix('"'); }
	text::fragment parse_raw_string_literal_prefix();
	text::fragment parse_raw_string_literal_delimeter_open();
	text::fragment parse_raw_string_literal_body(std::string_view delimeter);
	text::fragment parse_raw_string_literal_delimeter_close(std::string_view delimeter)
	{
		return parse_exactly(delimeter);
	}
	text::fragment parse_symbols(); // TODO vague name

	text::fragment parse_comment_tag_todo();
	text::fragment parse_comment_tag_doc();
	text::fragment parse_comment_single_body();
	text::fragment parse_comment_single_doc_body();
	text::fragment parse_comment_multi_body();
	text::fragment parse_comment_multi_doc_body();

	// quoted string, nothing special inside like escape sequences
	// intended for specific parts of the preprocessor
	text::fragment parse_quoted(char begin_delimeter, char end_delimeter);
	text::fragment parse_quoted(char delimeter)
	{
		return parse_quoted(delimeter, delimeter);
	}

	text::fragment parse_escape_sequence();
	text::fragment parse_text_literal_body(char delimeter);

private:
	template <typename Parser>
	text::fragment parse(Parser&& parser);

	text::fragment return_parse_result(bool is_success, spliced_text_iterator updated_iterator);

	spliced_text_iterator m_iterator;
};

}