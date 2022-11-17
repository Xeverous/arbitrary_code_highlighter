#pragma once

#include <ach/clangd/semantic_token.hpp>
#include <ach/text/types.hpp>
#include <ach/utility/range.hpp>

namespace ach::clangd {

enum class error_reason
{
	syntax_error,
	unsupported,
	invalid_semantic_token_data,
	internal_error_missed_semantic_token,
	internal_error_unhandled_preprocessor,
	internal_error_unhandled_context,
	internal_error_unhandled_context_no_preprocessor,
	internal_error_unhandled_comment,
	internal_error_raw_string_literal_quote_open,
	internal_error_raw_string_literal_quote_close,
	internal_error_raw_string_literal_paren_open,
	internal_error_raw_string_literal_paren_close,
	internal_error_token_to_action,
	internal_error_unhandled_end_of_input
};

struct highlighter_error
{
	text::position pos;
	utility::range<const semantic_token*> last_semantic_tokens;
	error_reason reason;
};

}
