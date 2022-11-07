#include <ach/text/extractor.hpp>

#include <boost/test/unit_test.hpp>

#include <initializer_list>

namespace ut = boost::unit_test;
namespace tt = boost::test_tools;

BOOST_AUTO_TEST_SUITE(text_extractor_suite)

	BOOST_AUTO_TEST_CASE(empty_input_invariants)
	{
		ach::text::extractor te("");
		BOOST_TEST(!te.peek_next_char().has_value());
		BOOST_TEST(te.has_reached_end());
		const ach::text::located_span loc = te.current_location();
		BOOST_TEST(loc.whole_line().empty());
		BOOST_TEST(loc.str().empty());
		BOOST_TEST(loc.span().column == 0);
	}

	enum class extraction_operation { alphas_underscores, digits, identifier, n_characters, until_end_of_line, quoted };

	struct extraction_step
	{
		extraction_step( // non n_characters ctor
			extraction_operation operation,
			std::string_view expected_token,
			std::size_t expected_line_number,
			std::size_t expected_first_column)
		: operation(operation)
		, expected_token(expected_token)
		, expected_line_number(expected_line_number)
		, expected_first_column(expected_first_column)
		{
		}

		extraction_step( // n_characters ctor
			std::string_view expected_token,
			std::size_t expected_line_number,
			std::size_t expected_first_column,
			bool load_next_line = false)
		: operation(extraction_operation::n_characters)
		, expected_token(expected_token)
		, expected_line_number(expected_line_number)
		, expected_first_column(expected_first_column)
		, load_next_line(load_next_line)
		{
		}

		extraction_operation operation;
		std::string_view expected_token;
		std::size_t expected_line_number = 0;
		std::size_t expected_first_column = 0;

		// only for extraction_operation::n_characters
		bool load_next_line = false;
	};

	tt::assertion_result test_text_extractor(
		std::string_view input,
		std::initializer_list<extraction_step> steps)
	{
		BOOST_TEST_MESSAGE("Testing text::extractor for input \"" << input << "\"");

		ach::text::extractor te(input);

		for (extraction_step step : steps) {
			BOOST_TEST_REQUIRE(
				!te.has_reached_end(),
				"text extractor should not reach end while steps are remaining");

			const auto loc = [&]() -> ach::text::located_span {
				if (step.operation == extraction_operation::alphas_underscores) {
					return te.extract_alphas_underscores();
				}
				else if (step.operation == extraction_operation::digits) {
					return te.extract_digits();
				}
				else if (step.operation == extraction_operation::identifier) {
					return te.extract_identifier();
				}
				else if (step.operation == extraction_operation::n_characters) {
					ach::text::located_span loc = te.extract_n_characters(step.expected_token.size());

					if (step.load_next_line)
						BOOST_TEST(te.load_next_line());

					return loc;
				}
				else if (step.operation == extraction_operation::until_end_of_line) {
					return te.extract_until_end_of_line();
				}
				else if (step.operation == extraction_operation::quoted) {
					return te.extract_quoted('\'', '\\');
				}
				else {
					BOOST_FAIL("test bug: non-exhaustive operation implementation");
					return ach::text::located_span({}, 0, 0, 0);
				}
			}();

			BOOST_TEST_MESSAGE("expecting \"" << step.expected_token << "\" token");
			BOOST_TEST(loc.span().line == step.expected_line_number);
			BOOST_TEST(loc.span().column == step.expected_first_column);
			BOOST_TEST(step.expected_token == loc.str(), tt::per_element());
		}

		BOOST_TEST_REQUIRE(te.has_reached_end(), "text extractor should reach end after all steps");

		return true;
	}


	BOOST_AUTO_TEST_SUITE(main_text_extractor_tests,
		* ut::depends_on("text_extractor_suite/empty_input_invariants"))
		/*
		 * Do not write "const auto steps =" in the tests, there is a bug in GCC with
		 * initializer list type deduction which breaks constness and causes compile error.
		 *
		 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63149
		 * https://stackoverflow.com/questions/32515183
		 */
		BOOST_AUTO_TEST_CASE(empty_input)
		{
			BOOST_TEST(test_text_extractor("", {}));
		}

		BOOST_AUTO_TEST_CASE(alphas_underscores_space_breaked)
		{
			auto steps = {
				extraction_step(extraction_operation::identifier, "a_c", 0, 0),
				extraction_step(" ", 0, 3),
				extraction_step(extraction_operation::identifier, "d_f", 0, 4),
				extraction_step(" ", 0, 7),
				extraction_step(extraction_operation::identifier, "g_i", 0, 8),
			};
			BOOST_TEST(test_text_extractor("a_c d_f g_i", steps));
		}

		BOOST_AUTO_TEST_CASE(identifiers_space_breaked)
		{
			auto steps = {
				extraction_step(extraction_operation::identifier, "abc", 0, 0),
				extraction_step(" ", 0, 3),
				extraction_step(extraction_operation::identifier, "def", 0, 4),
				extraction_step(" ", 0, 7),
				extraction_step(extraction_operation::identifier, "ghi", 0, 8),
			};
			BOOST_TEST(test_text_extractor("abc def ghi", steps));
		}

		BOOST_AUTO_TEST_CASE(identifiers_alnum_space_breaked)
		{
			auto steps = {
				extraction_step(extraction_operation::identifier, "ab1c", 0, 0),
				extraction_step(" ", 0, 4),
				extraction_step(extraction_operation::identifier, "def2", 0, 5),
				extraction_step(" ", 0, 9),
				extraction_step(extraction_operation::identifier, "g4hi5", 0, 10),
			};
			BOOST_TEST(test_text_extractor("ab1c def2 g4hi5", steps));
		}

		BOOST_AUTO_TEST_CASE(identifiers_alnum_empty_token_breaked)
		{
			auto steps = {
				extraction_step(extraction_operation::identifier, "ab1c", 0, 0),
				extraction_step("`", 0, 4),
				extraction_step(extraction_operation::identifier, "def2", 0, 5),
				extraction_step("`", 0, 9),
				extraction_step(extraction_operation::identifier, "g4hi5", 0, 10),
			};
			BOOST_TEST(test_text_extractor("ab1c`def2`g4hi5", steps));
		}

		BOOST_AUTO_TEST_CASE(identifiers_line_breaked)
		{
			auto steps = {
				extraction_step(extraction_operation::identifier, "abc", 0, 0),
				extraction_step("\n", 0, 3, true),
				extraction_step(extraction_operation::identifier, "def", 1, 0),
				extraction_step("\n", 1, 3, true),
				extraction_step(extraction_operation::identifier, "ghi", 2, 0),
				extraction_step("\n", 2, 3)
			};
			BOOST_TEST(test_text_extractor("abc\ndef\nghi\n", steps));
		}

		BOOST_AUTO_TEST_CASE(alphas_underscores_number_breaked)
		{
			auto steps = {
				extraction_step(extraction_operation::alphas_underscores, "a",   0, 0),
				extraction_step(extraction_operation::digits,             "1",   0, 1),
				extraction_step(extraction_operation::alphas_underscores, "bb",  0, 2),
				extraction_step(extraction_operation::digits,             "22",  0, 4),
				extraction_step(extraction_operation::alphas_underscores, "ccc", 0, 6),
				extraction_step(extraction_operation::digits,             "333", 0, 9),
			};
			BOOST_TEST(test_text_extractor("a1bb22ccc333", steps));
		}

		BOOST_AUTO_TEST_CASE(n_characters)
		{
			auto steps = {
				extraction_step("a",    0, 0),
				extraction_step("bb",   0, 1),
				extraction_step("ccc",  0, 3),
				extraction_step("dddd", 0, 6),
				extraction_step("e",    0, 10)
			};
			BOOST_TEST(test_text_extractor("abbcccdddde", steps));
		}

		BOOST_AUTO_TEST_CASE(line_breaks)
		{
			auto steps = {
				extraction_step(extraction_operation::until_end_of_line, "X",   0, 0),
				extraction_step("\n", 0, 1, true),
				extraction_step(extraction_operation::until_end_of_line, "YY",  1, 0),
				extraction_step("\n", 1, 2, true),
				extraction_step(extraction_operation::until_end_of_line, "ZZZ", 2, 0),
				extraction_step("\n", 2, 3, true),
				extraction_step(extraction_operation::until_end_of_line, "",    3, 0),
				extraction_step("\n", 3, 0, true),
				extraction_step(extraction_operation::until_end_of_line, "",    4, 0),
				extraction_step("\n", 4, 0, true),
				extraction_step(extraction_operation::until_end_of_line, "",    5, 0),
				extraction_step("\n", 5, 0)
			};
			BOOST_TEST(test_text_extractor("X\nYY\nZZZ\n\n\n\n", steps));
		}

		BOOST_AUTO_TEST_CASE(quoted_multiple)
		{
			auto steps = {
				extraction_step(extraction_operation::quoted, "'abc'", 0, 0),
				extraction_step(extraction_operation::identifier, "XYZ", 0, 5),
				extraction_step(extraction_operation::quoted, "'def'", 0, 8),
				extraction_step(" ", 0, 13),
				extraction_step(extraction_operation::digits, "123", 0, 14),
				extraction_step("==", 0, 17),
				extraction_step(extraction_operation::quoted, "'cccc'", 0, 19),
				extraction_step(":", 0, 25),
				extraction_step(extraction_operation::quoted, "'XX'", 0, 26)
			};
			BOOST_TEST(test_text_extractor("'abc'XYZ'def' 123=='cccc':'XX'", steps));
		}

		BOOST_AUTO_TEST_CASE(quoted_escape)
		{
			const std::string_view text = R"('abc\na\b\bccc\\\nX')";
			auto steps = {
				extraction_step(extraction_operation::quoted, text, 0, 0)
			};
			BOOST_TEST(test_text_extractor(text, steps));
		}

		BOOST_AUTO_TEST_CASE(quoted_escape_multiple)
		{
			auto steps = {
				extraction_step(extraction_operation::quoted, R"('abc\n')", 0, 0),
				extraction_step(extraction_operation::alphas_underscores, "XYZ", 0, 7),
				extraction_step(extraction_operation::digits, "4", 0, 10),
				extraction_step(extraction_operation::quoted, R"('a\bc')", 0, 11),
				extraction_step(" ", 0, 17),
				extraction_step(extraction_operation::digits, "543", 0, 18),
				extraction_step("=", 0, 21),
				extraction_step(extraction_operation::quoted, R"('cc\\\\')", 0, 22),
				extraction_step(":", 0, 30),
				extraction_step(extraction_operation::quoted, R"('\nX')", 0, 31)
			};
			BOOST_TEST(test_text_extractor(R"('abc\n'XYZ4'a\bc' 543='cc\\\\':'\nX')", steps));
		}

		BOOST_AUTO_TEST_CASE(all)
		{
			auto steps = {
				extraction_step(extraction_operation::identifier, "ccc7", 0, 0),
				extraction_step(" ",  0,  4),
				extraction_step(extraction_operation::identifier, "x",    0, 5),
				extraction_step("\n", 0,  6, true),
				extraction_step(extraction_operation::identifier, "X",  1, 0),
				extraction_step("`",  1,  1),
				extraction_step(extraction_operation::identifier, "Z",  1, 2),
				extraction_step(" ",  1,  3),
				extraction_step(extraction_operation::alphas_underscores, "__", 1, 4),
				extraction_step(extraction_operation::digits,     "123", 1, 6),
				extraction_step(extraction_operation::until_end_of_line, " a % 5", 1, 9),
				extraction_step("\n", 1, 15, true),
				extraction_step("*$", 2,  0),
				extraction_step(extraction_operation::quoted, "'a\\bc'", 2, 2),
				extraction_step(" ",  2,  8),
				extraction_step(extraction_operation::quoted, "''", 2, 9)
			};
			BOOST_TEST(test_text_extractor("ccc7 x\nX`Z __123 a % 5\n*$'a\\bc' ''", steps));
		}

	BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
