#include <ach/clangd/code_tokenizer.hpp>
#include <ach/clangd/splice_utils.hpp>

#include <string_view>

namespace ach::clangd {

namespace {

bool is_keyword(std::string_view identifier, utility::range<const std::string*> keywords)
{
	for (const auto& kw : keywords)
		if (compare_spliced_with_raw(identifier, kw))
			return true;

	return false;
}

preprocessor_state_t preprocessor_directive_to_state(std::string_view directive)
{
	if (compare_spliced_with_raw(directive, "include"))
		return preprocessor_state_t::preprocessor_after_include;
	else if (compare_spliced_with_raw(directive, "define"))
		return preprocessor_state_t::preprocessor_after_define;
	else if (compare_spliced_with_raw(directive, "ifdef"))
		return preprocessor_state_t::preprocessor_after_ifdef_ifndef_elifdef_elifndef_undef;
	else if (compare_spliced_with_raw(directive, "ifndef"))
		return preprocessor_state_t::preprocessor_after_ifdef_ifndef_elifdef_elifndef_undef;
	else if (compare_spliced_with_raw(directive, "elifdef"))
		return preprocessor_state_t::preprocessor_after_ifdef_ifndef_elifdef_elifndef_undef;
	else if (compare_spliced_with_raw(directive, "elifndef"))
		return preprocessor_state_t::preprocessor_after_ifdef_ifndef_elifdef_elifndef_undef;
	else if (compare_spliced_with_raw(directive, "undef"))
		return preprocessor_state_t::preprocessor_after_ifdef_ifndef_elifdef_elifndef_undef;
	else if (compare_spliced_with_raw(directive, "line"))
		return preprocessor_state_t::preprocessor_after_line;
	else if (compare_spliced_with_raw(directive, "error"))
		return preprocessor_state_t::preprocessor_after_error_warning;
	else if (compare_spliced_with_raw(directive, "warning"))
		return preprocessor_state_t::preprocessor_after_error_warning;
	else
		return preprocessor_state_t::preprocessor_after_other;
}

}

void code_tokenizer::on_parsed_newline()
{
	m_preprocessor_state = preprocessor_state_t::line_begin;
	m_preprocessor_macro_params.clear();
}

[[nodiscard]] std::optional<highlighter_error>
code_tokenizer::fill_with_tokens(bool highlight_printf_formatting, std::vector<code_token>& tokens)
{
	tokens.clear();

	while (true) {
		std::variant<code_token, highlighter_error> token_or_error = next_code_token(highlight_printf_formatting);

		if (std::holds_alternative<highlighter_error>(token_or_error))
			return std::get<highlighter_error>(token_or_error);

		tokens.push_back(std::get<code_token>(token_or_error));

		if (tokens.back().syntax_element == syntax_element_type::end_of_input)
			return std::nullopt;
	}
}

std::variant<code_token, highlighter_error>
code_tokenizer::next_code_token(bool highlight_printf_formatting)
{
	switch (m_context_state) {
		case context_state_t::literal_end_optional_suffix:
			m_context_state = context_state_t::none;
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty())
				return code_token(identifier, syntax_element_type::literal_suffix);
			[[fallthrough]];
		case context_state_t::none:
			return next_code_token_context_none();
		case context_state_t::comment_single:
			return next_code_token_context_comment(false, false);
		case context_state_t::comment_single_doxygen:
			return next_code_token_context_comment(false, true);
		case context_state_t::comment_multi:
			return next_code_token_context_comment(true, false);
		case context_state_t::comment_multi_doxygen:
			return next_code_token_context_comment(true, true);
		case context_state_t::comment_end:
			m_context_state = context_state_t::none;
			return code_token(empty_match(), syntax_element_type::comment_end);
		case context_state_t::literal_character:
			return next_code_token_context_quoted_literal('\'', false, highlight_printf_formatting);
		case context_state_t::literal_string:
			return next_code_token_context_quoted_literal('"', true, highlight_printf_formatting);
		case context_state_t::literal_string_raw_quote_open:
			if (text::fragment quote = m_parser.parse_exactly('"'); !quote.empty()) {
				m_context_state = context_state_t::literal_string_raw_delimeter_open;
				return code_token(quote, syntax_element_type::literal_string_raw_quote);
			}
			return make_error(error_reason::internal_error_raw_string_literal_quote_open);
			/////////////// splice off
		case context_state_t::literal_string_raw_delimeter_open:
			m_context_state = context_state_t::literal_string_raw_paren_open;
			if (text::fragment delimeter = m_parser.parse_raw_string_literal_delimeter_open(); !delimeter.empty()) {
				m_raw_string_literal_delimeter = delimeter;
				return code_token(delimeter, syntax_element_type::literal_string_raw_delimeter);
			}
			[[fallthrough]];
		case context_state_t::literal_string_raw_paren_open:
			if (text::fragment paren = m_parser.parse_exactly('('); !paren.empty()) {
				m_context_state = context_state_t::literal_string_raw_body;
				return code_token(paren, syntax_element_type::literal_string_raw_paren);
			}
			return make_error(error_reason::internal_error_raw_string_literal_paren_open);
		case context_state_t::literal_string_raw_body:
			// it is safe to pass m_raw_string_literal_delimeter.str here without checking because:
			// - if the delimeter is empty, the parse will still work
			// - raw string delimeters can not be spliced so std::string_view can be used directly
			m_context_state = context_state_t::literal_string_raw_paren_close;
			if (text::fragment body = m_parser.parse_raw_string_literal_body(m_raw_string_literal_delimeter.str); !body.empty()) {
				return code_token(body, syntax_element_type::literal_string);
			}
			[[fallthrough]];
		case context_state_t::literal_string_raw_paren_close:
			if (text::fragment paren = m_parser.parse_exactly(')'); !paren.empty()) {
				m_context_state = context_state_t::literal_string_raw_delimeter_close;
				return code_token(paren, syntax_element_type::literal_string_raw_paren);
			}
			return make_error(error_reason::internal_error_raw_string_literal_paren_close);
		case context_state_t::literal_string_raw_delimeter_close: {
			m_context_state = context_state_t::literal_string_raw_quote_close;
			const std::string_view delimeter = m_raw_string_literal_delimeter.str;
			m_raw_string_literal_delimeter = {};
			if (text::fragment delim = m_parser.parse_raw_string_literal_delimeter_close(delimeter); !delim.empty()) {
				return code_token(delim, syntax_element_type::literal_string_raw_delimeter);
			}
			[[fallthrough]];
		}
		case context_state_t::literal_string_raw_quote_close:
			if (text::fragment quote = m_parser.parse_exactly('"'); !quote.empty()) {
				m_context_state = context_state_t::none;
				return code_token(quote, syntax_element_type::literal_string_raw_quote);
			}
			return make_error(error_reason::internal_error_raw_string_literal_quote_close);
		/////////////// TODO splice on
	}

