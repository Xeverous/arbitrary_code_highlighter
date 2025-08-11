#include <ach/clangd/code_tokenizer.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/text/types.hpp>

#include "clangd_common.hpp"

#include <boost/test/tools/assertion_result.hpp>
#include <boost/test/unit_test.hpp>

#include <string_view>
#include <vector>

using namespace ach;
using namespace ach::clangd;

BOOST_AUTO_TEST_SUITE(code_tokenizer_suite)

	struct test_code_token
	{
		syntax_element_type syntax_element;
		std::string_view str; // text positions will be calculated based on string contents
	};

	code_token make_code_token(test_code_token test_token, text::position current_position)
	{
		text::text_iterator it(test_token.str.data(), current_position);
		const auto last = test_token.str.data() + test_token.str.size();
		while (it.pointer() != last)
			++it;

		return code_token{
			text::fragment{test_token.str, text::range{current_position, it.position()}},
			test_token.syntax_element
		};
	}

	void print_semantic_tokens(std::ostream& os, utility::range<const semantic_token*> sem_tokens)
	{
		for (auto it = sem_tokens.first; it != sem_tokens.last; ++it)
			os << *it << "\n";
	}

	[[nodiscard]] boost::test_tools::assertion_result test_code_tokenizer_impl(
		std::string_view input,
		const std::vector<test_code_token>& expected_output_tokens,
		bool highlight_printf_formatting)
	{
		code_tokenizer tokenizer(input, {keywords.begin(), keywords.begin() + keywords.size()});
		text::position current_position = {};

		for (std::size_t i = 0; i < expected_output_tokens.size(); ++i) {
			if (current_position != tokenizer.current_position()) {
				boost::test_tools::assertion_result result = false;
				result.message().stream() << "positions differ! test position: " << current_position
					<< "tokenizer position: " << tokenizer.current_position();
				return result;
			}

			std::variant<code_token, highlighter_error> token_or_error =
				tokenizer.next_code_token(highlight_printf_formatting);

			if (std::holds_alternative<highlighter_error>(token_or_error)) {
				boost::test_tools::assertion_result result = false;
				result.message().stream() << "unexpected error on iteration " << i
					<< ": " << std::get<highlighter_error>(token_or_error);
				return result;
			}

			const auto expected_code_token = make_code_token(expected_output_tokens[i], current_position);
			const auto actual_code_token = std::get<code_token>(token_or_error);
			current_position = expected_code_token.origin.r.last;

			if (actual_code_token.origin != expected_code_token.origin
				|| actual_code_token.syntax_element != expected_code_token.syntax_element)
			{
				boost::test_tools::assertion_result result = false;
				result.message().stream() << "tokens differ on iteration " << i << ":\n"
					"EXPECTED:\n" << expected_code_token << "ACTUAL:\n" << actual_code_token << "\n";

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

		return true;
	}

	void test_code_tokenizer(
		std::string_view input,
		const std::vector<test_code_token>& expected_output_tokens,
		bool highlight_printf_formatting = false)
	{
		BOOST_TEST(test_code_tokenizer_impl(input, expected_output_tokens, highlight_printf_formatting));
	}

	BOOST_AUTO_TEST_CASE(empty_input)
	{
		test_code_tokenizer("", {
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	// tests most fundamental preprocessor parsing logic
	BOOST_AUTO_TEST_CASE(pp_include)
	{
		test_code_tokenizer("#include <iostream>", {
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("include")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_header_file, std::string_view("<iostream>")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(pp_object_like_macro)
	{
		test_code_tokenizer("#define MACRO f(__FILE__, __LINE__)", {
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("define")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro, std::string_view("MACRO")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("f")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("__FILE__")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(",")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("__LINE__")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(")")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(pp_function_like_macro_multiple_params_with_ellipsis)
	{
		std::string_view input =
			"#define MACRO ( x , yy , zzz, ...) \\\n"
			"\tf1(x, yy); \\\n"
			"\tf2(yy, zzz); \\\n"
			"\tf3(x, zzz)";

		test_code_tokenizer(input, {
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("define")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro, std::string_view("MACRO")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("(")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(",")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("yy")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(",")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("zzz")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(",")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("...")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(")")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" \\\n\t")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("f1")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(",")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("yy")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(")")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(";")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" \\\n\t")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("f2")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("yy")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(",")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("zzz")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(")")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(";")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" \\\n\t")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("f3")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(",")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("zzz")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(")")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(pp_function_like_macro_with_stringize)
	{
		std::string_view input =
			"#define PRINT(x) \\\n"
			"\tstd::cout << #x \" = \" << (x) << \"\\n\"";

		test_code_tokenizer(input, {
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("define")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro, std::string_view("PRINT")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("(")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(")")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" \\\n\t")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("std")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(":")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(":")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("cout")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("<")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("<")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(" = ")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("<")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("<")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("(")},
			test_code_token{syntax_element_type::preprocessor_macro_param, std::string_view("x")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view(")")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("<")},
			test_code_token{syntax_element_type::preprocessor_macro_body, std::string_view("<")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\n")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	// test syntax_element_type::preprocessor_other and
	// the ability to emit syntax_element_type::comment_end upon encountering end of input
	BOOST_AUTO_TEST_CASE(pp_pragma_and_comment)
	{
		test_code_tokenizer("#pragma once // TODO comment", {
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("pragma")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_other, std::string_view("once")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_single, std::string_view("//")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_tag_todo, std::string_view("TODO")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(" comment")},
			test_code_token{syntax_element_type::comment_end, std::string_view()},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	// test a string literal in preprocessor directive
	BOOST_AUTO_TEST_CASE(pp_file_and_string_literal)
	{
		test_code_tokenizer("#file \"main.cpp\" 1", {
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("file")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::literal_string, std::string_view("\"main.cpp\"")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::literal_number, std::string_view("1")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
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

		test_code_tokenizer(input, {
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("ifdef")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro, std::string_view("BUILD_CONFIG")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::whitespace, std::string_view("  ")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("undef")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_macro, std::string_view("BUILD_BUG")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("else")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n\n")},
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::whitespace, std::string_view("  ")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("error")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_other, std::string_view("'whatever\" possibly q'uoted")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("endif")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	// (this is valid code, comments behaviorally act as a single space)
	BOOST_AUTO_TEST_CASE(pp_and_multiline_comment)
	{
		std::string_view input =
			"#include \\\n"
			"/*\n"
			" * text\n"
			" */ <string_view>\n"
			"/* */ #include /*\n"
			"*/ <iostream> // comment\n"
			"\n"
			"/**/int/***/main()//\n"
			"{\n"
			"\t/* */ std /***/:: /**/ cout /***/ << /**/ \"hello, world\" /***/ \"\\n\" /**/; //\n"
			"\tstd::cout << std::string_view(\"a\\0b\\0c\", 5);\n"
			"} //";

		test_code_tokenizer(input, {
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("include")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" \\\n")},
			test_code_token{syntax_element_type::comment_begin_multi, std::string_view("/*")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("\n * text\n ")},
			test_code_token{syntax_element_type::comment_end, std::string_view("*/")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_header_file, std::string_view("<string_view>")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},

			test_code_token{syntax_element_type::comment_begin_multi, std::string_view("/*")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_end, std::string_view("*/")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_hash, std::string_view("#")},
			test_code_token{syntax_element_type::preprocessor_directive, std::string_view("include")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_multi, std::string_view("/*")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("\n")},
			test_code_token{syntax_element_type::comment_end, std::string_view("*/")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::preprocessor_header_file, std::string_view("<iostream>")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_single, std::string_view("//")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(" comment")},
			test_code_token{syntax_element_type::comment_end, std::string_view("\n\n")},

			test_code_token{syntax_element_type::comment_begin_multi, std::string_view("/**/")},
			test_code_token{syntax_element_type::comment_end, std::string_view("")},
			test_code_token{syntax_element_type::keyword, std::string_view("int")},
			test_code_token{syntax_element_type::comment_begin_multi_doxygen, std::string_view("/**")},
			test_code_token{syntax_element_type::comment_end, std::string_view("*/")},
			test_code_token{syntax_element_type::identifier, std::string_view("main")},
			test_code_token{syntax_element_type::symbol, std::string_view("(")},
			test_code_token{syntax_element_type::symbol, std::string_view(")")},
			test_code_token{syntax_element_type::comment_begin_single, std::string_view("//")},
			test_code_token{syntax_element_type::comment_end, std::string_view("\n")},

			test_code_token{syntax_element_type::symbol, std::string_view("{")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},

			test_code_token{syntax_element_type::whitespace, std::string_view("\t")},
			test_code_token{syntax_element_type::comment_begin_multi, std::string_view("/*")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_end, std::string_view("*/")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::identifier, std::string_view("std")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_multi_doxygen, std::string_view("/**")},
			test_code_token{syntax_element_type::comment_end, std::string_view("*/")},
			test_code_token{syntax_element_type::symbol, std::string_view(":")},
			test_code_token{syntax_element_type::symbol, std::string_view(":")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_multi, std::string_view("/**/")},
			test_code_token{syntax_element_type::comment_end, std::string_view("")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::identifier, std::string_view("cout")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_multi_doxygen, std::string_view("/**")},
			test_code_token{syntax_element_type::comment_end, std::string_view("*/")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::symbol, std::string_view("<")},
			test_code_token{syntax_element_type::symbol, std::string_view("<")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_multi, std::string_view("/**/")},
			test_code_token{syntax_element_type::comment_end, std::string_view("")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("hello, world")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_multi_doxygen, std::string_view("/**")},
			test_code_token{syntax_element_type::comment_end, std::string_view("*/")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\n")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_multi, std::string_view("/**/")},
			test_code_token{syntax_element_type::comment_end, std::string_view("")},
			test_code_token{syntax_element_type::symbol, std::string_view(";")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_single, std::string_view("//")},
			test_code_token{syntax_element_type::comment_end, std::string_view("\n")},

			test_code_token{syntax_element_type::whitespace, std::string_view("\t")},
			test_code_token{syntax_element_type::identifier, std::string_view("std")},
			test_code_token{syntax_element_type::symbol, std::string_view(":")},
			test_code_token{syntax_element_type::symbol, std::string_view(":")},
			test_code_token{syntax_element_type::identifier, std::string_view("cout")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::symbol, std::string_view("<")},
			test_code_token{syntax_element_type::symbol, std::string_view("<")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::identifier, std::string_view("std")},
			test_code_token{syntax_element_type::symbol, std::string_view(":")},
			test_code_token{syntax_element_type::symbol, std::string_view(":")},
			test_code_token{syntax_element_type::identifier, std::string_view("string_view")},
			test_code_token{syntax_element_type::symbol, std::string_view("(")},
			test_code_token{syntax_element_type::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("a")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\0")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("b")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\0")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("c")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_element_type::symbol, std::string_view(",")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::literal_number, std::string_view("5")},
			test_code_token{syntax_element_type::symbol, std::string_view(")")},
			test_code_token{syntax_element_type::symbol, std::string_view(";")},
			test_code_token{syntax_element_type::whitespace, std::string_view("\n")},
			test_code_token{syntax_element_type::symbol, std::string_view("}")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::comment_begin_single, std::string_view("//")},
			test_code_token{syntax_element_type::comment_end, std::string_view("")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()},
		});
	}

	// test multiple keywords, unknown identifier, symbol, C++14 literal and literal suffix
	BOOST_AUTO_TEST_CASE(var_definition_and_literal_suffix)
	{
		test_code_tokenizer("const long long x = 123'456'789ll;", {
			test_code_token{syntax_element_type::keyword, std::string_view("const")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::keyword, std::string_view("long")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::keyword, std::string_view("long")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::identifier, std::string_view("x")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::symbol, std::string_view("=")},
			test_code_token{syntax_element_type::whitespace, std::string_view(" ")},
			test_code_token{syntax_element_type::literal_number, std::string_view("123'456'789")},
			test_code_token{syntax_element_type::literal_suffix, std::string_view("ll")},
			test_code_token{syntax_element_type::symbol, std::string_view(";")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(escape_and_format_sequences)
	{
		test_code_tokenizer(R"("abc%s%% \\%+.?";)", {
			test_code_token{syntax_element_type::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("abc%s%% ")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\\\")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("%+.?")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_element_type::symbol, std::string_view(";")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});

		test_code_tokenizer(R"("abc%s%% \\%+.?";)", {
			test_code_token{syntax_element_type::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("abc")},
			test_code_token{syntax_element_type::format_sequence, std::string_view("%s%%")},
			test_code_token{syntax_element_type::nothing_special, std::string_view(" ")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\\\")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("%+.?")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_element_type::symbol, std::string_view(";")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		}, true);
	}

	BOOST_AUTO_TEST_CASE(literal_prefix)
	{
		test_code_tokenizer("f(L'\\0');", {
			test_code_token{syntax_element_type::identifier, std::string_view("f")},
			test_code_token{syntax_element_type::symbol, std::string_view("(")},
			test_code_token{syntax_element_type::literal_prefix, std::string_view("L")},
			test_code_token{syntax_element_type::literal_char_begin, std::string_view("'")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\0")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("'")},
			test_code_token{syntax_element_type::symbol, std::string_view(")")},
			test_code_token{syntax_element_type::symbol, std::string_view(";")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(literal_suffix)
	{
		test_code_tokenizer(R"(f(u8"w\0x\"y\xffz"s);)", {
			test_code_token{syntax_element_type::identifier, std::string_view("f")},
			test_code_token{syntax_element_type::symbol, std::string_view("(")},
			test_code_token{syntax_element_type::literal_prefix, std::string_view("u8")},
			test_code_token{syntax_element_type::literal_string_begin, std::string_view("\"")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("w")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\0")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("x")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\\"")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("y")},
			test_code_token{syntax_element_type::escape_sequence, std::string_view("\\xff")},
			test_code_token{syntax_element_type::nothing_special, std::string_view("z")},
			test_code_token{syntax_element_type::literal_text_end, std::string_view("\"")},
			test_code_token{syntax_element_type::literal_suffix, std::string_view("s")},
			test_code_token{syntax_element_type::symbol, std::string_view(")")},
			test_code_token{syntax_element_type::symbol, std::string_view(";")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

	BOOST_AUTO_TEST_CASE(raw_string_literal)
	{
		std::string_view input = R"(f(u8R"test(raw\nstring\nliteral)test");)";

		test_code_tokenizer(input, {
			test_code_token{syntax_element_type::identifier, std::string_view("f")},
			test_code_token{syntax_element_type::symbol, std::string_view("(")},
			test_code_token{syntax_element_type::literal_prefix, std::string_view("u8R")},
			test_code_token{syntax_element_type::literal_string_raw_quote, std::string_view("\"")},
			test_code_token{syntax_element_type::literal_string_raw_delimeter, std::string_view("test")},
			test_code_token{syntax_element_type::literal_string_raw_paren, std::string_view("(")},
			test_code_token{syntax_element_type::literal_string, std::string_view("raw\\nstring\\nliteral")},
			test_code_token{syntax_element_type::literal_string_raw_paren, std::string_view(")")},
			test_code_token{syntax_element_type::literal_string_raw_delimeter, std::string_view("test")},
			test_code_token{syntax_element_type::literal_string_raw_quote, std::string_view("\"")},
			test_code_token{syntax_element_type::symbol, std::string_view(")")},
			test_code_token{syntax_element_type::symbol, std::string_view(";")},
			test_code_token{syntax_element_type::end_of_input, std::string_view()}
		});
	}

BOOST_AUTO_TEST_SUITE_END()
