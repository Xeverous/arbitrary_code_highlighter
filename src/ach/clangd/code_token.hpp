#pragma once

#include <ach/clangd/semantic_token.hpp>
#include <ach/text/types.hpp>
#include <ach/utility/enum.hpp>
#include <ach/utility/visitor.hpp>

#include <variant>
#include <ostream>

namespace ach::clangd {

ACH_RICH_ENUM_CLASS(syntax_token,
	// # (both as preprocessor command and stringization) and ##
	(preprocessor_hash)
	// without preceeding #, only identifier - there may be whitespace between # and the directive
	(preprocessor_directive)
	// with "" or <>, may appear after #include, after #line and inside __has_include()
	(preprocessor_header_file)
	(preprocessor_macro)
	// parameters of function-like macros
	(preprocessor_macro_param)
	(preprocessor_macro_body)
	// anything else not specified earlier: text after #error, macro bodies and more
	// literals and keywords inside macro bodies will use their regular token types
	(preprocessor_other)

	(disabled_code_begin)
	(disabled_code_end)

	(comment_begin_single)
	(comment_begin_single_doxygen)
	(comment_begin_multi)
	(comment_begin_multi_doxygen)
	(comment_end)
	// inside any comment, stuff like TODO and FIXME
	(comment_tag_todo)
	// only inside doxygen documentation comments, stuff like @brief
	(comment_tag_doxygen)

	(keyword)

	// Identifiers should be reported using their dedicated type.
	// Use this when an identifier has no semantic token information.
	(identifier_unknown)

	// for any kind of literal
	(literal_prefix)
	(literal_suffix)

	// both integers and floating-point
	(literal_number)
	// for simple cases in preprocessor code
	// whole literal in 1 token, no support for escape sequences
	(literal_string)
	(literal_char_begin)
	(literal_string_begin)
	(literal_text_end) // closes both char and string literals
	(literal_string_raw_quote)
	(literal_string_raw_delimeter)
	(literal_string_raw_paren)
	(escape_sequence)
	(format_sequence)

	(whitespace)
	// used when inside some context but no specific highlight is desired
	(nothing_special)

	(end_of_input)
);

inline std::ostream& operator<<(std::ostream& os, syntax_token token)
{
	return os << utility::to_string(token);
}

struct identifier_token
{
	semantic_token_info info;
	semantic_token_color_variance color_variance; // TODO simplify implementation
};

constexpr bool operator==(identifier_token lhs, identifier_token rhs)
{
	return lhs.info == rhs.info && lhs.color_variance == rhs.color_variance;
}

constexpr bool operator!=(identifier_token lhs, identifier_token rhs)
{
	return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, identifier_token token)
{
	return os << token.info;
}

using code_token_type = std::variant<syntax_token, identifier_token>;

struct code_token
{
	code_token_type token_type;
	text::fragment origin;
};

constexpr bool operator==(code_token lhs, code_token rhs)
{
	return lhs.token_type == rhs.token_type && lhs.origin == rhs.origin;
}

constexpr bool operator!=(code_token lhs, code_token rhs)
{
	return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, code_token token)
{
	os << token.origin;
	return std::visit(utility::visitor{
		[&os](syntax_token token) -> std::ostream& { return os << "syntax token: " << token << "\n"; },
		[&os](identifier_token token) -> std::ostream& { return os << token; }
	}, token.token_type);
}

}
