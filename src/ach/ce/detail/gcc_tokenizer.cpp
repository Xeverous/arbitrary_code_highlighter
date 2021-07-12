#include <ach/ce/detail/gcc_tokenizer.hpp>
#include <ach/utility/text.hpp>

#include <algorithm>
#include <cassert>

namespace ach::ce::detail {

namespace {

bool is_diff_line(std::string_view line)
{
	return utility::starts_with(line, "  +++ |");
}

// input:  14 |   return width * height;
// output: return width * height;
std::string_view skip_line_numbering(std::string_view line, std::size_t n)
{
	// not == because there should be more characters than just numbering
	assert(n < line.size());
	line.remove_prefix(n);
	return line;
}

bool is_underline_line(std::string_view line, std::optional<std::size_t> numbering_length)
{
	if (numbering_length)
		line = skip_line_numbering(line, *numbering_length);

	return std::all_of(line.begin(), line.end(),
		[](char c) { return c == ' ' || c == '~' || c == '^'; });
}

bool is_separator_line(std::string_view line, std::optional<std::size_t> numbering_length)
{
	if (numbering_length)
		line = skip_line_numbering(line, *numbering_length);

	return std::all_of(line.begin(), line.end(),
		[](char c) { return c == ' ' || c == '|'; });
}

// returns number of characters occupied by line numbering
std::optional<std::size_t> has_line_numbering(std::string_view line)
{
	std::size_t i = 0;

	while (i < line.size()) {
		if (line[i] == ' ')
			++i;
		else
			break;
	}

	// lines with line numbering are neither admonitions nor
	// "In file included from" so they must begin with space
	// if the line did not begin with a space it wasn't a line with numbering
	if (i == 0)
		return std::nullopt;

	const std::size_t old_i = i;

	while (i < line.size()) {
		if (utility::is_digit(line[i]))
			++i;
		else
			break;
	}

	// i not changed => no digits found => not a line numbering
	if (i == old_i)
		return std::nullopt;

	// at the end of "    14 |" the should be " |"
	// if not present it's not a line numbering

	if (i == line.size())
		return std::nullopt;

	if (line[++i] != ' ')
		return std::nullopt;

	if (i == line.size())
		return std::nullopt;

	if (line[++i] != '|')
		return std::nullopt;

	return i;
}

constexpr const char str_in_file_included_from[] = "In file included from ";
constexpr const char str_from[]                  = "                 from ";
constexpr std::size_t str_include_length = std::size(str_from);

constexpr const char str_note[]    = "note:";
constexpr const char str_warning[] = "warning:";
constexpr const char str_error[]   = "error:";
constexpr std::size_t str_note_length    = std::size(str_note);
constexpr std::size_t str_warning_length = std::size(str_warning);
constexpr std::size_t str_error_length   = std::size(str_error);

[[nodiscard]] std::optional<line_classification>
classify_line(
	std::string_view line,
	std::optional<line_classification> previous_line_classification,
	std::optional<std::size_t> line_numbering_length)
{
	if (utility::starts_with(line, str_in_file_included_from) || utility::starts_with(line, str_from))
		return line_classification::include;

	if (!utility::starts_with(line, " "))
		return line_classification::admonition;

	if (previous_line_classification == line_classification::admonition) {
		return line_classification::code;
	}
	else if (previous_line_classification == line_classification::code) {
		if (is_underline_line(line, line_numbering_length))
			return line_classification::underline;
		else if (is_diff_line(line))
			return line_classification::diff;
		else
			return std::nullopt;
	}
	else if (previous_line_classification == line_classification::underline) {
		if (is_separator_line(line, line_numbering_length))
			return line_classification::separator;
		else
			return line_classification::proposition;
	}
	else if (previous_line_classification == line_classification::separator) {
		return line_classification::hint;
	}
	else if (previous_line_classification == line_classification::hint) {
		return line_classification::proposition;
	}
	else if (previous_line_classification == line_classification::diff) {
		return line_classification::code;
	}
	else {
		return std::nullopt;
	}
}

template <typename Predicate>
std::size_t num_chars_until_last_match(std::string_view text, Predicate pred)
{
	auto const first = text.rbegin();
	auto const last = text.rend();
	return std::find_if(first, last, pred).base() - last.base();
}

void parse_admonition_text(text_extractor& extractor, admonition_type at, std::queue<gcc_token>& tokens)
{
	enum class state_t { normal, inside_quote, inside_bracket };
	state_t state = state_t::normal;
	std::size_t i = 0;

	while (!extractor.has_reached_line_end()) {
		std::string_view str = extractor.current_location().str();

		if (state == state_t::inside_quote) {
			for (; i < str.size(); ++i) {
				if (str[i] == '\'' || str[i] == '’') {
					state = state_t::normal;
					break;
				}
			}

			tokens.push(gcc_token{extractor.extract_n_characters(i), token_color::highlight});
			i = 1; // include closing ' in next normal - they are not highlighted
		}
		else if (state == state_t::inside_bracket) {
			for (; i < str.size(); ++i) {
				if (str[i] == ']') {
					state = state_t::normal;
					break;
				}
			}

			assert(at != admonition_type::none);
			if (at == admonition_type::note)
				tokens.push(gcc_token{extractor.extract_n_characters(i), token_color::note});
			else if (at == admonition_type::error)
				tokens.push(gcc_token{extractor.extract_n_characters(i), token_color::warning});
			else if (at == admonition_type::error)
				tokens.push(gcc_token{extractor.extract_n_characters(i), token_color::error});
			i = 1; // include closing ] in next normal - they are not highlighted
		}
		else {
			for (; i < str.size(); ++i) {
				if (str[i] == '\'' || str[i] == '‘' || str[i] == '`') {
					state = state_t::inside_quote;
					++i; // include opening ' in normal - they are not highlighted
					break;
				}

				if (at != admonition_type::none && str[i] == '[') {
					state = state_t::inside_bracket;
					++i; // include opening [ in normal - they are not highlighted
					break;
				}
			}

			tokens.push(gcc_token{extractor.extract_n_characters(i), token_color::normal});
			i = 0;
		}
	}
}

underline_info parse_underline(text_location text)
{
	auto const str = text.str();
	std::size_t pivot = -1;
	for (std::size_t i = 0; i < str.size(); ++i) {
		if (str[i] == '~' || str[i] == '^') {
			if (str[i] == '^')
				pivot = i;
		}
	}
}

} // namespace

void gcc_tokenizer::parse_more_tokens()
{
	if (_extractor.has_reached_end())
		return;

	std::optional<line_classification> maybe_classification = classify_line(
		_extractor.current_line(), _previous_line_classification, _line_numbering_length);

	if (!maybe_classification) {
		_error = token_error{_extractor.current_location(), "can not classify line"};
		return;
	}

	// finally _previous_line_classification = maybe_classification; _extractor.load_next_line();

	switch (*maybe_classification) {
		case line_classification::include: {
			_tokens.push(gcc_token{_extractor.extract_n_characters(str_include_length), token_color::normal});

			auto const loc = _extractor.current_location();
			auto const len = num_chars_until_last_match(loc.str(), [](char c) { return c == ',' || c == ':'; });

			if (len == 0) {
				_error = token_error{loc, "expected non-zero length path before ',' or ':'"};
				return;
			}

			_tokens.push(gcc_token{_extractor.extract_n_characters(len), token_color::highlight});
			_tokens.push(gcc_token{_extractor.extract_rest_of_line(), token_color::normal});
			return;
		}
		case line_classification::admonition: {
			auto const loc = _extractor.current_location();
			auto const pos = loc.str().find_first_of(": ");

			if (pos == std::string_view::npos) {
				_error = token_error{loc, "expected non-zero length path before \": \""};
				return;
			}

			++pos; // we also want to include ":" part of ": ", this is safe when search succeeded
			_tokens.push(gcc_token{_extractor.extract_n_characters(pos), token_color::highlight});

			auto const remaining_loc = _extractor.current_location();
			auto const remaining_str = remaining_loc.str();

			if (utility::starts_with(remaining_str, str_note)) {
				_tokens.push(gcc_token{_extractor.extract_n_characters(str_note_length), token_color::note});
				_last_admonition_type = admonition_type::note;
			}
			else if (utility::starts_with(remaining_str, str_warning)) {
				_tokens.push(gcc_token{_extractor.extract_n_characters(str_warning_length), token_color::warning});
				_last_admonition_type = admonition_type::warning;
			}
			else if (utility::starts_with(remaining_str, str_error)) {
				_tokens.push(gcc_token{_extractor.extract_n_characters(str_error_length), token_color::error});
				_last_admonition_type = admonition_type::error;
			}
			else {
				_last_admonition_type = admonition_type::none;
			}

			parse_admonition_text(_extractor, _last_admonition_type, _tokens);
			return;
		}
		case line_classification::code: {
			_line_numbering_length = has_line_numbering(_extractor.current_location().str());
			if (_line_numbering_length)
				_tokens.push(gcc_token{_extractor.extract_n_characters(*_line_numbering_length), token_color::normal});
			_pending_code_line = _extractor.extract_rest_of_line();
			return;
		}
		case line_classification::underline: {
			if (!_pending_code_line) {
				_error = token_error{_extractor.current_location(), "underline - expected pending code line"};
				return;
			}

			if (_line_numbering_length)
				_tokens.push(gcc_token{_extractor.extract_n_characters(*_line_numbering_length), token_color::normal});

			_pending_underline_line = _extractor.extract_rest_of_line();
			//auto const loc = _extractor.extract_until_end_of_line();
			//underline_info info = parse_underline(loc);
			return;
		}
		case line_classification::separator: {
			if (!_pending_code_line || !_pending_underline_line) {
				_error = token_error{_extractor.current_location(), "separator - expected pending code and underline lines"};
				return;
			}
		}
	}

	_error = token_error{_extractor.current_location(), "unhandled line classification"};
}

}


