#include <ach/core.hpp>
#include <ach/errors.hpp>
#include <ach/text_location.hpp>

#include <boost/test/unit_test.hpp>

#include <string_view>
#include <ostream>
#include <algorithm>

namespace {

void find_and_print_mismatched_line(
	std::string_view expected,
	std::string_view actual,
	boost::test_tools::assertion_result& test_result)
{
	auto const expected_first = expected.data();
	auto const expected_last  = expected.data() + expected.size();
	auto const actual_first   = actual.data();
	auto const actual_last    = actual.data() + actual.size();
	auto const [expected_it, actual_it] = std::mismatch(expected_first, expected_last, actual_first, actual_last);

	auto const line_number = std::count(actual_first, actual_it, '\n');
	test_result.message() << "on line " << line_number << " (expected/actual):\n";

	auto const print_line = [&](auto first, auto pos, auto last) {
		auto const line_first = std::find(std::make_reverse_iterator(pos), std::make_reverse_iterator(first), '\n').base();
		auto const line_last = std::find(pos, last, '\n');

		for (auto it = line_first; it != line_last; ++it)
			test_result.message() << *it;

		test_result.message() << '\n';

		for (auto it = line_first; it != pos; ++it)
			test_result.message() << ' ';

		test_result.message() << "^\n";
	};

	print_line(expected_first, expected_it, expected_last);
	print_line(actual_first, actual_it, actual_last);
}

[[nodiscard]]
boost::test_tools::assertion_result run_and_compare(
	std::string_view input_code,
	std::string_view input_color,
	std::string_view expected_output,
	ach::highlighter_options const& options = {})
{
	std::variant<std::string, ach::highlighter_error> output = ach::run_highlighter(input_code, input_color, options);

	if (auto ptr = std::get_if<ach::highlighter_error>(&output); ptr != nullptr) {
		boost::test_tools::assertion_result test_result(false);
		test_result.message() << *ptr;
		return test_result;
	}

	BOOST_TEST_REQUIRE(std::holds_alternative<std::string>(output));
	auto const actual_output = static_cast<std::string_view>(std::get<std::string>(output));

	if (actual_output == expected_output)
		return true;

	boost::test_tools::assertion_result test_result(false);
	find_and_print_mismatched_line(expected_output, actual_output, test_result);
	return test_result;
}

void check_location(
	char const* location_name,
	ach::text_location expected,
	ach::text_location actual,
	boost::test_tools::assertion_result& test_result)
{
	if (expected.line_number() != actual.line_number()) {
		test_result = false;
		test_result.message() << location_name << ": line numbers differ, expected "
			<< expected.line_number() << " but got " << actual.line_number() << "\n";
	}

	if (expected.line() != actual.line()) {
		test_result = false;
		test_result.message() << location_name << ": lines differ\n"
			"EXPECTED (" << expected.line().size() << " characters):\n" << expected.line() << "\n"
			"ACTUAL (" << actual.line().size() << " characters):\n" << actual.line() << "\n";
	}

	if (expected.first_column() != actual.first_column()) {
		test_result = false;
		test_result.message() << location_name << ": first column numbers differ, "
			"expected " << expected.first_column() << " but got " << actual.first_column() << "\n";
	}

	if (expected.length() != actual.length()) {
		test_result = false;
		test_result.message() << location_name << ": lengths differ, "
			"expected " << expected.length() << " but got " << actual.length() << "\n";
	}
}

[[nodiscard]]
boost::test_tools::assertion_result run_and_expect_error(
	std::string_view input_code,
	std::string_view input_color,
	ach::highlighter_error expected_error,
	ach::highlighter_options const& options = {})
{
	std::variant<std::string, ach::highlighter_error> output = ach::run_highlighter(input_code, input_color, options);

	boost::test_tools::assertion_result test_result(true);

	// we add extra endline before any messages because the initial boost message does not have one
	// and it prints very long lines (includes source path and entire code under BOOST_TEST macro)
	test_result.message() << "\n";

	if (auto ptr = std::get_if<std::string>(&output); ptr != nullptr) {
		test_result = false;
		test_result.message() << "parse succeeded but should not\n"
			"CODE:\n" << input_code << "\n"
			"COLOR:\n" << input_color << "\n"
			"RESULT:\n" << *ptr << "\n";
		return test_result;
	}

	auto const actual_error = std::get<ach::highlighter_error>(output);

	if (expected_error.reason != actual_error.reason) {
		test_result = false;
		test_result.message() << "errors are different\n"
			"EXPECTED: " << expected_error.reason << "\n"
			"ACTUAL: " << actual_error.reason << "\n";
	}

	check_location("code", expected_error.code_location, actual_error.code_location, test_result);
	check_location("color", expected_error.color_location, actual_error.color_location, test_result);

	if (!test_result) {
		test_result.message() << "EXPECTED ERROR:\n" << expected_error << "\n"
			"ACTUAL ERROR:\n" << actual_error << "\n";
	}

	return test_result;
}

}

