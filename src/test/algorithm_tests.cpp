#include <ach/utility/algorithm.hpp>
#include <ach/text/utils.hpp>

#include <boost/test/tools/interface.hpp>
#include <boost/test/unit_test.hpp>

#include <cstddef>
#include <string_view>

using namespace ach;

BOOST_AUTO_TEST_SUITE(algorithm_search_whole_word)

	boost::test_tools::assertion_result run_and_compare(
		std::string_view input, std::string_view pattern, std::ptrdiff_t expected_pos)
	{
		const auto it = utility::search_whole_word(
			input.begin(), input.end(), pattern.begin(), pattern.end(), text::is_alnum_or_underscore);
		std::ptrdiff_t actual_pos = it - input.begin();

		// this is for pretty print
		BOOST_TEST(expected_pos == actual_pos);

		return expected_pos == actual_pos;
	}

	// overload for when no result is expected
	boost::test_tools::assertion_result run_and_compare(
		std::string_view input, std::string_view pattern)
	{
		return run_and_compare(input, pattern, input.size());
	}

	BOOST_AUTO_TEST_CASE(empty_input_empty_pattern)
	{
		BOOST_TEST(run_and_compare("", ""));
	}

	BOOST_AUTO_TEST_CASE(empty_input)
	{
		BOOST_TEST(run_and_compare("", "a"));
	}

	BOOST_AUTO_TEST_CASE(empty_pattern)
	{
		BOOST_TEST(run_and_compare("a", "", 0));
	}

	BOOST_AUTO_TEST_CASE(same_input_and_pattern)
	{
		BOOST_TEST(run_and_compare("a", "a", 0));
	}

	BOOST_AUTO_TEST_CASE(result_at_begin)
	{
		BOOST_TEST(run_and_compare("aaa;bbb;ccc", "aaa", 0));
	}

	BOOST_AUTO_TEST_CASE(result_inside)
	{
		BOOST_TEST(run_and_compare("aaa;bbb;ccc", "bbb", 4));
	}

	BOOST_AUTO_TEST_CASE(result_at_end)
	{
		BOOST_TEST(run_and_compare("aaa;bbb;ccc", "ccc", 8));
	}

	BOOST_AUTO_TEST_CASE(pattern_too_short)
	{
		BOOST_TEST(run_and_compare("aaa;bbb;ccc", "bb"));
	}

	BOOST_AUTO_TEST_CASE(pattern_too_long)
	{
		BOOST_TEST(run_and_compare("aaa;bbb;ccc", "bbbb"));
	}

	BOOST_AUTO_TEST_CASE(multiple_delimeters)
	{
		BOOST_TEST(run_and_compare(";;aaa;;bbb;;ccc;;", "bbb", 7));
	}

BOOST_AUTO_TEST_SUITE_END()
