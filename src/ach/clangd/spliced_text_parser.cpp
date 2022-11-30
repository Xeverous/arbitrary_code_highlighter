#include <ach/clangd/spliced_text_parser.hpp>
#include <ach/text/utils.hpp>
#include <ach/utility/functional.hpp>
#include <ach/utility/type_traits.hpp>

#include <utility>
#include <type_traits>

namespace ach::clangd {

using utility::function_to_function_object;

namespace {

/*
 * based on Boost.Spirit design, without attributes
 * (all parsers only return bool and modify the iterator)
 * https://en.wikipedia.org/wiki/Spirit_Parser_Framework
 */
namespace parsers {

using utility::remove_cvref_t;

/*
 * Register every parser type to support SFINAE.
 * Without SFINAE operator overloads are too eager and apply
 * to non-parser types, causing syntax problems, e.g. *first.
 *
 * Trait specialization + trait check was choosen over inheritance + is_base_of
 * check because inheritance complicates {}-style object construction.
 */
template <typename T>
struct is_parser : std::false_type {};
template <typename T>
constexpr bool is_parser_v = is_parser<T>::value;

// fundamental parsers

struct any_char
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		if (first == last)
			return false;

		++first;
		return true;
	}
};
template <>
struct is_parser<any_char> : std::true_type {};

template <typename F>
struct specific_char
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		if (first == last)
			return false;

		if (!f(*first))
			return false;

		++first;
		return true;
	}

	F f;
};
template <typename F>
struct is_parser<specific_char<F>> : std::true_type {};
template <typename F>
specific_char(F&& f) -> specific_char<remove_cvref_t<F>>;

struct literal_char
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		if (first == last)
			return false;

		if (*first != c)
			return false;

		++first;
		return true;
	}

	char c;
};
template <>
struct is_parser<literal_char> : std::true_type {};

struct literal_string
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		ForwardIterator it = first;
		for (char c : str) {
			if (it == last)
				return false;

			if (*it != c)
				return false;

			++it;
		}

		first = it;
		return true;
	}

	std::string_view str;
};
template <>
struct is_parser<literal_string> : std::true_type {};

// parser modifiers

template <typename Parser>
struct zero_or_more_of
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		while (parser(first, last)) {}
		return true;
	}

	Parser parser;
};
template <typename T>
struct is_parser<zero_or_more_of<T>> : std::true_type {};
template <typename Parser, typename = std::enable_if_t<is_parser_v<remove_cvref_t<Parser>>>>
auto operator*(Parser&& parser)
{
	return zero_or_more_of<remove_cvref_t<Parser>>{std::forward<Parser>(parser)};
}

template <typename Parser>
struct one_or_more_of
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		if (!parser(first, last))
			return false;

		while (parser(first, last)) {}
		return true;
	}

	Parser parser;
};
template <typename T>
struct is_parser<one_or_more_of<T>> : std::true_type {};
template <typename Parser, typename = std::enable_if_t<is_parser_v<remove_cvref_t<Parser>>>>
auto operator+(Parser&& parser)
{
	return one_or_more_of<remove_cvref_t<Parser>>{std::forward<Parser>(parser)};
}

template <typename Parser>
struct one_or_none_of
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		parser(first, last);
		return true;
	}

	Parser parser;
};
template <typename T>
struct is_parser<one_or_none_of<T>> : std::true_type {};
template <typename Parser, typename = std::enable_if_t<is_parser_v<remove_cvref_t<Parser>>>>
auto operator-(Parser&& parser)
{
	return one_or_none_of<remove_cvref_t<Parser>>{std::forward<Parser>(parser)};
}

template <typename Parser>
struct none_of
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		ForwardIterator it = first;
		return !parser(it, last);
	}

	Parser parser;
};
template <typename T>
struct is_parser<none_of<T>> : std::true_type {};
template <typename Parser, typename = std::enable_if_t<is_parser_v<remove_cvref_t<Parser>>>>
auto operator!(Parser&& parser)
{
	return none_of<remove_cvref_t<Parser>>{std::forward<Parser>(parser)};
}

template <typename ParserOne, typename ParserAnother>
struct one_or_another
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		if (parser_one(first, last))
			return true;

		return parser_another(first, last);
	}

	ParserOne parser_one;
	ParserAnother parser_another;
};
template <typename T, typename U>
struct is_parser<one_or_another<T, U>> : std::true_type {};
template <
	typename ParserOne,
	typename ParserAnother,
	typename = std::enable_if_t<is_parser_v<remove_cvref_t<ParserOne>>>,
	typename = std::enable_if_t<is_parser_v<remove_cvref_t<ParserAnother>>>