BOOST_AUTO_TEST_SUITE(highlighter_positive)

	BOOST_AUTO_TEST_CASE(empty_input)
	{
		BOOST_TEST(run_and_compare("", "", ""));
	}

	BOOST_AUTO_TEST_CASE(identifier_span)
	{
		BOOST_TEST(run_and_compare(
			"one two three",
			"keyword keyword keyword",
			"<span class=\"keyword\">one</span> <span class=\"keyword\">two</span> <span class=\"keyword\">three</span>"));
	}

	BOOST_AUTO_TEST_CASE(identifier_symbol)
	{
		BOOST_TEST(run_and_compare(
			"one + two % three",
			"keyword + keyword % keyword",
			"<span class=\"keyword\">one</span> + <span class=\"keyword\">two</span> % <span class=\"keyword\">three</span>"));
	}

	BOOST_AUTO_TEST_CASE(fixed_length_span)
	{
		BOOST_TEST(run_and_compare(
			"onetwothree",
			"3first3second5third",
			"<span class=\"first\">one</span><span class=\"second\">two</span><span class=\"third\">three</span>"));
	}

	BOOST_AUTO_TEST_CASE(fixed_length_span_symbol)
	{
		BOOST_TEST(run_and_compare(
			"one + two % three",
			"3first + 3second % 5third",
			"<span class=\"first\">one</span> + <span class=\"second\">two</span> % <span class=\"third\">three</span>"));
	}

	BOOST_AUTO_TEST_CASE(line_delimited_empty_input)
	{
		BOOST_TEST(run_and_compare(
			"",
			"0com",
			"<span class=\"com\"></span>"));
	}

	BOOST_AUTO_TEST_CASE(line_delimited_empty_remaining)
	{
		BOOST_TEST(run_and_compare(
			"xyz",
			"val0com",
			"<span class=\"val\">xyz</span><span class=\"com\"></span>"));
	}

	BOOST_AUTO_TEST_CASE(line_delimited_word)
	{
		BOOST_TEST(run_and_compare(
			"xd",
			"0com",
			"<span class=\"com\">xd</span>"));
	}

	BOOST_AUTO_TEST_CASE(line_delimited_word_symbol)
	{
		BOOST_TEST(run_and_compare(
			"xd ",
			"0com",
			"<span class=\"com\">xd </span>"));
	}

	BOOST_AUTO_TEST_CASE(line_delimited_span)
	{
		BOOST_TEST(run_and_compare(
			"int x = 0; // abc\n"
			"foo y = 1; // def\n"
			"bar z = 2; // ghi\n",
			"built_in variable = num; 0comment\n"
			"built_in variable = num; 0comment\n"
			"built_in variable = num; 0comment\n",
			"<span class=\"built_in\">int</span> <span class=\"variable\">x</span> = <span class=\"num\">0</span>; <span class=\"comment\">// abc</span>\n"
			"<span class=\"built_in\">foo</span> <span class=\"variable\">y</span> = <span class=\"num\">1</span>; <span class=\"comment\">// def</span>\n"
			"<span class=\"built_in\">bar</span> <span class=\"variable\">z</span> = <span class=\"num\">2</span>; <span class=\"comment\">// ghi</span>\n"));
	}

	BOOST_AUTO_TEST_CASE(symbol)
	{
		BOOST_TEST(run_and_compare(
			"! @ # $ % ^ & * () - + = [ ] { } < > | : . , ; ~ ? /",
			"! @ # $ % ^ & * () - + = [ ] { } < > | : . , ; ~ ? /",
			"! @ # $ % ^ &amp; * () - + = [ ] { } &lt; &gt; | : . , ; ~ ? /"));
	}

	BOOST_AUTO_TEST_CASE(empty_token)
	{
		BOOST_TEST(run_and_compare(
			"run(*args, **kwargs)",
			"func(1oo`param, 2oo`param)",
			"<span class=\"func\">run</span>(<span class=\"oo\">*</span><span class=\"param\">args</span>, <span class=\"oo\">**</span><span class=\"param\">kwargs</span>)"));
	}

	BOOST_AUTO_TEST_CASE(end_of_line)
	{
		BOOST_TEST(run_and_compare(
			"a\nor\n3\n",
			"variable\nkeyword\nnum\n",
			"<span class=\"variable\">a</span>\n<span class=\"keyword\">or</span>\n<span class=\"num\">3</span>\n"));
	}

	BOOST_AUTO_TEST_CASE(whitespace)
	{
		BOOST_TEST(run_and_compare(
			"a b  c   d    e     f",
			"variable variable  variable   variable    variable     variable",
			"<span class=\"variable\">a</span> <span class=\"variable\">b</span>  <span class=\"variable\">c</span>   "
			"<span class=\"variable\">d</span>    <span class=\"variable\">e</span>     <span class=\"variable\">f</span>"));
	}

	BOOST_AUTO_TEST_CASE(whitespace_multiline)
	{
		BOOST_TEST(run_and_compare(
			"a  b  c\n\nd e f\n\n\ng*h^i",
			"variable  variable  variable\n\nvariable 0other\n\n\nvariable*variable^variable",
			"<span class=\"variable\">a</span>  <span class=\"variable\">b</span>  <span class=\"variable\">c</span>\n"
			"\n"
			"<span class=\"variable\">d</span> <span class=\"other\">e f</span>\n"
			"\n"
			"\n"
			"<span class=\"variable\">g</span>*<span class=\"variable\">h</span>^<span class=\"variable\">i</span>"));
	}

	BOOST_AUTO_TEST_CASE(quoted_text)
	{
		BOOST_TEST(run_and_compare(
			R"('foo' + "bar")",
			"chr + str",
			"<span class=\"chr\">'foo'</span> + <span class=\"str\">\"bar\"</span>"));
	}

	BOOST_AUTO_TEST_CASE(quoted_text_escapes)
	{
		BOOST_TEST(run_and_compare(
			R"(X: 'abc' + "string\nwith\bescapes")",
			"variable: chr + str",
			"<span class=\"variable\">X</span>: <span class=\"chr\">'abc'</span> + "
			"<span class=\"str\">\"string<span class=\"str_esc\">\\n</span>with<span class=\"str_esc\">\\b</span>escapes\"</span>"));
	}

	BOOST_AUTO_TEST_CASE(quoted_text_escapes_adjacent)
	{
		BOOST_TEST(run_and_compare(
			R"('a\b\a\bc' + "lorem\n\nipsum\n\0\adolor")",
			"chr + str",
			"<span class=\"chr\">'a<span class=\"chr_esc\">\\b\\a\\b</span>c'</span> + "
			"<span class=\"str\">\"lorem<span class=\"str_esc\">\\n\\n</span>ipsum<span class=\"str_esc\">\\n\\0\\a</span>dolor\"</span>"));
	}

	BOOST_AUTO_TEST_CASE(replace_underscores_to_hyphens)
	{
		BOOST_TEST(run_and_compare(
			"one1 two2 three3",
			"key_word k_e_y_w_o_r_d _keyword_",
			"<span class=\"key-word\">one1</span> <span class=\"k-e-y-w-o-r-d\">two2</span> <span class=\"-keyword-\">three3</span>",
			ach::highlighter_options{ach::generation_options{true}, {}}));
	}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(highlighter_negative)

	BOOST_AUTO_TEST_CASE(exhausted_color)
	{
		BOOST_TEST(run_and_expect_error(
			"sizeof...(Args) <= 123.0f",
			"keyword...(tparam) <= num",
			ach::highlighter_error{
				ach::text_location(1, "keyword...(tparam) <= num", 25, 0),
				ach::text_location(1, "sizeof...(Args) <= 123.0f", 22, 3),
				ach::errors::exhausted_color
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_symbol_missing)
	{
		BOOST_TEST(run_and_expect_error(
			"abc xyz",
			"val val val",
			ach::highlighter_error{
				ach::text_location(1, "val val val", 7, 1),
				ach::text_location(1, "abc xyz", 7, 0),
				ach::errors::expected_symbol
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_symbol_duplicated)
	{
		BOOST_TEST(run_and_expect_error(
			"@ # $",
			"@ # $$",
			ach::highlighter_error{
				ach::text_location(1, "@ # $$", 5, 1),
				ach::text_location(1, "@ # $", 5, 0),
				ach::errors::expected_symbol
			}));
	}

	BOOST_AUTO_TEST_CASE(mismatched_symbol)
	{
		BOOST_TEST(run_and_expect_error(
			"@ # $",
			"@ # ?",
			ach::highlighter_error{
				ach::text_location(1, "@ # ?", 4, 1),
				ach::text_location(1, "@ # $", 4, 1),
				ach::errors::mismatched_symbol
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_identifier_empty)
	{
		BOOST_TEST(run_and_expect_error(
			"abc xyz ",
			"val val val",
			ach::highlighter_error{
				ach::text_location(1, "val val val", 8, 3),
				ach::text_location(1, "abc xyz ", 8, 0),
				ach::errors::expected_identifier
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_identifier_got_number)
	{
		BOOST_TEST(run_and_expect_error(
			"abc xyz 123",
			"val val val",
			ach::highlighter_error{
				ach::text_location(1, "val val val", 8, 3),
				ach::text_location(1, "abc xyz 123", 8, 0),
				ach::errors::expected_identifier
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_number_empty)
	{
		BOOST_TEST(run_and_expect_error(
			"123 456 ",
			"num num num",
			ach::highlighter_error{
				ach::text_location(1, "num num num", 8, 3),
				ach::text_location(1, "123 456 ", 8, 0),
				ach::errors::expected_number
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_number_got_identifier)
	{
		BOOST_TEST(run_and_expect_error(
			"123 456 abc",
			"num num num",
			ach::highlighter_error{
				ach::text_location(1, "num num num", 8, 3),
				ach::text_location(1, "123 456 abc", 8, 0),
				ach::errors::expected_number
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_line_feed_empty)
	{
		BOOST_TEST(run_and_expect_error(
			"abc",
			"val\nval",
			ach::highlighter_error{
				ach::text_location(1, "val\n", 3, 1),
				ach::text_location(1, "abc", 3, 0),
				ach::errors::expected_line_feed
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_line_feed_got_symbol)
	{
		BOOST_TEST(run_and_expect_error(
			"abc@",
			"val\nval",
			ach::highlighter_error{
				ach::text_location(1, "val\n", 3, 1),
				ach::text_location(1, "abc@", 3, 1),
				ach::errors::expected_line_feed
			}));
	}

	BOOST_AUTO_TEST_CASE(insufficient_characters)
	{
		BOOST_TEST(run_and_expect_error(
			"abc xyz\nfoo bar", // extra line of code to make sure remaining characters are checked per line
			"val 5val",
			ach::highlighter_error{
				ach::text_location(1, "val 5val", 4, 4),
				ach::text_location(1, "abc xyz\n", 4, 0),
				ach::errors::insufficient_characters
			}));
	}

	BOOST_AUTO_TEST_CASE(invalid_number)
	{
		BOOST_TEST(run_and_expect_error(
			"abc xyz\nfoo bar",
			"val 123456789012345678901234567890val",
			ach::highlighter_error{
				ach::text_location(1, "val 123456789012345678901234567890val", 4, 30),
				ach::text_location(1, "abc xyz\n", 4, 0),
				ach::errors::invalid_number
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_quoted_missing_end)
	{
		BOOST_TEST(run_and_expect_error(
			"xyz + 'a\tb\bc",
			"val + chr",
			ach::highlighter_error{
				ach::text_location(1, "val + chr", 6, 3),
				ach::text_location(1, "xyz + 'a\tb\bc", 6, 0),
				ach::errors::expected_quoted
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_span_class_empty)
	{
		BOOST_TEST(run_and_expect_error(
			"@ # $",
			"@ # $0",
			ach::highlighter_error{
				ach::text_location(1, "@ # $0", 5, 1),
				ach::text_location(1, "@ # $", 5, 0),
				ach::errors::expected_span_class
			}));
	}

	BOOST_AUTO_TEST_CASE(expected_span_class_got_symbol)
	{
		BOOST_TEST(run_and_expect_error(
			"@ # $",
			"@ # $0!",
			ach::highlighter_error{
				ach::text_location(1, "@ # $0!", 5, 1),
				ach::text_location(1, "@ # $", 5, 0),
				ach::errors::expected_span_class
			}));
	}

BOOST_AUTO_TEST_SUITE_END()
