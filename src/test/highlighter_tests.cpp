#include <ach/core.hpp>
#include <ach/text_location.hpp>

#include <boost/test/unit_test.hpp>

#include <string_view>
#include <iostream>

void print(ach::text_location tl)
{
	std::cout << "line " << tl.line_number() << ":\n" << tl.line() << '\n';

	for (auto i = 0; i < tl.first_column(); ++i)
		std::cout << ' ';

	for (auto i = 0u; i < tl.str().size(); ++i)
		std::cout << '~';

	std::cout << '\n';
}

void print_error(const ach::highlighter_error& error)
{
	std::cout << "ERROR:\n" << error.reason << "\nin code ";
	print(error.code_location);
	std::cout << "in color ";
	print(error.color_location);
}

void find_and_print_mismatched_line(
	std::string_view expected,
	std::string_view actual)
{
	auto const expected_first = expected.data();
	auto const expected_last  = expected.data() + expected.size();
	auto const actual_first   = actual.data();
	auto const actual_last    = actual.data() + actual.size();
	auto const [expected_it, actual_it] = std::mismatch(expected_first, expected_last, actual_first, actual_last);

	auto const line_number = std::count(actual_first, actual_it, '\n');
	std::cout << "on line " << line_number << " (expected/actual):\n";

	auto const print_line = [](auto first, auto pos, auto last) {
		auto const line_first = std::find(std::make_reverse_iterator(pos), std::make_reverse_iterator(first), '\n').base();
		auto const line_last = std::find(pos, last, '\n');

		for (auto it = line_first; it != line_last; ++it)
			std::cout << *it;

		std::cout << '\n';

		for (auto it = line_first; it != pos; ++it)
			std::cout << ' ';

		std::cout << "^\n";
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
		print_error(*ptr);
		return false;
	}

	auto const actual_output = static_cast<std::string_view>(std::get<std::string>(output));

	if (actual_output == expected_output)
		return true;

	find_and_print_mismatched_line(expected_output, actual_output);
	return false;
}

BOOST_AUTO_TEST_SUITE(highlighter_suite)

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

	BOOST_AUTO_TEST_CASE(replace_underscores_to_hyphens)
	{
		BOOST_TEST(run_and_compare(
			"one two three",
			"key_word k_e_y_w_o_r_d _keyword_",
			"<span class=\"key-word\">one</span> <span class=\"k-e-y-w-o-r-d\">two</span> <span class=\"-keyword-\">three</span>",
			ach::highlighter_options{ach::generation_options{true},{}}));
	}

BOOST_AUTO_TEST_SUITE_END()
