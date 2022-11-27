#pragma once

#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/state.hpp>
#include <ach/text/types.hpp>
#include <ach/utility/range.hpp>
#include <ach/utility/enum.hpp>

#include <string_view>

namespace ach::clangd {

ACH_RICH_ENUM_CLASS(error_reason,
	(syntax_error)
	(unsupported)
	(invalid_semantic_token_data)
	(internal_error_missed_semantic_token)
	(internal_error_unhandled_preprocessor)
	(internal_error_unhandled_context)
	(internal_error_unhandled_context_no_preprocessor)
	(internal_error_unhandled_comment)
	(internal_error_raw_string_literal_quote_open)
	(internal_error_raw_string_literal_quote_close)
	(internal_error_raw_string_literal_paren_open)
	(internal_error_raw_string_literal_paren_close)
	(internal_error_token_to_action)
	(internal_error_unhandled_end_of_input)
);

struct highlighter_error
{
	error_reason reason;
	text::position pos;
	utility::range<const semantic_token*> last_semantic_tokens;
	context_state_t context_state;
	preprocessor_state_t preprocessor_state;
};

}