>
auto operator|(ParserOne&& parser_one, ParserAnother&& parser_another)
{
	return one_or_another<remove_cvref_t<ParserOne>, remove_cvref_t<ParserAnother>>{
		std::forward<ParserOne>(parser_one), std::forward<ParserAnother>(parser_another)};
}

template <typename ParserOne, typename ParserAnother>
struct one_except_another
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		ForwardIterator it = first;
		if (parser_another(it, last))
			return false;

		return parser_one(first, last);
	}

	ParserOne parser_one;
	ParserAnother parser_another;
};
template <typename T, typename U>
struct is_parser<one_except_another<T, U>> : std::true_type {};
template <
	typename ParserOne,
	typename ParserAnother,
	typename = std::enable_if_t<is_parser_v<remove_cvref_t<ParserOne>>>,
	typename = std::enable_if_t<is_parser_v<remove_cvref_t<ParserAnother>>>
>
auto operator-(ParserOne&& parser_one, ParserAnother&& parser_another)
{
	return one_except_another<remove_cvref_t<ParserOne>, remove_cvref_t<ParserAnother>>{
		std::forward<ParserOne>(parser_one), std::forward<ParserAnother>(parser_another)};
}

template <typename ParserOne, typename ParserAnother>
struct one_and_another
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		ForwardIterator it = first;
		if (parser_one(first, last) && parser_another(first, last))
			return true;

		first = it;
		return false;
	}

	ParserOne parser_one;
	ParserAnother parser_another;
};
template <typename T, typename U>
struct is_parser<one_and_another<T, U>> : std::true_type {};
template <
	typename ParserOne,
	typename ParserAnother,
	typename = std::enable_if_t<is_parser_v<remove_cvref_t<ParserOne>>>,
	typename = std::enable_if_t<is_parser_v<remove_cvref_t<ParserAnother>>>
>
auto operator>>(ParserOne&& parser_one, ParserAnother&& parser_another)
{
	return one_and_another<remove_cvref_t<ParserOne>, remove_cvref_t<ParserAnother>>{
		std::forward<ParserOne>(parser_one), std::forward<ParserAnother>(parser_another)};
}

template <typename Parser>
struct look_ahead
{
	template <typename ForwardIterator>
	bool operator()(ForwardIterator& first, const ForwardIterator& last)
	{
		ForwardIterator it = first;
		return parser(it, last);
	}

	Parser parser;
};
template <typename T>
struct is_parser<look_ahead<T>> : std::true_type {};
template <typename Parser, typename = std::enable_if_t<is_parser_v<remove_cvref_t<Parser>>>>
auto operator&(Parser&& parser)
{
	return look_ahead<remove_cvref_t<Parser>>{std::forward<Parser>(parser)};
}

template <typename Parser>
auto make_keyword_parser(Parser&& parser)
{
	return std::forward<Parser>(parser)
		>> !specific_char{function_to_function_object(text::is_alnum_or_underscore)};
}

// since C++14 ' can be inserted between digits
template <typename Parser>
auto make_digit_sequence_parser(Parser&& digit_parser)
{
	return *(-literal_char{'\''} >> std::forward<Parser>(digit_parser));
}

// predefined parsers

inline auto comment_tag_todo = make_keyword_parser(literal_string{"TODO"} | literal_string{"FIXME"});
inline auto comment_tag_doxygen = literal_char{'@'} >> +specific_char{function_to_function_object(text::is_alpha)};

inline auto digit_binary  = specific_char{function_to_function_object(text::is_digit_binary)};
inline auto digit_octal   = specific_char{function_to_function_object(text::is_digit_octal)};
inline auto digit_decimal = specific_char{function_to_function_object(text::is_digit)};
inline auto digit_hex     = specific_char{function_to_function_object(text::is_digit_hex)};

inline auto hex_prefix = parsers::literal_string{"0x"} | parsers::literal_string{"0X"};

inline auto exponent_tail = -(literal_char{'+'} | literal_char{'-'}) >> +digit_decimal;
inline auto exponent_decimal = (literal_char{'e'} | literal_char{'E'}) >> exponent_tail;
inline auto exponent_hex     = (literal_char{'p'} | literal_char{'P'}) >> exponent_tail;

inline auto identifier =
	specific_char{function_to_function_object(text::is_alpha_or_underscore)}
	>> *specific_char{function_to_function_object(text::is_alnum_or_underscore)};

inline auto text_literal_prefix = literal_char{'L'} | literal_string{"u8"} | literal_char{'u'} | literal_char{'U'};

