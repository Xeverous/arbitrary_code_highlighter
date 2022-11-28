#pragma once

#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/state.hpp>
#include <ach/text/types.hpp>
#include <ach/utility/range.hpp>
#include <ach/utility/enum.hpp>

#include <string_view>
#include <ostream>

namespace ach::clangd {

ACH_RICH_ENUM_CLASS(error_reason,
	(syntax_error)
	(unsupported)
	(invalid_semantic_token_data)
	(internal_error_missed_semantic_token)
	(internal_error_unhandled_preprocessor)
	(internal_error_unhandled_preprocessor_diagnostic_message)
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

inline std::ostream& operator<<(std::ostream& os, highlighter_error error)
{
	os << "[" << error.pos << "] error: " << utility::to_string(error.reason) << "\n"
		"context state: " << utility::to_string(error.context_state) << "\n"
		"preprocessor state: " << utility::to_string(error.preprocessor_state) << "\n";

	os << "last semantic tokens:";
	if (error.last_semantic_tokens.empty())
	{
		os << " (none)\n";
	}
	else
	{
		os << "\n";
		for (semantic_token token : error.last_semantic_tokens)
			os << "\t[" << token.pos << ", length" << token.length << "]\n";
	}

	return os;
}

}
