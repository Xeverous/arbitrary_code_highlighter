#pragma once

#include <ach/clangd/state.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/semantic_token.hpp>
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
private:
	code_tokenizer(std::string_view code, utility::range<const semantic_token*> semantic_tokens)
	: m_parser(code)
	, m_all_semantic_tokens{semantic_tokens}
	, m_current_semantic_tokens{semantic_tokens.first, semantic_tokens.first}
	{}

public:
	static std::optional<code_tokenizer> create(std::string_view code, utility::range<const semantic_token*> semantic_tokens)
	{
		code_tokenizer ct(code, semantic_tokens);
		if (ct.advance_semantic_tokens())
			return ct;
		else
			return std::nullopt;
	}

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

	auto current_semantic_tokens() const noexcept
	{
		return m_current_semantic_tokens;
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
	next_code_token(utility::range<const std::string*> keywords, bool highlight_printf_formatting);

private:
	text::fragment empty_match() const noexcept
	{
		return m_parser.empty_match();
	}

	highlighter_error make_error(error_reason reason) const
	{
		return {reason, current_position(), m_current_semantic_tokens, m_context_state, m_preprocessor_state};
	}

	std::string_view semantic_token_str(semantic_token sem_token) const;

	// move to the next set of tokens that describe one (potentially spliced) entity
	[[nodiscard]] bool advance_semantic_tokens();

	void on_parsed_newline();

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token_context_none(utility::range<const std::string*> keywords);

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token_context_comment(bool is_multiline, bool is_documentation);

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token_context_quoted_literal(char delimeter, bool allow_suffix, bool highlight_printf_formatting);

	[[nodiscard]] std::variant<code_token, highlighter_error>
	next_code_token_basic(utility::range<const std::string*> keywords, bool inside_macro_body);

	[[nodiscard]] std::variant<code_token, highlighter_error>
	make_code_token_from_semantic_tokens(text::fragment identifier);

	bool is_in_macro_params(std::string_view param) const
	{
		for (auto p : m_preprocessor_macro_params)
			if (compare_spliced(p, param))
				return true;

		return false;
	}

	spliced_text_parser m_parser;

	utility::range<const semantic_token*> m_all_semantic_tokens;
	utility::range<const semantic_token*> m_current_semantic_tokens;

	preprocessor_state_t m_preprocessor_state = preprocessor_state_t::line_begin;
	context_state_t m_context_state = context_state_t::none;
	std::optional<text::position> m_disabled_code_end_pos;
	// non-empty when parsing preprocessor object-like macro
	// this will allow to highlight macro parameters inside macro body
	// note: vector content strings may be spliced
	std::vector<std::string_view> m_preprocessor_macro_params;
	// non-empty when parsing a raw string literal with non-zero length delimeter
	// (the delimeter is outside parenthesis: e.g. foo in R"foo(str)foo")
	text::fragment m_raw_string_literal_delimeter;
};

}