inline auto escape_simple = parsers::specific_char{[](char c) {
	return c == '\'' || c == '"' || c == '?' || c == '\\' || c == 'a'
		|| c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v';
}};
inline auto escape_numeric =
	// \nnn
	(parsers::digit_octal >> parsers::digit_octal >> parsers::digit_octal)
	// \nn
	| (parsers::digit_octal >> parsers::digit_octal)
	// \n
	| parsers::digit_octal
	// \o{n...} (arbitrary number of octal)
	| (parsers::literal_char{'o'} >> parsers::literal_char{'{'} >> +parsers::digit_octal >> parsers::literal_char{'}'})
	// \xn... (arbitrary number of hex)
	| (parsers::literal_char{'x'} >> +parsers::digit_hex)
	// \x{n...} (arbitrary number of hex)
	| (parsers::literal_char{'x'} >> parsers::literal_char{'{'} >> +parsers::digit_hex >> parsers::literal_char{'}'})
	// \unnnn (4 hex digits)
	| (parsers::literal_char{'u'} >> parsers::digit_hex >> parsers::digit_hex >> parsers::digit_hex >> parsers::digit_hex)
	// \u{n...} (arbitrary number of hex digits)
	| (parsers::literal_char{'u'} >> parsers::literal_char{'{'} >> +parsers::digit_hex >> parsers::literal_char{'}'})
	// \Unnnn (8 hex digits)
	| (parsers::literal_char{'U'}
		>> parsers::digit_hex >> parsers::digit_hex >> parsers::digit_hex >> parsers::digit_hex
		>> parsers::digit_hex >> parsers::digit_hex >> parsers::digit_hex >> parsers::digit_hex
	)
	// \N{name} where name consists of A-Z, 0-9, '-', ' '
	| (parsers::literal_char{'N'} >> parsers::literal_char{'{'}
		>> +parsers::specific_char{[](char c) {
			return ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '-' || c == ' ';
		}}
		>> parsers::literal_char{'}'}
	)
;
inline auto escape_implementation_defined =
	parsers::specific_char{function_to_function_object(text::is_from_basic_character_set)};

} // namespace parsers

} // namespace

text::fragment spliced_text_parser::parse_exactly(char c)
{
	return parse(parsers::literal_char{c});
}

text::fragment spliced_text_parser::parse_exactly(std::string_view str)
{
	return parse(parsers::literal_string{str});
}

text::fragment spliced_text_parser::parse_newlines()
{
	return parse(+(parsers::literal_char{'\n'} | parsers::literal_string{"\r\n"}));
}

text::fragment spliced_text_parser::parse_preprocessor_diagnostic_message()
{
	return parse(+(parsers::any_char{}
		- parsers::literal_char{'\n'}
		- parsers::literal_string{"//"}
		- parsers::literal_string{"/*"}
	));
}

text::fragment spliced_text_parser::parse_non_newline_whitespace()
{
	return parse(+parsers::specific_char{function_to_function_object(text::is_non_newline_whitespace)});
}

text::fragment spliced_text_parser::parse_digits()
{
	return parse(+parsers::specific_char{function_to_function_object(text::is_digit)});
}

text::fragment spliced_text_parser::parse_identifier()
{
	return parse(parsers::identifier);
}

text::fragment spliced_text_parser::parse_numeric_literal()
{
	// order is important here (parse more complex ones first)
	// TODO implement C++14 ' support for floating-point literals
	return parse(
		// floating point - hex
		(parsers::hex_prefix >> *parsers::digit_hex >> parsers::literal_char{'.'} >> +parsers::digit_hex >> parsers::exponent_hex)
		| (parsers::hex_prefix >> +parsers::digit_hex >> -parsers::literal_char{'.'} >> parsers::exponent_hex)
		// floating point - integer
		| (*parsers::digit_decimal >> parsers::literal_char{'.'} >> +parsers::digit_decimal >> -parsers::exponent_decimal)
		| (+parsers::digit_decimal >> parsers::literal_char{'.'} >> -parsers::exponent_decimal)
		| (+parsers::digit_decimal >> parsers::exponent_decimal)
		// integers - hex
		| ((parsers::hex_prefix)
			>> parsers::digit_hex >> parsers::make_digit_sequence_parser(parsers::digit_hex))
		// integers - binary
		| ((parsers::literal_string{"0b"} | parsers::literal_string{"0B"})
			>> parsers::digit_binary >> parsers::make_digit_sequence_parser(parsers::digit_binary))
		// integers - octal
		| (parsers::literal_char{'0'} >> parsers::make_digit_sequence_parser(parsers::digit_octal))
		// integers - decimal
		| ((parsers::digit_decimal - parsers::literal_char{'0'}) >> parsers::make_digit_sequence_parser(parsers::digit_decimal))
	);
}

