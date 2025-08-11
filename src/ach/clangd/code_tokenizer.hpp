#pragma once

#include <ach/clangd/state.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/clangd/spliced_text_parser.hpp>
#include <ach/utility/range.hpp>

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace ach::clangd {

class code_tokenizer
{
public:
	code_tokenizer(std::string_view code, utility::range<const std::string*> keywords)
	: m_keywords{keywords}
	, m_parser(code)
	{}

	// note: this doesn't mean the parser is finished
	// if it reaches end it may still emit some tokens (e.g. comment_end) before end_of_input
	bool has_reached_end() const noexcept
	{
		return m_parser.has_reached_end();
	}

	text::position current_position() const noexcept
	{
		return m_parser.current_position();
	}

	context_state_t current_context_state() const noexcept
	{
		return m_context_state;
	}

	preprocessor_state_t current_preprocessor_state() const noexcept
	{
		return m_preprocessor_state;
	}

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token(bool highlight_printf_formatting);

	// Clear vector and fill it with all remaining tokens.
	// If an error occurs, partial fill may happen.
	[[nodiscard]] std::optional<highlighter_error>
	fill_with_tokens(bool highlight_printf_formatting, std::vector<code_token>& tokens);

private:
	text::fragment empty_match() const noexcept
	{
		return m_parser.empty_match();
	}

	highlighter_error make_error(error_reason reason) const
	{
		return highlighter_error::from_parser(reason, current_position(), m_context_state, m_preprocessor_state);
	}

	void on_parsed_newline();

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token_context_none();

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token_context_comment(bool is_multiline, bool is_documentation);

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token_context_quoted_literal(char delimeter, bool allow_suffix, bool highlight_printf_formatting);

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token_basic(bool inside_macro_body);

	bool is_in_macro_params(std::string_view param) const
	{
		for (auto p : m_preprocessor_macro_params)
			if (compare_spliced(p, param))
				return true;

		return false;
	}

	utility::range<const std::string*> m_keywords;
	spliced_text_parser m_parser;

	// simple state machine to improve decision making on the parser

	preprocessor_state_t m_preprocessor_state = preprocessor_state_t::line_begin;
	context_state_t m_context_state = context_state_t::none;
	// non-empty when parsing preprocessor object-like macro
	// this will allow to highlight macro parameters inside macro body
	// note: vector content strings may be spliced
	std::vector<std::string_view> m_preprocessor_macro_params;
	// non-empty when parsing a raw string literal with non-zero length delimeter
	// (the delimeter is outside parenthesis: e.g. foo in R"foo(str)foo")
	text::fragment m_raw_string_literal_delimeter;
};

}
