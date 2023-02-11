#include <ach/clangd/code_tokenizer.hpp>
#include <ach/clangd/splice_utils.hpp>

#include <iterator>
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

bool matches_semantic_tokens(text::fragment identifier, utility::range<const semantic_token*> sem_tokens)
{
	if (sem_tokens.empty())
		return false;

	return identifier.r.first == sem_tokens.first->pos_begin()
		&& identifier.r.last == std::prev(sem_tokens.last)->pos_end();
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

std::string_view code_tokenizer::semantic_token_str(semantic_token sem_token) const
{
	auto it = m_parser.current_iterator().to_text_iterator();
	assert(it.position() <= sem_token.pos_begin());
	while (it.position() < sem_token.pos_begin())
		++it;

	return {it.pointer(), sem_token.length};
}

bool code_tokenizer::advance_semantic_tokens()
{
	m_current_semantic_tokens.first = m_current_semantic_tokens.last;

	auto it = m_current_semantic_tokens.first;
	while (it != m_all_semantic_tokens.last) {
		if (ends_with_backslash_whitespace(semantic_token_str(*it))) {
			// the token is spliced
			if (it->info == m_current_semantic_tokens.first->info)
				++it; // spliced parts should have the same info...
			else
				return false; // ...if not, the semantic token data is invalid
		}
		else {
			// no more splice: accept the token and stop the loop
			// no test of info here as 2 adjacent tokens may have the same info by accident
			++it;
			break;
		}
	}

	m_current_semantic_tokens.last = it;
	return true;
}

void code_tokenizer::on_parsed_newline()
{
	m_preprocessor_state = preprocessor_state_t::line_begin;
	m_preprocessor_macro_params.clear();
}

std::variant<code_token, highlighter_error>
code_tokenizer::next_code_token(utility::range<const std::string*> keywords, bool highlight_printf_formatting)
{
	if (m_disabled_code_end_pos == current_position()) {
		m_disabled_code_end_pos = std::nullopt;
		return code_token{syntax_token::disabled_code_end, empty_match()};
	}

	if (!m_current_semantic_tokens.empty()) {
		const semantic_token& stok = *m_current_semantic_tokens.first;

		if (current_position() > stok.pos)
			return make_error(error_reason::internal_error_missed_semantic_token);

		if (stok.info.type == semantic_token_type::disabled_code && stok.pos == current_position()) {
			const semantic_token& last_stok = *(m_current_semantic_tokens.last - 1);
			m_disabled_code_end_pos = text::position{last_stok.pos.line, last_stok.pos.column + last_stok.length};
			if (!advance_semantic_tokens())
				return make_error(error_reason::invalid_semantic_token_data);
			return code_token{syntax_token::disabled_code_begin, empty_match()};
		}
	}

	switch (m_context_state) {
		case context_state_t::literal_end_optional_suffix:
			m_context_state = context_state_t::none;
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty())
				return code_token{syntax_token::literal_suffix, identifier};
			[[fallthrough]];
		case context_state_t::none:
			return next_code_token_context_none(keywords);
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
			return code_token{syntax_token::comment_end, empty_match()};
		case context_state_t::literal_character:
			return next_code_token_context_quoted_literal('\'', false, highlight_printf_formatting);
		case context_state_t::literal_string:
			return next_code_token_context_quoted_literal('"', true, highlight_printf_formatting);
		case context_state_t::literal_string_raw_quote_open:
			if (text::fragment quote = m_parser.parse_exactly('"'); !quote.empty()) {
				m_context_state = context_state_t::literal_string_raw_delimeter_open;
				return code_token{syntax_token::literal_string_raw_quote, quote};
			}
			return make_error(error_reason::internal_error_raw_string_literal_quote_open);
			/////////////// splice off
		case context_state_t::literal_string_raw_delimeter_open:
			m_context_state = context_state_t::literal_string_raw_paren_open;
			if (text::fragment delimeter = m_parser.parse_raw_string_literal_delimeter_open(); !delimeter.empty()) {
				m_raw_string_literal_delimeter = delimeter;
				return code_token{syntax_token::literal_string_raw_delimeter, delimeter};
			}
			[[fallthrough]];
		case context_state_t::literal_string_raw_paren_open:
			if (text::fragment paren = m_parser.parse_exactly('('); !paren.empty()) {
				m_context_state = context_state_t::literal_string_raw_body;
				return code_token{syntax_token::literal_string_raw_paren, paren};
			}
			return make_error(error_reason::internal_error_raw_string_literal_paren_open);
		case context_state_t::literal_string_raw_body:
			// it is safe to pass m_raw_string_literal_delimeter.str here without checking because:
			// - if the delimeter is empty, the parse will still work
			// - raw string delimeters can not be spliced so std::string_view can be used directly
			m_context_state = context_state_t::literal_string_raw_paren_close;
			if (text::fragment body = m_parser.parse_raw_string_literal_body(m_raw_string_literal_delimeter.str); !body.empty()) {
				return code_token{syntax_token::literal_string, body};
			}
			[[fallthrough]];
		case context_state_t::literal_string_raw_paren_close:
			if (text::fragment paren = m_parser.parse_exactly(')'); !paren.empty()) {
				m_context_state = context_state_t::literal_string_raw_delimeter_close;
				return code_token{syntax_token::literal_string_raw_paren, paren};
			}
			return make_error(error_reason::internal_error_raw_string_literal_paren_close);
		case context_state_t::literal_string_raw_delimeter_close: {
			m_context_state = context_state_t::literal_string_raw_quote_close;
			const std::string_view delimeter = m_raw_string_literal_delimeter.str;
			m_raw_string_literal_delimeter = {};
			if (text::fragment delim = m_parser.parse_raw_string_literal_delimeter_close(delimeter); !delim.empty()) {
				return code_token{syntax_token::literal_string_raw_delimeter, delim};
			}
			[[fallthrough]];
		}
		case context_state_t::literal_string_raw_quote_close:
			if (text::fragment quote = m_parser.parse_exactly('"'); !quote.empty()) {
				m_context_state = context_state_t::none;
				return code_token{syntax_token::literal_string_raw_quote, quote};
			}
			return make_error(error_reason::internal_error_raw_string_literal_quote_close);
		/////////////// TODO splice on
	}

	return make_error(error_reason::internal_error_unhandled_context);
}