text::fragment spliced_text_parser::parse_text_literal_prefix(char quote)
{
	return parse(parsers::text_literal_prefix >> &parsers::literal_char{quote});
}

text::fragment spliced_text_parser::parse_raw_string_literal_prefix()
{
	return parse(-parsers::text_literal_prefix >> parsers::literal_char{'R'} >> &parsers::literal_char{'"'});
}

text::fragment spliced_text_parser::parse_raw_string_literal_delimeter_open()
{
	return parse(+parsers::specific_char{[](char c) {
		return text::is_from_basic_character_set(c) && c != '(' && c != ')' && c != '\\' && !text::is_whitespace(c);
	}});
}

text::fragment spliced_text_parser::parse_raw_string_literal_body(std::string_view delimeter)
{
	// any_char should actually be "a character from the translation character set" but because
	// it allows any UCS scalar value and there are no ambiguities, any_char is used instead
	return parse(+(parsers::any_char{} - (parsers::literal_char{')'} >> parsers::literal_string{delimeter})));
}

text::fragment spliced_text_parser::parse_symbols()
{
	return parse(+(parsers::specific_char{[](char c) {
		return c == '!' || c == '%' || c == '&'
		|| (0x28 <= c && c <= 0x2f) // '(', ')', '*', '+', ',', '-', '.', '/'
		|| (0x3a <= c && c <= 0x3f) // ':', ';', '<', '=', '>', '?'
		|| c == '[' || c == ']' || c == '^' || c == '{' || c == '|' || c == '}' || c == '~';
	}} - parsers::literal_string{"//"} - parsers::literal_string{"/*"}));
}

text::fragment spliced_text_parser::parse_comment_tag_todo()
{
	return parse(parsers::comment_tag_todo);
}

text::fragment spliced_text_parser::parse_comment_tag_doxygen()
{
	return parse(parsers::comment_tag_doxygen);
}

text::fragment spliced_text_parser::parse_comment_single_body()
{
	return parse(
		+(parsers::any_char{} - parsers::literal_char{'\n'} - parsers::comment_tag_todo)
	);
}

text::fragment spliced_text_parser::parse_comment_single_doxygen_body()
{
	return parse(
		+(parsers::any_char{}
			- parsers::literal_char{'\n'}
			- parsers::comment_tag_doxygen
			- parsers::comment_tag_todo
		)
	);
}

text::fragment spliced_text_parser::parse_comment_multi_body()
{
	return parse(
		+(parsers::any_char{} - parsers::literal_string{"*/"} - parsers::comment_tag_todo)
	);
}

text::fragment spliced_text_parser::parse_comment_multi_doxygen_body()
{
	return parse(
		+(parsers::any_char{}
			- parsers::literal_string{"*/"}
			- parsers::comment_tag_doxygen
			- parsers::comment_tag_todo
		)
	);
}

text::fragment spliced_text_parser::parse_quoted(char begin_delimeter, char end_delimeter)
{
	return parse(
		parsers::literal_char{begin_delimeter}
		>> *parsers::specific_char{[=](char c) { return c != end_delimeter && c != '\n'; }}
		>> parsers::literal_char{end_delimeter}
	);
}

text::fragment spliced_text_parser::parse_escape_sequence()
{
	return parse(+(parsers::literal_char{'\\'}
		>> (parsers::escape_simple | parsers::escape_numeric | parsers::escape_implementation_defined)
	));
}

text::fragment spliced_text_parser::parse_text_literal_body(char delimeter)
{
	return parse(+(parsers::any_char{} - parsers::literal_char{delimeter}));
}

text::fragment spliced_text_parser::return_parse_result(bool is_success, spliced_text_iterator updated_iterator)
{
	if (is_success) {
		assert(updated_iterator != m_iterator &&
			"every successful parse should move the iterator"
			"- otherwise the tokenizer will get stuck on an infinite loop");
		text::fragment result{str_from_range(m_iterator, updated_iterator), {m_iterator.position(), updated_iterator.position()}};
		m_iterator = updated_iterator;
		return result;
	}
	else {
		assert(updated_iterator == m_iterator &&
			"every failed parse should result in no iterator change"
			"- otherwise the parser has been written incorrectly");
		return empty_match();
	}
}

template <typename Parser>
text::fragment spliced_text_parser::parse(Parser&& parser)
{
	spliced_text_iterator updated_iterator = m_iterator;
	bool is_success = std::forward<Parser>(parser)(updated_iterator, spliced_text_iterator());
	return return_parse_result(is_success, updated_iterator);
}

} // namespace ach::clangd
