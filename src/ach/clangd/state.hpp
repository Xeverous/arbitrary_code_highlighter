#pragma once

#include <string_view>

namespace ach::clangd {

enum class preprocessor_state_t
{
	// before any non-whitespace character has been read
	// if the first non-whitespace character is #, it will be a preprocessor line
	line_begin,

	// ...if not, this value means that a given line for sure isn't preprocessor
	no_preprocessor,

	preprocessor_after_hash,

	preprocessor_after_define,
	preprocessor_after_define_identifier,
	preprocessor_after_define_identifier_paren_open,
	preprocessor_macro_body, // both object-like and function-like macros
	preprocessor_after_undef,
	preprocessor_after_include,
	preprocessor_after_line,
	preprocessor_after_other // unrecognized directive, use generic coloring
};

constexpr std::string_view to_string(preprocessor_state_t state) noexcept
{
	switch (state) {
		case preprocessor_state_t::line_begin:
			return "line_begin";
		case preprocessor_state_t::no_preprocessor:
			return "no_preprocessor";
		case preprocessor_state_t::preprocessor_after_hash:
			return "preprocessor_after_hash";
		case preprocessor_state_t::preprocessor_after_define:
			return "preprocessor_after_define";
		case preprocessor_state_t::preprocessor_after_define_identifier:
			return "preprocessor_after_define_identifier";
		case preprocessor_state_t::preprocessor_after_define_identifier_paren_open:
			return "preprocessor_after_define_identifier_paren_open";
		case preprocessor_state_t::preprocessor_macro_body:
			return "preprocessor_macro_body";
		case preprocessor_state_t::preprocessor_after_undef:
			return "preprocessor_after_undef";
		case preprocessor_state_t::preprocessor_after_include:
			return "preprocessor_after_include";
		case preprocessor_state_t::preprocessor_after_line:
			return "preprocessor_after_line";
		case preprocessor_state_t::preprocessor_after_other:
			return "preprocessor_after_other";
	}

	return "?";
}

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

constexpr std::string_view to_string(context_state_t state) noexcept
{
	switch (state) {
		case context_state_t::none:
			return "none";
		case context_state_t::comment_single:
			return "comment_single";
		case context_state_t::comment_single_doxygen:
			return "comment_single_doxygen";
		case context_state_t::comment_multi:
			return "comment_multi";
		case context_state_t::comment_multi_doxygen:
			return "comment_multi_doxygen";
		case context_state_t::comment_end:
			return "comment_end";
		case context_state_t::literal_character:
			return "literal_character";
		case context_state_t::literal_string:
			return "literal_string";
		case context_state_t::literal_end_optional_suffix:
			return "literal_end_optional_suffix";
		case context_state_t::literal_string_raw_quote_open:
			return "literal_string_raw_quote_open";
		case context_state_t::literal_string_raw_delimeter_open:
			return "literal_string_raw_delimeter_open";
		case context_state_t::literal_string_raw_paren_open:
			return "literal_string_raw_paren_open";
		case context_state_t::literal_string_raw_body:
			return "literal_string_raw_body";
		case context_state_t::literal_string_raw_paren_close:
			return "literal_string_raw_paren_close";
		case context_state_t::literal_string_raw_delimeter_close:
			return "literal_string_raw_delimeter_close";
		case context_state_t::literal_string_raw_quote_close:
			return "literal_string_raw_quote_close";
	}

	return "?";
}

}
