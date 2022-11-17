#include <ach/clangd/code_tokenizer.hpp>
#include <ach/clangd/splice_utils.hpp>

#include <iterator>

namespace ach::clangd {

using detail::context_state_t;
using detail::preprocessor_state_t;

namespace {

bool is_keyword(text::fragment identifier, utility::range<const std::string*> keywords)
{
	for (const auto& kw : keywords)
		if (compare_potentially_spliced(identifier, kw))
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

preprocessor_state_t preprocessor_directive_to_state(text::fragment directive)
{
	if (compare_potentially_spliced(directive, "define"))
		return preprocessor_state_t::preprocessor_after_define;
	else if (compare_potentially_spliced(directive, "undef"))
		return preprocessor_state_t::preprocessor_after_undef;
	else if (compare_potentially_spliced(directive, "include"))
		return preprocessor_state_t::preprocessor_after_include;
	else if (compare_potentially_spliced(directive, "line"))
		return preprocessor_state_t::preprocessor_after_line;
	else
		return preprocessor_state_t::preprocessor_after_other;
}

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
			// no more splice: stop the loop
			// no test of info here as 2 adjacent tokens may have the same info by accident
			break;
		}
	}

	m_current_semantic_tokens.last = it;
	return true;
}

std::variant<code_token, highlighter_error> code_tokenizer::next_code_token(utility::range<const std::string*> keywords)
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
		case context_state_t::comment_single_doc:
			return next_code_token_context_comment(false, true);
		case context_state_t::comment_multi:
			return next_code_token_context_comment(true, false);
		case context_state_t::comment_multi_doc:
			return next_code_token_context_comment(true, true);
		case context_state_t::comment_end:
			m_context_state = context_state_t::none;
			return code_token{syntax_token::comment_end, empty_match()};
		case context_state_t::literal_character:
			return next_code_token_context_quoted_literal('\'', false);
		case context_state_t::literal_string:
			return next_code_token_context_quoted_literal('"', true);
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
		m_context_state = context_state_t::comment_single_doc;
		return code_token{syntax_token::comment_begin_single_doc, comment_start};
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
		m_context_state = context_state_t::comment_multi_doc;
		return code_token{syntax_token::comment_begin_multi_doc, comment_start};
	}

	if (text::fragment comment_start = m_parser.parse_exactly("/*"); !comment_start.empty()) {
		m_context_state = context_state_t::comment_multi;
		return code_token{syntax_token::comment_begin_multi, comment_start};
	}

	if (text::fragment whitespace = m_parser.parse_non_newline_whitespace(); !whitespace.empty()) {
		return code_token{syntax_token::whitespace, whitespace};
	}

	if (text::fragment newline = m_parser.parse_newline(); !newline.empty()) {
		m_preprocessor_state = preprocessor_state_t::line_begin;
		return code_token{syntax_token::whitespace, newline};
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
			return next_code_token_no_preprocessor(keywords);
		}
		case preprocessor_state_t::preprocessor_after_hash: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				m_preprocessor_state = preprocessor_directive_to_state(identifier);
				return code_token{syntax_token::preprocessor_directive, identifier};
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_define: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				return code_token{syntax_token::preprocessor_macro, identifier};
			}

			return make_error(error_reason::unsupported);
		}
		case preprocessor_state_t::preprocessor_after_undef: {
			if (text::fragment identifier = m_parser.parse_identifier(); !identifier.empty()) {
				return code_token{syntax_token::preprocessor_macro, identifier};
			}

			return make_error(error_reason::syntax_error);
		}
		case preprocessor_state_t::preprocessor_after_include: {
			if (text::fragment quoted = m_parser.parse_quoted('<', '>'); !quoted.empty()) {
				return code_token{syntax_token::preprocessor_file, quoted};
			}

			if (text::fragment quoted = m_parser.parse_quoted('"'); !quoted.empty()) {
				return code_token{syntax_token::preprocessor_file, quoted};
			}

			return make_error(error_reason::syntax_error);
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

			return make_error(error_reason::unsupported);
		}
	}

	return make_error(error_reason::internal_error_unhandled_preprocessor);
}