std::variant<code_token, highlighter_error> code_tokenizer::next_code_token_context_none(utility::range<const std::string*> keywords)
{
	if (m_parser.has_reached_end())
		return code_token{syntax_token::end_of_input, empty_match()};

	// comments are first because they are higher in parsing priority than almost anything else
	// the else are: trigraphs (unsupported) and splice (implemented at the level of the iterator)

	if (text::fragment comment_start = m_parser.parse_exactly("///"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_single_doxygen;
		return code_token{syntax_token::comment_begin_single_doxygen, comment_start};
	}

	if (text::fragment comment_start = m_parser.parse_exactly("//"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_single;
		return code_token{syntax_token::comment_begin_single, comment_start};
	}

	// "/**/" has to be detected explicitly because it contains "/**" which starts a doc comment
	if (text::fragment comment_start = m_parser.parse_exactly("/**/"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_end;
		return code_token{syntax_token::comment_begin_multi, comment_start};
	}

	if (text::fragment comment_start = m_parser.parse_exactly("/**"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_multi_doxygen;
		return code_token{syntax_token::comment_begin_multi_doxygen, comment_start};
	}

	if (text::fragment comment_start = m_parser.parse_exactly("/*"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_multi;
		return code_token{syntax_token::comment_begin_multi, comment_start};
	}

	if (text::fragment whitespace = m_parser.parse_non_newline_whitespace(); !whitespace.empty()) {
		return code_token{syntax_token::whitespace, whitespace};
	}

	if (text::fragment newlines = m_parser.parse_newlines(); !newlines.empty()) {
		on_parsed_newline();
		return code_token{syntax_token::whitespace, newlines};
	}

	switch (m_preprocessor_state) {
		case preprocessor_state_t::line_begin: {
			if (text::fragment literal = m_parser.parse_exactly('#'); !literal.empty()) {
				m_preprocessor_state = preprocessor_state_t::preprocessor_after_hash;
				return code_token{syntax_token::preprocessor_hash, literal};
			}

			m_preprocessor_state = preprocessor_state_t::no_preprocessor;
			[[fallthrough]];
		}
		case preprocessor_state_t::no_preprocessor: {
			return next_code_token_basic(keywords, false);
		}
		case preprocessor_state_t::preprocessor_after_hash: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				m_preprocessor_state = preprocessor_directive_to_state(identifier.str);
				return code_token{syntax_token::preprocessor_directive, identifier};
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_define: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				m_preprocessor_state = preprocessor_state_t::preprocessor_after_define_identifier;
				if (matches_semantic_tokens(identifier, m_current_semantic_tokens))
					return make_code_token_from_semantic_tokens(identifier);
				else
					return code_token{syntax_token::preprocessor_macro, identifier};
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_define_identifier: {
			if (text::fragment paren = m_parser.parse_exactly('('); !paren.empty()) {
				m_preprocessor_state = preprocessor_state_t::preprocessor_after_define_identifier_paren_open;
				return code_token{syntax_token::nothing_special, paren};
			}

			m_preprocessor_state = preprocessor_state_t::preprocessor_macro_body;
			[[fallthrough]];
		}
		case preprocessor_state_t::preprocessor_macro_body: {
			return next_code_token_basic(keywords, true);
		}
		case preprocessor_state_t::preprocessor_after_define_identifier_paren_open: {
			// For the purpose of simple implementation, allow identifier, "," and "..."
			// in any order, including incorrect combinations like "x,,y" and "..., x".
			// Such code isn't valid but detecting invalid code isn't the goal of this program.
			if (text::fragment paren = m_parser.parse_exactly(')'); !paren.empty()) {
				m_preprocessor_state = preprocessor_state_t::preprocessor_macro_body;
				return code_token{syntax_token::nothing_special, paren};
			}

			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				m_preprocessor_macro_params.push_back(identifier.str);
				return code_token{syntax_token::preprocessor_macro_param, identifier};
			}

			if (text::fragment comma = m_parser.parse_exactly(','); !comma.empty()) {
				return code_token{syntax_token::nothing_special, comma};
			}

			if (text::fragment ellipsis = m_parser.parse_exactly("..."); !ellipsis.empty()) {
				return code_token{syntax_token::nothing_special, ellipsis};
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_ifdef_ifndef_elifdef_elifndef_undef: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				return code_token{syntax_token::preprocessor_macro, identifier};
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_include: {
			if (text::fragment quoted = m_parser.parse_quoted('<', '>'); !quoted.empty()) {
				return code_token{syntax_token::preprocessor_header_file, quoted};
			}

			if (text::fragment quoted = m_parser.parse_quoted('"'); !quoted.empty()) {
				return code_token{syntax_token::preprocessor_header_file, quoted};
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_error_warning: {
			// because these can have arbitrary syntax (excluding comments) there is special parsing call and
			// the result is always emitted as preprocessor_other (it may contain broken parens and quotes)
			if (text::fragment text = m_parser.parse_preprocessor_diagnostic_message(); !text.empty()) {
				return code_token{syntax_token::preprocessor_other, text};
			}

			return make_error(error_reason::internal_error_unhandled_preprocessor_diagnostic_message);
		}
		case preprocessor_state_t::preprocessor_after_line:
		case preprocessor_state_t::preprocessor_after_other: {
			if (text::fragment quoted = m_parser.parse_quoted('"'); !quoted.empty()) {
				return code_token{syntax_token::literal_string, quoted};
			}

			// preprocessor directives (not macros) support only integer literals
			if (text::fragment digits = m_parser.parse_digits(); !digits.empty()) {
				return code_token{syntax_token::literal_number, digits};
			}

			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				return code_token{syntax_token::preprocessor_other, identifier};
			}

			if (text::fragment symbols = m_parser.parse_symbols(); !symbols.empty()) {
				return code_token{syntax_token::preprocessor_other, symbols};
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
			return code_token{syntax_token::comment_tag_doxygen, comment_tag};
	}

	if (text::fragment comment_tag = m_parser.parse_comment_tag_todo(); !comment_tag.empty())
		return code_token{syntax_token::comment_tag_todo, comment_tag};

	if (is_multiline) {
		if (is_doxygen) {
			if (text::fragment comment_body = m_parser.parse_comment_multi_doxygen_body(); !comment_body.empty())
				return code_token{syntax_token::nothing_special, comment_body};
		}
		else {
			if (text::fragment comment_body = m_parser.parse_comment_multi_body(); !comment_body.empty())
				return code_token{syntax_token::nothing_special, comment_body};
		}

		if (text::fragment comment_end = m_parser.parse_exactly("*/"); !comment_end.empty()) {
			m_context_state = context_state_t::none;
			return code_token{syntax_token::comment_end, comment_end};
		}
	}
	else {
		if (is_doxygen) {
			if (text::fragment comment_body = m_parser.parse_comment_single_doxygen_body(); !comment_body.empty())
				return code_token{syntax_token::nothing_special, comment_body};
		}
		else {
			if (text::fragment comment_body = m_parser.parse_comment_single_body(); !comment_body.empty())
				return code_token{syntax_token::nothing_special, comment_body};
		}

		if (text::fragment newlines = m_parser.parse_newlines(); !newlines.empty()) {
			m_context_state = context_state_t::none;
			on_parsed_newline();
			return code_token{syntax_token::comment_end, newlines};
		}

		// end of file should also close comments
		// this is needed to emit comment_end before context-none code emits end_of_input
		if (has_reached_end()) {
			m_context_state = context_state_t::none;
			return code_token{syntax_token::comment_end, empty_match()};
		}
	}

	return make_error(error_reason::internal_error_unhandled_comment);
}

std::variant<code_token, highlighter_error>
code_tokenizer::next_code_token_context_quoted_literal(char delimeter, bool allow_suffix, bool highlight_printf_formatting)
{
	if (text::fragment escape = m_parser.parse_escape_sequence(); !escape.empty())
		return code_token{syntax_token::escape_sequence, escape};

	if (highlight_printf_formatting) {
		if (text::fragment formatting = m_parser.parse_format_sequence_printf(); !formatting.empty())
			return code_token{syntax_token::format_sequence, formatting};
	}

	if (text::fragment frag = m_parser.parse_text_literal_body(delimeter, highlight_printf_formatting); !frag.empty())
		return code_token{syntax_token::nothing_special, frag};

	if (text::fragment delim = m_parser.parse_exactly(delimeter); !delim.empty()) {
		if (allow_suffix)
			m_context_state = context_state_t::literal_end_optional_suffix;
		else
			m_context_state = context_state_t::none;

		return code_token{syntax_token::literal_text_end, delim};
	}

	return make_error(error_reason::syntax_error);
}

std::variant<code_token, highlighter_error>
code_tokenizer::next_code_token_basic(utility::range<const std::string*> keywords, bool inside_macro_body)
{
	if (text::fragment prefix = m_parser.parse_raw_string_literal_prefix(); !prefix.empty()) {
		m_context_state = context_state_t::literal_string_raw_quote_open;
		return code_token{syntax_token::literal_prefix, prefix};
	}

	if (text::fragment prefix = m_parser.parse_text_literal_prefix('\''); !prefix.empty()) {
		return code_token{syntax_token::literal_prefix, prefix};
	}

	if (text::fragment prefix = m_parser.parse_text_literal_prefix('"'); !prefix.empty()) {
		return code_token{syntax_token::literal_prefix, prefix};
	}

	if (text::fragment quote = m_parser.parse_exactly('\''); !quote.empty()) {
		m_context_state = context_state_t::literal_character;
		return code_token{syntax_token::literal_char_begin, quote};
	}

	if (text::fragment quote = m_parser.parse_exactly('"'); !quote.empty()) {
		m_context_state = context_state_t::literal_string;
		return code_token{syntax_token::literal_string_begin, quote};
	}

	if (text::fragment literal = m_parser.parse_numeric_literal(); !literal.empty()) {
		m_context_state = context_state_t::literal_end_optional_suffix;
		return code_token{syntax_token::literal_number, literal};
	}

	if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
		if (matches_semantic_tokens(identifier, m_current_semantic_tokens)) {
			// A unique case: clangd doesn't report keywords except auto.
			// If auto can be deduced, it is reported as a semantic token.
			// This is simply unneeded: advance semantic tokens and report a keyword.
			if (compare_spliced_with_raw(identifier.str, "auto")) {
				if (!advance_semantic_tokens())
					return make_error(error_reason::invalid_semantic_token_data);
				return code_token{syntax_token::keyword, identifier};
			}

			// Now, whether the identifier is a keyword is irrelevant. If it is,
			// it's probably a pseudo-keyword like "override" (which technically
			// is a special identifier with context-dependent meaning).
			// In such case assume the identifier does not function as a keyword
			// and report it according to the semantic token information.
			return make_code_token_from_semantic_tokens(identifier);
		}
		else {
			if (is_keyword(identifier.str, keywords))
				return code_token{syntax_token::keyword, identifier};

			if (inside_macro_body) {
				if (is_in_macro_params(identifier.str))
					return code_token{syntax_token::preprocessor_macro_param, identifier};
				else
					return code_token{syntax_token::preprocessor_macro_body, identifier};
			}

			// no semantic info, not a keyword and not a macro: unknown purpose
			// clangd doesn't report some entities - e.g. goto labels
			return code_token{syntax_token::identifier_unknown, identifier};
		}
	}

	if (text::fragment hash = m_parser.parse_exactly('#'); !hash.empty()) {
		if (inside_macro_body)
			return code_token{syntax_token::preprocessor_hash, hash};
		else
			return make_error(error_reason::syntax_error);
	}

	if (text::fragment symbols = m_parser.parse_symbols(); !symbols.empty()) {
		if (inside_macro_body)
			return code_token{syntax_token::preprocessor_macro_body, symbols};
		else
			return code_token{syntax_token::nothing_special, symbols};
	}

	return make_error(error_reason::syntax_error);
}

std::variant<code_token, highlighter_error>
code_tokenizer::make_code_token_from_semantic_tokens(text::fragment identifier)
{
	assert(!m_current_semantic_tokens.empty());
	auto token = identifier_token{
		m_current_semantic_tokens.first->info,
		m_current_semantic_tokens.first->color_variance
	};

	if (!advance_semantic_tokens())
		return make_error(error_reason::invalid_semantic_token_data);

	return code_token{token, identifier};
}

}
