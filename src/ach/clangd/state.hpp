#pragma once

#include <ach/utility/enum.hpp>

namespace ach::clangd {

ACH_RICH_ENUM_CLASS(preprocessor_state_t,
	// before any non-whitespace character has been read
	// if the first non-whitespace character is #, it will be a preprocessor line
	(line_begin)

	// ...if not, this value means that a given line for sure isn't preprocessor
	(no_preprocessor)

	(preprocessor_after_hash)

	(preprocessor_after_define)
	(preprocessor_after_define_identifier)
	(preprocessor_after_define_identifier_paren_open)
	(preprocessor_macro_body) // both object-like and function-like macros
	(preprocessor_after_undef)
	(preprocessor_after_include)
	(preprocessor_after_line)
	(preprocessor_after_other) // unrecognized directive, use generic coloring
);

ACH_RICH_ENUM_CLASS(context_state_t,
	(none)
	(comment_single)
	(comment_single_doxygen)
	(comment_multi)
	(comment_multi_doxygen)
	(comment_end)
	(literal_character)
	(literal_string)
	(literal_end_optional_suffix)
	(literal_string_raw_quote_open)
	(literal_string_raw_delimeter_open)
	(literal_string_raw_paren_open)
	(literal_string_raw_body)
	(literal_string_raw_paren_close)
	(literal_string_raw_delimeter_close)
	(literal_string_raw_quote_close)
);

}