	return make_error(error_reason::internal_error_unhandled_context);
}

std::variant<code_token, highlighter_error> code_tokenizer::next_code_token_context_none()
{
	if (m_parser.has_reached_end())
		return code_token(empty_match(), syntax_element_type::end_of_input);

	// comments are first because they are higher in parsing priority than almost anything else
	// the else are: trigraphs (unsupported) and splice (implemented at the level of the iterator)

	if (text::fragment comment_start = m_parser.parse_exactly("///"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_single_doxygen;
		return code_token(comment_start, syntax_element_type::comment_begin_single_doxygen);
	}

	if (text::fragment comment_start = m_parser.parse_exactly("//"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_single;
		return code_token(comment_start, syntax_element_type::comment_begin_single);
	}

	// "/**/" has to be detected explicitly because it contains "/**" which starts a doc comment
	if (text::fragment comment_start = m_parser.parse_exactly("/**/"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_end;
		return code_token(comment_start, syntax_element_type::comment_begin_multi);
	}

	if (text::fragment comment_start = m_parser.parse_exactly("/**"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_multi_doxygen;
		return code_token(comment_start, syntax_element_type::comment_begin_multi_doxygen);
	}

	if (text::fragment comment_start = m_parser.parse_exactly("/*"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_multi;
		return code_token(comment_start, syntax_element_type::comment_begin_multi);
	}

	if (text::fragment whitespace = m_parser.parse_non_newline_whitespace(); !whitespace.empty()) {
		return code_token(whitespace, syntax_element_type::whitespace);
	}

	if (text::fragment newlines = m_parser.parse_newlines(); !newlines.empty()) {
		on_parsed_newline();
		return code_token(newlines, syntax_element_type::whitespace);
	}

	/* preprocessor parsing design notes

	// varying number of tokens, may even do #if __has_include(<file>)
	// best approach: accept anything as pp-other but color string and file literals
	#if // #if, #ifdef, #ifndef, #else, #elif, #elifdef, #elifndef, #endif

	// pp-hash pp-directive pp-macro-name pp-macro-body
	#define IDENTIFIER identifier
	// pp-hash pp-directive pp-macro-name
	// strategy: pp-macro-param + keywords + literals + #/##
	#define IDENTIFIER(param) text with param or with #param or with##param
	// pp-hash pp-directive pp-macro-name
	#undef IDENTIFIER

	#include "file"      // pp-hash pp-directive pp-file
	#include <file>      // pp-hash pp-directive pp-file
	#include MACRO       // pp-hash pp-directive pp-other
	#line 123            // pp-hash pp-directive int-literal
	#line 123 "filename" // pp-hash pp-directive int-literal file-literal

	// best approach: anything further is only pp-other (not a string literal)
	#error   text...
	#warning text...
	#pragma  text...
	#unknown_directive text...
	*/
	switch (m_preprocessor_state) {
		case preprocessor_state_t::line_begin: {
			if (text::fragment literal = m_parser.parse_exactly('#'); !literal.empty()) {
				m_preprocessor_state = preprocessor_state_t::preprocessor_after_hash;
				return code_token(literal, syntax_element_type::preprocessor_hash);
			}

			m_preprocessor_state = preprocessor_state_t::no_preprocessor;
			[[fallthrough]];
		}
		case preprocessor_state_t::no_preprocessor: {
			return next_code_token_basic(false);
		}
		case preprocessor_state_t::preprocessor_after_hash: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				m_preprocessor_state = preprocessor_directive_to_state(identifier.str);
				return code_token(identifier, syntax_element_type::preprocessor_directive);
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_define: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				m_preprocessor_state = preprocessor_state_t::preprocessor_after_define_identifier;
				return code_token(identifier, syntax_element_type::preprocessor_macro);
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_define_identifier: {
			if (text::fragment paren = m_parser.parse_exactly('('); !paren.empty()) {
				m_preprocessor_state = preprocessor_state_t::preprocessor_after_define_identifier_paren_open;
				return code_token(paren, syntax_element_type::nothing_special);
			}

			m_preprocessor_state = preprocessor_state_t::preprocessor_macro_body;
			[[fallthrough]];
		}
		case preprocessor_state_t::preprocessor_macro_body: {
			return next_code_token_basic(true);
		}
		case preprocessor_state_t::preprocessor_after_define_identifier_paren_open: {
			// For the purpose of simple implementation, allow identifier, "," and "..."
			// in any order, including incorrect combinations like "x,,y" and "..., x".
			// Such code isn't valid but detecting invalid code isn't the goal of this program.
			if (text::fragment paren = m_parser.parse_exactly(')'); !paren.empty()) {
				m_preprocessor_state = preprocessor_state_t::preprocessor_macro_body;
				return code_token(paren, syntax_element_type::nothing_special);
			}

			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				m_preprocessor_macro_params.push_back(identifier.str);
				return code_token(identifier, syntax_element_type::preprocessor_macro_param);
			}

			if (text::fragment comma = m_parser.parse_exactly(','); !comma.empty()) {
				return code_token(comma, syntax_element_type::nothing_special);
			}

			if (text::fragment ellipsis = m_parser.parse_exactly("..."); !ellipsis.empty()) {
				return code_token(ellipsis, syntax_element_type::nothing_special);
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_ifdef_ifndef_elifdef_elifndef_undef: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				return code_token(identifier, syntax_element_type::preprocessor_macro);
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_include: {
			if (text::fragment quoted = m_parser.parse_quoted('<', '>'); !quoted.empty()) {
				return code_token(quoted, syntax_element_type::preprocessor_header_file);
			}

			if (text::fragment quoted = m_parser.parse_quoted('"'); !quoted.empty()) {
				return code_token(quoted, syntax_element_type::preprocessor_header_file);
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_error_warning: {
			// because these can have arbitrary syntax (excluding comments) there is special parsing call and
			// the result is always emitted as preprocessor_other (it may contain broken parens and quotes)
			if (text::fragment text = m_parser.parse_preprocessor_diagnostic_message(); !text.empty()) {
				return code_token(text, syntax_element_type::preprocessor_other);
			}

			return make_error(error_reason::internal_error_unhandled_preprocessor_diagnostic_message);
		}
		case preprocessor_state_t::preprocessor_after_line:
		case preprocessor_state_t::preprocessor_after_other: {
			if (text::fragment quoted = m_parser.parse_quoted('"'); !quoted.empty()) {
				return code_token(quoted, syntax_element_type::literal_string);
			}

			// preprocessor directives (not macros) support only integer literals
			if (text::fragment digits = m_parser.parse_digits(); !digits.empty()) {
				return code_token(digits, syntax_element_type::literal_number);
			}

			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				return code_token(identifier, syntax_element_type::preprocessor_other);
			}

			if (text::fragment symbols = m_parser.parse_symbols(); !symbols.empty()) {
				return code_token(symbols, syntax_element_type::preprocessor_other);
			}

			return make_error(error_reason::syntax_error);
		}
	}

	return make_error(error_reason::internal_error_unhandled_preprocessor);
}

std::variant<code_token, highlighter_error> code_tokenizer::next_code_token_context_comment(bool is_multiline, bool is_doxygen)
{
	if (is_doxygen) {
		if (text::fragment comment_tag = m_parser.parse_comment_tag_doxygen(); !comment_tag.empty())
			return code_token(comment_tag, syntax_element_type::comment_tag_doxygen);
	}

	if (text::fragment comment_tag = m_parser.parse_comment_tag_todo(); !comment_tag.empty())
		return code_token(comment_tag, syntax_element_type::comment_tag_todo);

	if (is_multiline) {
		if (is_doxygen) {
			if (text::fragment comment_body = m_parser.parse_comment_multi_doxygen_body(); !comment_body.empty())
				return code_token(comment_body, syntax_element_type::nothing_special);
		}
		else {
			if (text::fragment comment_body = m_parser.parse_comment_multi_body(); !comment_body.empty())
				return code_token(comment_body, syntax_element_type::nothing_special);
		}

		if (text::fragment comment_end = m_parser.parse_exactly("*/"); !comment_end.empty()) {
			m_context_state = context_state_t::none;
			return code_token(comment_end, syntax_element_type::comment_end);
		}
	}
	else {
		if (is_doxygen) {
			if (text::fragment comment_body = m_parser.parse_comment_single_doxygen_body(); !comment_body.empty())
				return code_token(comment_body, syntax_element_type::nothing_special);
		}
		else {
			if (text::fragment comment_body = m_parser.parse_comment_single_body(); !comment_body.empty())
				return code_token(comment_body, syntax_element_type::nothing_special);
		}

		if (text::fragment newlines = m_parser.parse_newlines(); !newlines.empty()) {
			m_context_state = context_state_t::none;
			on_parsed_newline();
			return code_token(newlines, syntax_element_type::comment_end);
		}

		// end of file should also close comments
		// this is needed to emit comment_end before context-none code emits end_of_input
		if (has_reached_end()) {
			m_context_state = context_state_t::none;
			return code_token(empty_match(), syntax_element_type::comment_end);
		}
	}

	return make_error(error_reason::internal_error_unhandled_comment);
}

std::variant<code_token, highlighter_error>
code_tokenizer::next_code_token_context_quoted_literal(char delimeter, bool allow_suffix, bool highlight_printf_formatting)
{
	if (text::fragment escape = m_parser.parse_escape_sequence(); !escape.empty())
		return code_token(escape, syntax_element_type::escape_sequence);

	if (highlight_printf_formatting) {
		if (text::fragment formatting = m_parser.parse_format_sequence_printf(); !formatting.empty())
			return code_token(formatting, syntax_element_type::format_sequence);
	}

	if (text::fragment frag = m_parser.parse_text_literal_body(delimeter, highlight_printf_formatting); !frag.empty())
		return code_token(frag, syntax_element_type::nothing_special);

	if (text::fragment delim = m_parser.parse_exactly(delimeter); !delim.empty()) {
		if (allow_suffix)
			m_context_state = context_state_t::literal_end_optional_suffix;
		else
			m_context_state = context_state_t::none;

		return code_token(delim, syntax_element_type::literal_text_end);
	}

	return make_error(error_reason::syntax_error);
}

std::variant<code_token, highlighter_error>
code_tokenizer::next_code_token_basic(bool inside_macro_body)
{
	if (text::fragment prefix = m_parser.parse_raw_string_literal_prefix(); !prefix.empty()) {
		m_context_state = context_state_t::literal_string_raw_quote_open;
		return code_token(prefix, syntax_element_type::literal_prefix);
	}

	if (text::fragment prefix = m_parser.parse_text_literal_prefix('\''); !prefix.empty()) {
		return code_token(prefix, syntax_element_type::literal_prefix);
	}

	if (text::fragment prefix = m_parser.parse_text_literal_prefix('"'); !prefix.empty()) {
		return code_token(prefix, syntax_element_type::literal_prefix);
	}

	if (text::fragment quote = m_parser.parse_exactly('\''); !quote.empty()) {
		m_context_state = context_state_t::literal_character;
		return code_token(quote, syntax_element_type::literal_char_begin);
	}

	if (text::fragment quote = m_parser.parse_exactly('"'); !quote.empty()) {
		m_context_state = context_state_t::literal_string;
		return code_token(quote, syntax_element_type::literal_string_begin);
	}

	if (text::fragment literal = m_parser.parse_numeric_literal(); !literal.empty()) {
		m_context_state = context_state_t::literal_end_optional_suffix;
		return code_token(literal, syntax_element_type::literal_number);
	}

	if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
		if (is_keyword(identifier.str, m_keywords))
			return code_token(identifier, syntax_element_type::keyword);

		if (inside_macro_body) {
			if (is_in_macro_params(identifier.str))
				return code_token(identifier, syntax_element_type::preprocessor_macro_param);
			else
				return code_token(identifier, syntax_element_type::preprocessor_macro_body);
		}

		// not a keyword and not a macro (perhaps attribute or label); report as generic identifier
		return code_token(identifier, syntax_element_type::identifier);
	}

	if (text::fragment hash = m_parser.parse_exactly('#'); !hash.empty()) {
		if (inside_macro_body)
			return code_token(hash, syntax_element_type::preprocessor_hash);
		else
			return make_error(error_reason::syntax_error);
	}

	// Parse only 1 symbol because when multiple symbols are next to each other,
	// each can be a token of a different type: bracket, (non-)overloaded operator
	if (text::fragment symbol = m_parser.parse_symbol(); !symbol.empty()) {
		if (inside_macro_body)
			return code_token(symbol, syntax_element_type::preprocessor_macro_body);
		else
			return code_token(symbol, syntax_element_type::symbol);
	}

	return make_error(error_reason::syntax_error);
}

}
