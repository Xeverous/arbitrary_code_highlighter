#pragma once

#include <ach/clangd/code_token.hpp>
#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/clangd/spliced_text_parser.hpp>
#include <ach/utility/range.hpp>

#include <optional>
#include <variant>
#include <vector>

namespace ach::clangd {

namespace detail {

enum class preprocessor_state_t
{
	// before any non-whitespace character has been read
	// if the first non-whitespace character is #, it will be a preprocessor line
	line_begin,

	no_preprocessor,

	preprocessor_after_hash,

	preprocessor_after_define,
	preprocessor_after_undef,
	preprocessor_after_include,
	preprocessor_after_line,
	preprocessor_after_other // generic coloring
};

enum class context_state_t
{
	none,
	comment_single,
	comment_single_doxygen,
	comment_multi,
	comment_multi_doxygen,
	comment_end,
	literal_character,
	literal_string,
	literal_end_optional_suffix,
	literal_string_raw_quote_open,
	literal_string_raw_delimeter_open,
	literal_string_raw_paren_open,
	literal_string_raw_body,
	literal_string_raw_paren_close,
	literal_string_raw_delimeter_close,
	literal_string_raw_quote_close,
};

}

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

	std::variant<code_token, highlighter_error> next_code_token(utility::range<const std::string*> keywords);

private:
	text::fragment empty_match() const noexcept
	{
		return m_parser.empty_match();
	}

	highlighter_error make_error(error_reason reason) const
	{
		return highlighter_error{current_position(), m_current_semantic_tokens, reason};
	}

	std::string_view semantic_token_str(semantic_token sem_token) const;

	// move to the next set of tokens that describe one (potentially spliced) entity
	[[nodiscard]] bool advance_semantic_tokens();

	std::variant<code_token, highlighter_error> next_code_token_context_none(utility::range<const std::string*> keywords);
	std::variant<code_token, highlighter_error> next_code_token_context_comment(bool is_multiline, bool is_documentation);
	std::variant<code_token, highlighter_error> next_code_token_context_quoted_literal(char delimeter, bool allow_suffix);
	std::variant<code_token, highlighter_error> next_code_token_no_preprocessor(utility::range<const std::string*> keywords);

	spliced_text_parser m_parser;

	utility::range<const semantic_token*> m_all_semantic_tokens;
	utility::range<const semantic_token*> m_current_semantic_tokens;

	detail::preprocessor_state_t m_preprocessor_state = detail::preprocessor_state_t::line_begin;
	detail::context_state_t m_context_state = detail::context_state_t::none;
	std::optional<text::position> m_disabled_code_end_pos;
	// non-empty when parsing preprocessor object-like macro
	// this will allow to highlight macro parameters inside macro body
	std::vector<std::string_view> m_preprocessor_param_names;
	// non-empty when parsing a raw string literal with non-zero length delimeter
	// (the delimeter is outside parenthesis: e.g. foo in R"foo(str)foo")
	text::fragment m_raw_string_literal_delimeter;
};

}
