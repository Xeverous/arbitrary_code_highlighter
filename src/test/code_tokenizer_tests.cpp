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

	boost::test_tools::assertion_result compare_tokens(code_token expected, code_token actual)
	{
		if (expected != actual) {
			boost::test_tools::assertion_result result = false;
			result.message().stream() << "tokens differ:\nEXPECTED:\n" << expected << "ACTUAL:\n" << actual;
			return result;
		}

		return true;
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

	boost::test_tools::assertion_result test_code_tokenizer(
		std::string_view input,
		const std::vector<semantic_token>& sem_tokens,
		const std::vector<test_code_token>& test_tokens)
	{
		auto tokenizer = make_code_tokenizer(input, {sem_tokens.data(), sem_tokens.data() + sem_tokens.size()});
		text::position current_position = {};

		for (std::size_t i = 0; i < test_tokens.size(); ++i) {
			std::variant<code_token, highlighter_error> token_or_error = tokenizer.next_code_token(
				{keywords.begin(), keywords.begin() + keywords.size()});

			if (std::holds_alternative<highlighter_error>(token_or_error)) {
				boost::test_tools::assertion_result result = false;
				result.message().stream() << "unexpected error on iteration " << i
					<< ": " << std::get<highlighter_error>(token_or_error);
				return result;
			}

			code_token expected_code_token = make_code_token(test_tokens[i], current_position);
			current_position = expected_code_token.origin.r.last;
			BOOST_TEST_REQUIRE(compare_tokens(expected_code_token, std::get<code_token>(token_or_error)));
		}

		if (!tokenizer.has_reached_end()) {
			boost::test_tools::assertion_result result = false;
			result.message().stream() << "tokenizer did not reached end, pos = [" << tokenizer.current_position() << "]\n"
				"context state: " << utility::to_string(tokenizer.current_context_state()) << "\n"
				"preprocessor state: " << utility::to_string(tokenizer.current_preprocessor_state()) << "\n";
			return result;
		}

		return true;
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

	// test preprocessor object-like macro
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

	// test syntax_token::preprocessor_other and
	// the ability to emit syntax_token::comment_end upon encountering end of input
	BOOST_AUTO_TEST_CASE(pp_pragma_and_comment)
	{
		test_code_tokenizer("#pragma once // comment", {}, {
			test_code_token{syntax_token::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_token::preprocessor_directive, std::string_view("pragma")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::preprocessor_other, std::string_view("once")},
			test_code_token{syntax_token::whitespace, std::string_view(" ")},
			test_code_token{syntax_token::comment_begin_single, std::string_view("//")},
			test_code_token{syntax_token::nothing_special, std::string_view(" comment")},
			test_code_token{syntax_token::comment_end, std::string_view()},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(var_definition)
	{
		test_code_tokenizer("const long long x = 123'456'789;", {}, {
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
			test_code_token{syntax_token::nothing_special, std::string_view(";")},
			test_code_token{syntax_token::end_of_input, std::string_view()}
		});
	}

BOOST_AUTO_TEST_SUITE_END()
