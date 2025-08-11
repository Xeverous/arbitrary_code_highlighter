#pragma once

#include <ach/clangd/code_token.hpp>
#include <ach/clangd/state.hpp>
#include <ach/text/types.hpp>
#include <ach/utility/enum.hpp>
#include <ach/utility/visitor.hpp>

#include <optional>
#include <ostream>
#include <variant>

namespace ach::clangd {

ACH_RICH_ENUM_CLASS(error_reason,
	(syntax_error)
	(unsupported)
	(internal_error_unhandled_preprocessor)
	(internal_error_unhandled_preprocessor_diagnostic_message)
	(internal_error_unhandled_context)
	(internal_error_unhandled_comment)
	(internal_error_raw_string_literal_quote_open)
	(internal_error_raw_string_literal_quote_close)
	(internal_error_raw_string_literal_paren_open)
	(internal_error_raw_string_literal_paren_close)
	(internal_error_unsupported_spliced_token)
	(internal_error_improve_code_tokens)
	(internal_error_find_matching_tokens)
	(internal_error_token_to_action)
);

struct parser_error_details
{
	context_state_t context_state;
	preprocessor_state_t preprocessor_state;
};

struct semantic_error_details
{
	std::optional<syntax_element_type> syntax_element;
	semantic_token_info semantic_info;
};

struct highlighter_error
{
	static highlighter_error from_parser(
		error_reason reason, text::position pos, context_state_t context_state, preprocessor_state_t preprocessor_state)
	{
		return highlighter_error{reason, pos, parser_error_details{context_state, preprocessor_state}};
	}

	static highlighter_error from_semantic(
		error_reason reason, text::position pos, std::optional<syntax_element_type> syntax_element, semantic_token_info semantic_info)
	{
		return highlighter_error{reason, pos, semantic_error_details{syntax_element, semantic_info}};
	}

	error_reason reason;
	text::position pos;

	std::variant<parser_error_details, semantic_error_details> details;
};

inline std::ostream& operator<<(std::ostream& os, highlighter_error error)
{
	os << "[" << error.pos << "] ";

	std::visit(utility::visitor{
		[&](parser_error_details details) {
			os << "parser error: " << utility::to_string(error.reason) << "\n"
				"context state: " << utility::to_string(details.context_state) << "\n"
				"preprocessor state: " << utility::to_string(details.preprocessor_state) << "\n";
		},
		[&](semantic_error_details details) {
			os << "semantic error: " << utility::to_string(error.reason) << "\n"
				"syntax element: " << (details.syntax_element ? utility::to_string(*details.syntax_element) : "(none)") << "\n"
				"semantic info: " << details.semantic_info << "\n";
		}
	}, error.details);

	return os;
}

}
