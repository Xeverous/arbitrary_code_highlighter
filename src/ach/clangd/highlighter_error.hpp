#pragma once

#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/state.hpp>
#include <ach/text/types.hpp>
#include <ach/utility/range.hpp>

#include <string_view>

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

constexpr std::string_view to_string(error_reason reason)
{
	switch (reason) {
		case error_reason::syntax_error:
			return "syntax_error";
		case error_reason::unsupported:
			return "unsupported";
		case error_reason::invalid_semantic_token_data:
			return "invalid_semantic_token_data";
		case error_reason::internal_error_missed_semantic_token:
			return "internal_error_missed_semantic_token";
		case error_reason::internal_error_unhandled_preprocessor:
			return "internal_error_unhandled_preprocessor";
		case error_reason::internal_error_unhandled_context:
			return "internal_error_unhandled_context";
		case error_reason::internal_error_unhandled_context_no_preprocessor:
			return "internal_error_unhandled_context_no_preprocessor";
		case error_reason::internal_error_unhandled_comment:
			return "internal_error_unhandled_comment";
		case error_reason::internal_error_raw_string_literal_quote_open:
			return "internal_error_raw_string_literal_quote_open";
		case error_reason::internal_error_raw_string_literal_quote_close:
			return "internal_error_raw_string_literal_quote_close";
		case error_reason::internal_error_raw_string_literal_paren_open:
			return "internal_error_raw_string_literal_paren_open";
		case error_reason::internal_error_raw_string_literal_paren_close:
			return "internal_error_raw_string_literal_paren_close";
		case error_reason::internal_error_token_to_action:
			return "internal_error_token_to_action";
		case error_reason::internal_error_unhandled_end_of_input:
			return "internal_error_unhandled_end_of_input";
	}

	return "(unknown)";
}

struct highlighter_error
{
	error_reason reason;
	text::position pos;
	utility::range<const semantic_token*> last_semantic_tokens;
	context_state_t context_state;
	preprocessor_state_t preprocessor_state;
};

}
