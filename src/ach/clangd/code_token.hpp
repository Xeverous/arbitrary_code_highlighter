#pragma once

#include <ach/clangd/semantic_token.hpp>
#include <ach/text/types.hpp>
#include <ach/utility/enum.hpp>

#include <ostream>
#include <iomanip>

namespace ach::clangd {

ACH_RICH_ENUM_CLASS(syntax_element_type,
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
	(identifier) // all non-keyword identifiers

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

	(symbol) // non-overloaed operators and also stuff like {} , ;
	(overloaded_operator)

	(whitespace)
	// used when inside some context but no specific highlight is desired
	(nothing_special)

	(end_of_input)
);

inline std::ostream& operator<<(std::ostream& os, syntax_element_type token)
{
	return os << utility::to_string(token);
}

struct code_token
{
	constexpr code_token(text::fragment origin, syntax_element_type syntax_element)
	: origin(origin)
	, syntax_element(syntax_element)
	{}

	// these 2 have no sensible default-constructed state
	text::fragment origin;
	syntax_element_type syntax_element;

	// these 2 have good default initial states
	semantic_token_info semantic_info = {};
	semantic_token_color_variance color_variance = {};
};

constexpr bool operator==(code_token lhs, code_token rhs)
{
	return lhs.origin == rhs.origin
		&& lhs.syntax_element == rhs.syntax_element
		&& lhs.semantic_info == rhs.semantic_info
		&& lhs.color_variance == rhs.color_variance;
}

constexpr bool operator!=(code_token lhs, code_token rhs)
{
	return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, code_token token)
{
	// TODO add color_variance when implemneted
	return os << token.origin << " | " << std::setw(28) << token.syntax_element << " | " << token.semantic_info << "\n";
}

}