std::variant<code_token, highlighter_error> code_tokenizer::next_code_token_context_comment(bool is_multiline, bool is_documentation)
{
	if (is_documentation) {
		if (text::fragment comment_tag = m_parser.parse_comment_tag_doc(); !comment_tag.empty())
			return code_token{syntax_token::comment_tag_doc, comment_tag};
	}

	if (text::fragment comment_tag = m_parser.parse_comment_tag_todo(); !comment_tag.empty())
		return code_token{syntax_token::comment_tag_todo, comment_tag};

	if (is_multiline) {
		if (is_documentation) {
			if (text::fragment comment_body = m_parser.parse_comment_multi_doc_body(); !comment_body.empty())
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
		if (is_documentation) {
			if (text::fragment comment_body = m_parser.parse_comment_single_doc_body(); !comment_body.empty())
				return code_token{syntax_token::nothing_special, comment_body};
		}
		else {
			if (text::fragment comment_body = m_parser.parse_comment_single_body(); !comment_body.empty())
				return code_token{syntax_token::nothing_special, comment_body};
		}

		if (text::fragment newline = m_parser.parse_newline(); !newline.empty()) {
			m_context_state = context_state_t::none;
			return code_token{syntax_token::comment_end, newline};
		}
	}

	return make_error(error_reason::internal_error_unhandled_comment);
}

std::variant<code_token, highlighter_error> code_tokenizer::next_code_token_context_quoted_literal(char delimeter, bool allow_suffix)
{
	if (text::fragment escape = m_parser.parse_escape_sequence(); !escape.empty())
		return code_token{syntax_token::escape_sequence, escape};

	// TODO add formatting strings here (e.g. %s, %d)

	if (text::fragment frag = m_parser.parse_text_literal_body(delimeter); !frag.empty())
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
code_tokenizer::next_code_token_no_preprocessor(utility::range<const std::string*> keywords)
{
	if (text::fragment prefix = m_parser.parse_raw_string_literal_prefix(); !prefix.empty()) {
		m_context_state = context_state_t::literal_string_raw_quote_open;
		return code_token{syntax_token::literal_prefix, prefix};
	}

	if (text::fragment prefix = m_parser.parse_char_literal_prefix(); !prefix.empty()) {
		// TODO need a context change here? seems not necessary, quotes will just be picked up
		return code_token{syntax_token::literal_prefix, prefix};
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
			if (compare_potentially_spliced(identifier, "auto")) {
				if (!advance_semantic_tokens())
					return make_error(error_reason::invalid_semantic_token_data);
				return code_token{syntax_token::keyword, identifier};
			}

			// Now, whether the identifier is a keyword is irrelevant. If it is,
			// it's probably a pseudo-keyword like "override" (which technically
			// is a special identifier with context-dependent meaning).
			// In such case assume the identifier does not function as a keyword
			// and report it according to the semantic token information.
			assert(!m_current_semantic_tokens.empty());
			auto token = identifier_token{
				m_current_semantic_tokens.first->info,
				m_current_semantic_tokens.first->color_variance
			};
			if (!advance_semantic_tokens())
				return make_error(error_reason::invalid_semantic_token_data);
			return code_token{token, identifier};
		}
		else {
			if (is_keyword(identifier, keywords))
				return code_token{syntax_token::keyword, identifier};

			// no semantic info and not a keyword: some named entity of unknown purpose
			// clangd doesn't report some entities - e.g. goto labels
			return code_token{syntax_token::identifier_unknown, identifier};
		}
	}

	if (text::fragment hash = m_parser.parse_exactly('#'); !hash.empty())
		return make_error(error_reason::syntax_error);

	if (text::fragment backslash = m_parser.parse_exactly('\\'); !backslash.empty())
		return make_error(error_reason::syntax_error);

	if (text::fragment symbols = m_parser.parse_symbols(); !symbols.empty())
		return code_token{syntax_token::nothing_special, symbols};

	return make_error(error_reason::internal_error_unhandled_context_no_preprocessor);
}

}
