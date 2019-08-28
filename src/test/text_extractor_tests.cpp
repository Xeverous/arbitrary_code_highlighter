#include <ach/text_extractor.hpp>

#include <boost/test/unit_test.hpp>

#include <initializer_list>

namespace ut = boost::unit_test;
namespace tt = boost::test_tools;

BOOST_AUTO_TEST_SUITE(text_extractor_suite)

	BOOST_AUTO_TEST_CASE(empty_input_invariants)
	{
		ach::text_extractor te("");
		BOOST_TEST(!te.peek_next_char().has_value());
		BOOST_TEST(te.has_reached_end());
		ach::text_location const loc = te.current_location();
		BOOST_TEST(loc.line().empty());
		BOOST_TEST(loc.str().empty());
		BOOST_TEST(loc.first_column() == 0);
	}

	enum class extraction_operation { identifier, number, n_characters, until_end_of_line };

	struct extraction_step
	{
		extraction_step( // non n_characters ctor
			extraction_operation operation,
			std::string_view expected_token,
			int expected_line_number,
			int expected_first_column)
		: operation(operation)
		, expected_token(expected_token)
		, expected_line_number(expected_line_number)
		, expected_first_column(expected_first_column) {}

		extraction_step( // n_characters ctor
			int n,
			std::string_view expected_token,
			int expected_line_number,
			int expected_first_column)
		: operation(extraction_operation::n_characters)
		, n(n)
		, expected_token(expected_token)
		, expected_line_number(expected_line_number)
		, expected_first_column(expected_first_column)
		{
			BOOST_TEST_REQUIRE(n == static_cast<int>(expected_token.size()),
				"test written incorrectly: length of expected token does not match n");
		}

		extraction_operation operation;
		int n = 0; // only for extraction_operation::n_characters
		std::string_view expected_token;
		int expected_line_number = 0;
		int expected_first_column = 0;
	};

	tt::assertion_result test_text_extractor(
		std::string_view input,
		std::initializer_list<extraction_step> steps)
	{
		BOOST_TEST_MESSAGE("Testing text_extractor for input \"" << input << "\"");

		ach::text_extractor te(input);

		for (extraction_step step : steps) {
			BOOST_TEST_REQUIRE(
				!te.has_reached_end(),
				"text extractor should not reach end while steps are remaining");

			auto const loc = [&]() -> ach::text_location {
				if (step.operation == extraction_operation::identifier) {
					return te.extract_identifier();
				}
				else if (step.operation == extraction_operation::number) {
					return te.extract_number();
				}
				else if (step.operation == extraction_operation::n_characters) {
					return te.extract_n_characters(step.n);
				}
				else if (step.operation == extraction_operation::until_end_of_line) {
					return te.extract_until_end_of_line();
				}
				else {
					BOOST_FAIL("test bug: non-exhaustive operation implementation");
					return ach::text_location();
				}
			}();

			BOOST_TEST_MESSAGE("expecting \"" << step.expected_token << "\" token");
			BOOST_TEST(loc.line_number() == step.expected_line_number);
			BOOST_TEST(loc.first_column() == step.expected_first_column);
			BOOST_TEST(step.expected_token == loc.str(), tt::per_element());
		}

		BOOST_TEST_REQUIRE(te.has_reached_end(), "text extractor should reach end after all steps");

		return true;
	}


	BOOST_AUTO_TEST_SUITE(main_text_extractor_tests,
		* ut::depends_on("text_extractor_suite/empty_input_invariants"))

		BOOST_AUTO_TEST_CASE(empty_input)
		{
			BOOST_TEST(test_text_extractor("", {}));
		}

		BOOST_AUTO_TEST_CASE(identifiers_space_breaked)
		{
			auto const steps = {
				extraction_step(extraction_operation::identifier, "abc", 1, 0),
				extraction_step(1, " ", 1, 3),
				extraction_step(extraction_operation::identifier, "def", 1, 4),
				extraction_step(1, " ", 1, 7),
				extraction_step(extraction_operation::identifier, "ghi", 1, 8),
			};
			BOOST_TEST(test_text_extractor("abc def ghi", steps));
		}

		BOOST_AUTO_TEST_CASE(identifiers_line_breaked)
		{
			auto const steps = {
				extraction_step(extraction_operation::identifier, "abc", 1, 0),
				extraction_step(1, "\n", 1, 3),
				extraction_step(extraction_operation::identifier, "def", 2, 0),
				extraction_step(1, "\n", 2, 3),
				extraction_step(extraction_operation::identifier, "ghi", 3, 0),
				extraction_step(1, "\n", 3, 3)
			};
			BOOST_TEST(test_text_extractor("abc\ndef\nghi\n", steps));
		}

		BOOST_AUTO_TEST_CASE(identifiers_number_breaked)
		{
			auto const steps = {
				extraction_step(extraction_operation::identifier, "a",   1, 0),
				extraction_step(extraction_operation::number,     "1",   1, 1),
				extraction_step(extraction_operation::identifier, "bb",  1, 2),
				extraction_step(extraction_operation::number,     "22",  1, 4),
				extraction_step(extraction_operation::identifier, "ccc", 1, 6),
				extraction_step(extraction_operation::number,     "333", 1, 9),
			};
			BOOST_TEST(test_text_extractor("a1bb22ccc333", steps));
		}

		BOOST_AUTO_TEST_CASE(n_characters)
		{
			auto const steps = {
				extraction_step(1, "a",    1, 0),
				extraction_step(2, "bb",   1, 1),
				extraction_step(3, "ccc",  1, 3),
				extraction_step(4, "dddd", 1, 6),
				extraction_step(1, "e",    1, 10)
			};
			BOOST_TEST(test_text_extractor("abbcccdddde", steps));
		}

		BOOST_AUTO_TEST_CASE(line_breaks)
		{
			auto const steps = {
				extraction_step(extraction_operation::until_end_of_line, "X",   1, 0),
				extraction_step(1, "\n", 1, 1),
				extraction_step(extraction_operation::until_end_of_line, "YY",  2, 0),
				extraction_step(1, "\n", 2, 2),
				extraction_step(extraction_operation::until_end_of_line, "ZZZ", 3, 0),
				extraction_step(1, "\n", 3, 3),
				extraction_step(extraction_operation::until_end_of_line, "",    4, 0),
				extraction_step(1, "\n", 4, 0),
				extraction_step(extraction_operation::until_end_of_line, "",    5, 0),
				extraction_step(1, "\n", 5, 0),
				extraction_step(extraction_operation::until_end_of_line, "",    6, 0),
				extraction_step(1, "\n", 6, 0)
			};
			BOOST_TEST(test_text_extractor("X\nYY\nZZZ\n\n\n\n", steps));
		}

		BOOST_AUTO_TEST_CASE(all)
		{
			auto const steps = {
				extraction_step(extraction_operation::identifier, "ccc", 1, 0),
				extraction_step(1, " ",  1, 3),
				extraction_step(extraction_operation::identifier, "x",   1, 4),
				extraction_step(1, "\n", 1, 5),
				extraction_step(extraction_operation::identifier, "XYZ", 2, 0),
				extraction_step(1, " ",  2, 3),
				extraction_step(extraction_operation::identifier, "__",  2, 4),
				extraction_step(extraction_operation::number,     "123", 2, 6),
				extraction_step(extraction_operation::until_end_of_line, " a % 5", 2, 9),
				extraction_step(1, "\n", 2, 15),
				extraction_step(2, "*$", 3,  0),
			};
			BOOST_TEST(test_text_extractor("ccc x\nXYZ __123 a % 5\n*$", steps));
		}

	BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()