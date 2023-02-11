#include <ach/clangd/core.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/code_tokenizer.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/clangd/semantic_token.hpp>
#include <ach/text/types.hpp>

#include <boost/test/tools/assertion_result.hpp>
#include <boost/test/unit_test.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace ach;
using namespace ach::clangd;

const std::initializer_list<std::string> keywords = {
	"__restrict",
	"abstract",
	"alignas",
	"alignof",
	"and",
	"and_eq",
	"asm",
	"atomic_cancel",
	"atomic_commit",
	"atomic_noexcept",
	"auto",
	"bitand",
	"bitor",
	"bool",
	"break",
	"case",
	"catch",
	"char",
	"char8_t",
	"char16_t",
	"char32_t",
	"class",
	"compl",
	"concept",
	"const",
	"consteval",
	"constexpr",
	"constinit",
	"const_cast",
	"continue",
	"co_await",
	"co_return",
	"co_yield",
	"decltype",
	"default",
	"delete",
	"do",
	"double",
	"dynamic_cast",
	"else",
	"enum",
	"explicit",
	"export",
	"extern",
	"false",
	"float",
	"for",
	"friend",
	"goto",
	"if",
	"inline",
	"int",
	"interface",
	"let",
	"long",
	"mutable",
	"namespace",
	"new",
	"noexcept",
	"not",
	"not_eq",
	"nullptr",
	"operator",
	"or",
	"or_eq",
	"private",
	"protected",
	"public",
	"reflexpr",
	"register",
	"reinterpret_cast",
	"requires",
	"restrict",
	"return",
	"short",
	"signed",
	"sizeof",
	"static",
	"static_assert",
	"static_cast",
	"struct",
	"switch",
	"synchronized",
	"template",
	"this",
	"thread_local",
	"throw",
	"true",
	"try",
	"typedef",
	"typeid",
	"typename",
	"typeof",
	"union",
	"unsigned",
	"using",
	"var",
	"virtual",
	"void",
	"volatile",
	"wchar_t",
	"while",
	"with",
	"xor",
	"xor_eq"
};

BOOST_AUTO_TEST_SUITE(code_tokenizer_suite)

	code_tokenizer make_code_tokenizer(std::string_view input, utility::range<const semantic_token*> tokens)
	{
		auto result = code_tokenizer::create(input, tokens);
		BOOST_TEST_REQUIRE(result.has_value());
		return *std::move(result);
	}

	struct test_code_token
	{
		code_token_type token_type;
		std::string_view str; // text positions will be calculated based on string contents
	};

	code_token make_code_token(test_code_token test_token, text::position current_position)
	{
		text::text_iterator it(test_token.str.data(), current_position);
		const auto last = test_token.str.data() + test_token.str.size();
		while (it.pointer() != last)
			++it;

		return code_token{
			test_token.token_type,
			text::fragment{test_token.str, text::range{current_position, it.position()}}
		};
	}

	void print_semantic_tokens(std::ostream& os, utility::range<const semantic_token*> sem_tokens)
	{
		for (auto it = sem_tokens.first; it != sem_tokens.last; ++it)
			os << *it << "\n";
	}

	[[nodiscard]] boost::test_tools::assertion_result test_code_tokenizer_impl(
		std::string_view input,
		const std::vector<semantic_token>& sem_tokens,
		const std::vector<test_code_token>& test_tokens,
		highlighter_options options)
	{
		auto tokenizer = make_code_tokenizer(input, {sem_tokens.data(), sem_tokens.data() + sem_tokens.size()});
		text::position current_position = {};

		for (std::size_t i = 0; i < test_tokens.size(); ++i) {
			if (current_position != tokenizer.current_position()) {
				boost::test_tools::assertion_result result = false;
				result.message().stream() << "positions differ! test position: " << current_position
					<< "tokenizer position: " << tokenizer.current_position();
				return result;
			}

			std::variant<code_token, highlighter_error> token_or_error = tokenizer.next_code_token(
				{keywords.begin(), keywords.begin() + keywords.size()}, options.highlight_printf_formatting);

			if (std::holds_alternative<highlighter_error>(token_or_error)) {
				boost::test_tools::assertion_result result = false;
				result.message().stream() << "unexpected error on iteration " << i
					<< ": " << std::get<highlighter_error>(token_or_error);
				return result;
			}

			auto expected_code_token = make_code_token(test_tokens[i], current_position);
			auto actual_code_token = std::get<code_token>(token_or_error);
			current_position = expected_code_token.origin.r.last;

			if (expected_code_token != actual_code_token) {
				boost::test_tools::assertion_result result = false;
				result.message().stream() << "tokens differ:\n"
					"EXPECTED:\n" << expected_code_token << "ACTUAL:\n" << actual_code_token
					<< "current semantic tokens:\n";

				print_semantic_tokens(result.message().stream(), tokenizer.current_semantic_tokens());
				return result;
			}
		}

		if (!tokenizer.has_reached_end()) {
			boost::test_tools::assertion_result result = false;
			result.message().stream() << "tokenizer did not reached end, pos = [" << tokenizer.current_position() << "]\n"
				"context state: " << utility::to_string(tokenizer.current_context_state()) << "\n"
				"preprocessor state: " << utility::to_string(tokenizer.current_preprocessor_state()) << "\n";
			return result;
		}

		if (!tokenizer.current_semantic_tokens().empty()) {
			boost::test_tools::assertion_result result = false;
			result.message().stream() << "tokenizer reached end but did not exhaust semantic tokens:\n";
			print_semantic_tokens(result.message().stream(), tokenizer.current_semantic_tokens());
			return result;
		}

		return true;
	}

	void test_code_tokenizer(
		std::string_view input,
		const std::vector<semantic_token>& sem_tokens,
		const std::vector<test_code_token>& test_tokens,
		highlighter_options options = {})
	{
		BOOST_TEST(test_code_tokenizer_impl(input, sem_tokens, test_tokens, options));
	}

	BOOST_AUTO_TEST_CASE(empty_input)
	{
		test_code_tokenizer("", {}, {
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	// tests most fundamental preprocessor parsing logic
	BOOST_AUTO_TEST_CASE(pp_include)
	{
		test_code_tokenizer("#include <iostream>", {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("include")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_header_file, std::string_view("<iostream>")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(pp_object_like_macro)
	{
		test_code_tokenizer("#define MACRO f(__FILE__, __LINE__)", {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("define")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro, std::string_view("MACRO")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("f")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("__FILE__")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(",")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("__LINE__")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(")")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(pp_function_like_macro_multiple_params_with_ellipsis)
	{
		std::string_view input =
			"#define MACRO ( x , yy , zzz, ...) \\\n"
			"\tf1(x, yy); \\\n"
			"\tf2(yy, zzz); \\\n"
			"\tf3(x, zzz)";

		test_code_tokenizer(input, {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("define")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro, std::string_view("MACRO")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::nothing_special, std::string_view("(")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::nothing_special, std::string_view(",")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("yy")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::nothing_special, std::string_view(",")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("zzz")},
			test_code_token{syntax_token::nothing_special, std::string_view(",")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::nothing_special, std::string_view("...")},
			test_code_token{syntax_token::nothing_special, std::string_view(")")},
			test_code_token{syntax_token::whitespace, std::string_view(" \\\n\t")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("f1")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(",")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("yy")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(");")},
			test_code_token{syntax_token::whitespace, std::string_view(" \\\n\t")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("f2")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("yy")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(",")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("zzz")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(");")},
			test_code_token{syntax_token::whitespace, std::string_view(" \\\n\t")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("f3")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(",")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("zzz")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(")")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(pp_function_like_macro_with_stringize)
	{
		std::string_view input =
			"#define PRINT(x) \\\n"
			"\tstd::cout << #x \" = \" << (x) << \"\\n\"";

		test_code_tokenizer(input, {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("define")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro, std::string_view("PRINT")},
			test_code_token{syntax_token::nothing_special, std::string_view("(")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_token::nothing_special, std::string_view(")")},
			test_code_token{syntax_token::whitespace, std::string_view(" \\\n\t")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("std")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("::")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("cout")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("<<")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_token::nothing_special, std::string_view(" = ")},
			test_code_token{syntax_token::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("<<")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_token::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view(")")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro_body, std::string_view("<<")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_token::escape_sequence, std::string_view("\\n")},
			test_code_token{syntax_token::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	// test syntax_token::preprocessor_other and
	// the ability to emit syntax_token::comment_end upon encountering end of input
	BOOST_AUTO_TEST_CASE(pp_pragma_and_comment)
	{
		test_code_tokenizer("#pragma once // TODO comment", {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("pragma")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_other, std::string_view("once")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::comment_begin_single, std::string_view("//")},
			test_code_token{syntax_token::nothing_special, std::string_view(" ")},
			test_code_token{syntax_token::comment_tag_todo, std::string_view("TODO")},
			test_code_token{syntax_token::nothing_special, std::string_view(" comment")},
			test_code_token{syntax_token::comment_end, std::string_view()},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	// test a string literal in preprocessor directive
	BOOST_AUTO_TEST_CASE(pp_file_and_string_literal)
	{
		test_code_tokenizer("#file \"main.cpp\" 1", {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("file")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::literal_string, std::string_view("\"main.cpp\"")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::literal_number, std::string_view("1")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(pp_error_and_empty_directives)
	{
		std::string_view input =
			"#\n"
			"#ifdef BUILD_CONFIG\n"
			"#  undef BUILD_BUG\n"
			"#else\n"
			"#\n"
			"\n"
			"#  error 'whatever\" possibly q'uoted\n"
			"#endif\n";

		test_code_tokenizer(input, {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::whitespace, std::string_view("\n")},
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("ifdef")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro, std::string_view("BUILD_CONFIG")},
			test_code_token{syntax_token::whitespace, std::string_view("\n")},
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::whitespace, std::string_view("  ")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("undef")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_macro, std::string_view("BUILD_BUG")},
			test_code_token{syntax_token::whitespace, std::string_view("\n")},
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("else")},
			test_code_token{syntax_token::whitespace, std::string_view("\n")},
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::whitespace, std::string_view("\n\n")},
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::whitespace, std::string_view("  ")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("error")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_other, std::string_view("'whatever\" possibly q'uoted")},
			test_code_token{syntax_token::whitespace, std::string_view("\n")},
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("endif")},
			test_code_token{syntax_token::whitespace, std::string_view("\n")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	// (this is valid code, comments behaviorally act as a single space)
	BOOST_AUTO_TEST_CASE(pp_and_multiline_comment)
	{
		std::string_view input =
			"#include \\\n"
			"/*\n"
			" * text\n"
			" */ <string>";

		test_code_tokenizer(input, {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("include")},
			test_code_token{syntax_token::whitespace, std::string_view(" \\\n")},
			test_code_token{syntax_token::comment_begin_multi, std::string_view("/*")},
			test_code_token{syntax_token::nothing_special, std::string_view("\n * text\n ")},
			test_code_token{syntax_token::comment_end, std::string_view("*/")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_header_file, std::string_view("<string>")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	// test multiple keywords, unknown identifier, symbol, C++14 literal and literal suffix
	BOOST_AUTO_TEST_CASE(var_definition_and_literal_suffix)
	{
		test_code_tokenizer("const long long x = 123'456'789ll;", {}, {
			test_code_token{syntax_token::keyword, std::string_view("const")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::keyword, std::string_view("long")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::keyword, std::string_view("long")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::identifier_unknown, std::string_view("x")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::nothing_special, std::string_view("=")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::literal_number, std::string_view("123'456'789")},
			test_code_token{syntax_token::literal_suffix, std::string_view("ll")},
			test_code_token{syntax_token::nothing_special, std::string_view(";")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(escape_and_format_sequences)
	{
		test_code_tokenizer(R"("abc%s%% \\%+.?";)", {}, {
			test_code_token{syntax_token::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_token::nothing_special, std::string_view("abc%s%% ")},
			test_code_token{syntax_token::escape_sequence, std::string_view("\\\\")},
			test_code_token{syntax_token::nothing_special, std::string_view("%+.?")},
			test_code_token{syntax_token::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_token::nothing_special, std::string_view(";")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});

		highlighter_options options;
		options.highlight_printf_formatting = true;
		test_code_tokenizer(R"("abc%s%% \\%+.?";)", {}, {
			test_code_token{syntax_token::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_token::nothing_special, std::string_view("abc")},
			test_code_token{syntax_token::format_sequence, std::string_view("%s%%")},
			test_code_token{syntax_token::nothing_special, std::string_view(" ")},
			test_code_token{syntax_token::escape_sequence, std::string_view("\\\\")},
			test_code_token{syntax_token::nothing_special, std::string_view("%+.?")},
			test_code_token{syntax_token::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_token::nothing_special, std::string_view(";")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		}, options);
	}

	BOOST_AUTO_TEST_CASE(literal_prefix)
	{
		test_code_tokenizer("f(L'\\0');", {}, {
			test_code_token{syntax_token::identifier_unknown, std::string_view("f")},
			test_code_token{syntax_token::nothing_special, std::string_view("(")},
			test_code_token{syntax_token::literal_prefix, std::string_view("L")},
			test_code_token{syntax_token::literal_char_begin, std::string_view("'")},
			test_code_token{syntax_token::escape_sequence, std::string_view("\\0")},
			test_code_token{syntax_token::literal_text_end, std::string_view("'")},
			test_code_token{syntax_token::nothing_special, std::string_view(");")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(literal_suffix)
	{
		test_code_tokenizer(R"(f(u8"w\0x\"y\xffz"s);)", {}, {
			test_code_token{syntax_token::identifier_unknown, std::string_view("f")},
			test_code_token{syntax_token::nothing_special, std::string_view("(")},
			test_code_token{syntax_token::literal_prefix, std::string_view("u8")},
			test_code_token{syntax_token::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_token::nothing_special, std::string_view("w")},
			test_code_token{syntax_token::escape_sequence, std::string_view("\\0")},
			test_code_token{syntax_token::nothing_special, std::string_view("x")},
			test_code_token{syntax_token::escape_sequence, std::string_view("\\\"")},
			test_code_token{syntax_token::nothing_special, std::string_view("y")},
			test_code_token{syntax_token::escape_sequence, std::string_view("\\xff")},
			test_code_token{syntax_token::nothing_special, std::string_view("z")},
			test_code_token{syntax_token::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_token::literal_suffix, std::string_view("s")},
			test_code_token{syntax_token::nothing_special, std::string_view(");")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(raw_string_literal)
	{
		std::string_view input = R"(f(u8R"test(raw\nstring\nliteral)test");)";

		test_code_tokenizer(input, {}, {
			test_code_token{syntax_token::identifier_unknown, std::string_view("f")},
			test_code_token{syntax_token::nothing_special, std::string_view("(")},
			test_code_token{syntax_token::literal_prefix, std::string_view("u8R")},
			test_code_token{syntax_token::literal_string_raw_quote, std::string_view("\"")},
			test_code_token{syntax_token::literal_string_raw_delimeter, std::string_view("test")},
			test_code_token{syntax_token::literal_string_raw_paren, std::string_view("(")},
			test_code_token{syntax_token::literal_string, std::string_view("raw\\nstring\\nliteral")},
			test_code_token{syntax_token::literal_string_raw_paren, std::string_view(")")},
			test_code_token{syntax_token::literal_string_raw_delimeter, std::string_view("test")},
			test_code_token{syntax_token::literal_string_raw_quote, std::string_view("\"")},
			test_code_token{syntax_token::nothing_special, std::string_view(");")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(semantic_tokens)
	{
		std::string_view input =
			"template <typename T>\n"
			"bar foo<T>::f(int& n) const\n"
			"{\n"
			"\tauto k = 1;\n"
			"\treturn h(T::v, g, k, m, n);\n"
			"}\n";

		semantic_token_info sti_t1{semantic_token_type::template_parameter, semantic_token_modifiers{}.declaration().scope_class()};
		semantic_token_info sti_bar{semantic_token_type::class_, semantic_token_modifiers{}};
		semantic_token_info sti_foo{semantic_token_type::class_, semantic_token_modifiers{}};
		semantic_token_info sti_t2{semantic_token_type::template_parameter, semantic_token_modifiers{}.scope_class()};
		semantic_token_info sti_f{semantic_token_type::method, semantic_token_modifiers{}.virtual_()};
		semantic_token_info sti_n1{semantic_token_type::parameter, semantic_token_modifiers{}.declaration().scope_function()};
		semantic_token_info sti_auto{semantic_token_type::type, semantic_token_modifiers{}.deduced()};
		semantic_token_info sti_k1{semantic_token_type::variable, semantic_token_modifiers{}.declaration().scope_function()};
		semantic_token_info sti_h{semantic_token_type::function, semantic_token_modifiers{}};
		semantic_token_info sti_t3{semantic_token_type::template_parameter, semantic_token_modifiers{}.scope_class()};
		semantic_token_info sti_v{semantic_token_type::unknown, semantic_token_modifiers{}.dependent_name()};
		semantic_token_info sti_g{semantic_token_type::variable, semantic_token_modifiers{}.readonly().scope_global()};
		semantic_token_info sti_k2{semantic_token_type::variable, semantic_token_modifiers{}.scope_function()};
		semantic_token_info sti_m{semantic_token_type::variable, semantic_token_modifiers{}.scope_class()};
		semantic_token_info sti_n2{semantic_token_type::parameter, semantic_token_modifiers{}.scope_function().out_parameter()};

		test_code_tokenizer(input, {
				semantic_token{{0, 19}, 1, sti_t1, {}},
				semantic_token{{1, 0}, 3, sti_bar, {}},
				semantic_token{{1, 4}, 3, sti_foo, {}},
				semantic_token{{1, 8}, 1, sti_t2, {}},
				semantic_token{{1, 12}, 1u, sti_f, {}},
				semantic_token{{1, 19}, 1u, sti_n1, {}},
				semantic_token{{3, 1}, 4u, sti_auto, {}},
				semantic_token{{3, 6}, 1u, sti_k1, {}},
				semantic_token{{4, 8}, 1u, sti_h, {}},
				semantic_token{{4, 10}, 1u, sti_t3, {}},
				semantic_token{{4, 13}, 1u, sti_v, {}},
				semantic_token{{4, 16}, 1u, sti_g, {}},
				semantic_token{{4, 19}, 1u, sti_k2, {}},
				semantic_token{{4, 22}, 1u, sti_m, {}},
				semantic_token{{4, 25}, 1u, sti_n2, {}},
			}, {
				test_code_token{syntax_token::keyword, std::string_view("template")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{syntax_token::nothing_special, std::string_view("<")},
				test_code_token{syntax_token::keyword, std::string_view("typename")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_t1, {}}, std::string_view("T")},
				test_code_token{syntax_token::nothing_special, std::string_view(">")},
				test_code_token{syntax_token::whitespace, std::string_view("\n")},
				test_code_token{identifier_token{sti_bar, {}}, std::string_view("bar")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_foo, {}}, std::string_view("foo")},
				test_code_token{syntax_token::nothing_special, std::string_view("<")},
				test_code_token{identifier_token{sti_t2, {}}, std::string_view("T")},
				test_code_token{syntax_token::nothing_special, std::string_view(">::")},
				test_code_token{identifier_token{sti_f, {}}, std::string_view("f")},
				test_code_token{syntax_token::nothing_special, std::string_view("(")},
				test_code_token{syntax_token::keyword, std::string_view("int")},
				test_code_token{syntax_token::nothing_special, std::string_view("&")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_n1, {}}, std::string_view("n")},
				test_code_token{syntax_token::nothing_special, std::string_view(")")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{syntax_token::keyword, std::string_view("const")},
				test_code_token{syntax_token::whitespace, std::string_view("\n")},
				test_code_token{syntax_token::nothing_special, std::string_view("{")},
				test_code_token{syntax_token::whitespace, std::string_view("\n")},
				test_code_token{syntax_token::whitespace, std::string_view("\t")},
				test_code_token{syntax_token::keyword, std::string_view("auto")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_k1, {}}, std::string_view("k")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{syntax_token::nothing_special, std::string_view("=")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{syntax_token::literal_number, std::string_view("1")},
				test_code_token{syntax_token::nothing_special, std::string_view(";")},
				test_code_token{syntax_token::whitespace, std::string_view("\n")},
				test_code_token{syntax_token::whitespace, std::string_view("\t")},
				test_code_token{syntax_token::keyword, std::string_view("return")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_h, {}}, std::string_view("h")},
				test_code_token{syntax_token::nothing_special, std::string_view("(")},
				test_code_token{identifier_token{sti_t3, {}}, std::string_view("T")},
				test_code_token{syntax_token::nothing_special, std::string_view("::")},
				test_code_token{identifier_token{sti_v, {}}, std::string_view("v")},
				test_code_token{syntax_token::nothing_special, std::string_view(",")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_g, {}}, std::string_view("g")},
				test_code_token{syntax_token::nothing_special, std::string_view(",")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_k2, {}}, std::string_view("k")},
				test_code_token{syntax_token::nothing_special, std::string_view(",")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_m, {}}, std::string_view("m")},
				test_code_token{syntax_token::nothing_special, std::string_view(",")},
				test_code_token{syntax_token::whitespace, std::string_view(" ")},
				test_code_token{identifier_token{sti_n2, {}}, std::string_view("n")},
				test_code_token{syntax_token::nothing_special, std::string_view(");")},
				test_code_token{syntax_token::whitespace, std::string_view("\n")},
				test_code_token{syntax_token::nothing_special, std::string_view("}")},
				test_code_token{syntax_token::whitespace, std::string_view("\n")},
				test_code_token{syntax_token::end_of_input, std::string_view()}
			}
		);
	}

BOOST_AUTO_TEST_SUITE_END()
