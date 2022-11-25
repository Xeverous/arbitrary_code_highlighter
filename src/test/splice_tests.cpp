#include <ach/clangd/splice_utils.hpp>
#include <ach/clangd/spliced_text_iterator.hpp>
#include <ach/clangd/spliced_text_parser.hpp>
#include <ach/text/types.hpp>

#include <boost/test/unit_test.hpp>

#include <ostream>
#include <string_view>

namespace ach::text {

std::ostream& boost_test_print_type(std::ostream& os, position pos)
{
	return os << "line: " << pos.line << ", column: " << pos.column;
}

std::ostream& boost_test_print_type(std::ostream& os, range r)
{
	os << "(";
	boost_test_print_type(os, r.first);
	os << "), (";
	boost_test_print_type(os, r.last);
	return os << ")";
}

}

using namespace ach;

BOOST_AUTO_TEST_SUITE(splice_utils)

	BOOST_AUTO_TEST_CASE(front_splice_length)
	{
		BOOST_TEST(clangd::front_splice_length("") == 0u);
		BOOST_TEST(clangd::front_splice_length("\\") == 0u);
		BOOST_TEST(clangd::front_splice_length("\\ ") == 0u);
		BOOST_TEST(clangd::front_splice_length("\\\n") == 2u);
		BOOST_TEST(clangd::front_splice_length("\\ \n") == 3u);
		BOOST_TEST(clangd::front_splice_length("\\\t\n") == 3u);
		BOOST_TEST(clangd::front_splice_length("\\ ") == 0u);
		BOOST_TEST(clangd::front_splice_length("\\\\\n") == 0u);
	}

	boost::test_tools::assertion_result test_remove_front_splices(
		std::string_view input, std::string_view expected_result, text::position expected_result_pos)
	{
		text::position pos;
		clangd::remove_front_splices(input, pos);

		// this is for pretty print
		BOOST_TEST(input == expected_result);
		BOOST_TEST(pos == expected_result_pos);

		return input == expected_result && pos == expected_result_pos;
	}

	BOOST_AUTO_TEST_CASE(remove_front_splices)
	{
		BOOST_TEST(test_remove_front_splices("", "", {0, 0}));
		BOOST_TEST(test_remove_front_splices("abc", "abc", {0, 0}));
		BOOST_TEST(test_remove_front_splices("a\\\nbc", "a\\\nbc", {0, 0}));
		BOOST_TEST(test_remove_front_splices("a\\ \nbc", "a\\ \nbc", {0, 0}));
		BOOST_TEST(test_remove_front_splices("\\abc", "\\abc", {0, 0}));
		BOOST_TEST(test_remove_front_splices("\\ abc", "\\ abc", {0, 0}));
		BOOST_TEST(test_remove_front_splices("\\\nabc", "abc", {1, 0}));
		BOOST_TEST(test_remove_front_splices("\\ \nabc", "abc", {1, 0}));
		BOOST_TEST(test_remove_front_splices("\\ \n\\\nabc", "abc", {2, 0}));
		BOOST_TEST(test_remove_front_splices("\\\n\\ \nabc", "abc", {2, 0}));
		BOOST_TEST(test_remove_front_splices("\\ \n\\\na\\\nbc", "a\\\nbc", {2, 0}));
	}

	BOOST_AUTO_TEST_CASE(ends_with_backslash_whitespace)
	{
		BOOST_TEST(!clangd::ends_with_backslash_whitespace(""));
		BOOST_TEST(!clangd::ends_with_backslash_whitespace(" "));
		BOOST_TEST(!clangd::ends_with_backslash_whitespace("abc"));
		BOOST_TEST(!clangd::ends_with_backslash_whitespace("abc "));
		BOOST_TEST(!clangd::ends_with_backslash_whitespace("\\\nabc"));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("\\"));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("\\ "));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("abc\\"));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("abc\\ "));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("abc\\  "));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("abc\\\\"));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("abc\\\\ "));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("abc\\\\  "));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("\\\nabc\\"));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("\\\nabc\\ "));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("\\\nabc\\  "));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("\\\nabc\\\\"));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("\\\nabc\\\\ "));
		BOOST_TEST(clangd::ends_with_backslash_whitespace("\\\nabc\\\\  "));
	}

	boost::test_tools::assertion_result test_compare_spliced_with_raw(
		std::string_view input, std::string_view sv, bool expected_result)
	{
		return clangd::compare_spliced_with_raw(input, sv) == expected_result;
	}

	BOOST_AUTO_TEST_CASE(compare_spliced_with_raw)
	{
		BOOST_TEST(test_compare_spliced_with_raw("", "", true));
		BOOST_TEST(test_compare_spliced_with_raw("\\\n", "", true));
		BOOST_TEST(test_compare_spliced_with_raw("\\ \n", "", true));
		BOOST_TEST(test_compare_spliced_with_raw("\\ \n\\\n", "", true));
		BOOST_TEST(test_compare_spliced_with_raw("\\\n\\ \n", "", true));
		BOOST_TEST(test_compare_spliced_with_raw("a\\\n\\ \nb", "ab", true));
		BOOST_TEST(test_compare_spliced_with_raw("\\\na\\\nb\\\nc\\\n", "abc", true));
		BOOST_TEST(test_compare_spliced_with_raw("\\ \na\\ \nb\\ \nc\\ \n", "abc", true));
		BOOST_TEST(test_compare_spliced_with_raw("\\ \na\\ \n b\\ \nc\\ \n", "abc", false));
		BOOST_TEST(test_compare_spliced_with_raw("\\ \na\\ \nb\\ \nc \\ \n", "abc", false));
	}

	BOOST_AUTO_TEST_CASE(spliced_text_iterator)
	{
		clangd::spliced_text_iterator it("\\\na\\\nbcd\\ \nef");
		// extra parentheses required because otherwise , is used to separate macro arguments
		BOOST_TEST(it.position() == (text::position{1, 0}));
		BOOST_TEST(*it == 'a');
		const auto it_at_a = it;
		++it;
		BOOST_TEST(it.position() == (text::position{2, 0}));
		BOOST_TEST(*it == 'b');
		++it;
		BOOST_TEST(it.position() == (text::position{2, 1}));
		BOOST_TEST(*it == 'c');
		++it;
		BOOST_TEST(it.position() == (text::position{2, 2}));
		BOOST_TEST(*it == 'd');
		++it;
		BOOST_TEST(it.position() == (text::position{3, 0}));
		BOOST_TEST(*it == 'e');
		++it;
		BOOST_TEST(it.position() == (text::position{3, 1}));
		BOOST_TEST(*it == 'f');
		const auto it_at_f = it;
		++it;
		BOOST_TEST(it.position() == (text::position{3, 2}));
		BOOST_TEST((it == clangd::spliced_text_iterator{}));
		BOOST_TEST(clangd::str_from_range(it_at_a, it_at_f) == "a\\\nbcd\\ \ne");
	}

	BOOST_AUTO_TEST_CASE(spliced_text_parser)
	{
		clangd::spliced_text_parser parser(
			"\\ \n"
			"#include <iostream>\n"
			"/* some TODO comment */\n"
			"int main()\n"
			"{\n"
			"\tint i = 0xdeadbeef;\n"
			"\tdouble d = 0x1.4p+3;\n"
			"\tauto c = u8'\\xff';\n"
			"}\n"
		);

		BOOST_TEST(parser.current_position() == (text::position{1, 0}));

		text::fragment frag = parser.parse_exactly('$');
		BOOST_TEST(parser.current_position() == (text::position{1, 0}));
		BOOST_TEST(frag.empty());
		BOOST_TEST(frag.str.empty());
		BOOST_TEST(frag.r.empty());

		frag = parser.parse_exactly('#');
		BOOST_TEST(parser.current_position() == (text::position{1, 1}));
		BOOST_TEST(frag.str == "#");
		BOOST_TEST(frag.r.first == (text::position{1, 0}));
		BOOST_TEST(frag.r.last  == (text::position{1, 1}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{1, 8}));
		BOOST_TEST(frag.str == "include");
		BOOST_TEST(frag.r.first == (text::position{1, 1}));
		BOOST_TEST(frag.r.last  == (text::position{1, 8}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{1, 9}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{1, 8}));
		BOOST_TEST(frag.r.last  == (text::position{1, 9}));

		frag = parser.parse_quoted('<', '>');
		BOOST_TEST(parser.current_position() == (text::position{1, 19}));
		BOOST_TEST(frag.str == "<iostream>");
		BOOST_TEST(frag.r.first == (text::position{1, 9}));
		BOOST_TEST(frag.r.last  == (text::position{1, 19}));

		frag = parser.parse_newline();
		BOOST_TEST(parser.current_position() == (text::position{2, 0}));
		BOOST_TEST(frag.str == "\n");
		BOOST_TEST(frag.r.first == (text::position{1, 19}));
		BOOST_TEST(frag.r.last  == (text::position{2, 0}));

		frag = parser.parse_exactly("/*");
		BOOST_TEST(parser.current_position() == (text::position{2, 2}));
		BOOST_TEST(frag.str == "/*");
		BOOST_TEST(frag.r.first == (text::position{2, 0}));
		BOOST_TEST(frag.r.last  == (text::position{2, 2}));

		frag = parser.parse_comment_multi_body();
		BOOST_TEST(parser.current_position() == (text::position{2, 8}));
		BOOST_TEST(frag.str == " some ");
		BOOST_TEST(frag.r.first == (text::position{2, 2}));
		BOOST_TEST(frag.r.last  == (text::position{2, 8}));

		frag = parser.parse_comment_tag_todo();
		BOOST_TEST(parser.current_position() == (text::position{2, 12}));
		BOOST_TEST(frag.str == "TODO");
		BOOST_TEST(frag.r.first == (text::position{2, 8}));
		BOOST_TEST(frag.r.last  == (text::position{2, 12}));

		frag = parser.parse_comment_multi_body();
		BOOST_TEST(parser.current_position() == (text::position{2, 21}));
		BOOST_TEST(frag.str == " comment ");
		BOOST_TEST(frag.r.first == (text::position{2, 12}));
		BOOST_TEST(frag.r.last  == (text::position{2, 21}));

		frag = parser.parse_exactly("*/");
		BOOST_TEST(parser.current_position() == (text::position{2, 23}));
		BOOST_TEST(frag.str == "*/");
		BOOST_TEST(frag.r.first == (text::position{2, 21}));
		BOOST_TEST(frag.r.last  == (text::position{2, 23}));

		frag = parser.parse_newline();
		BOOST_TEST(parser.current_position() == (text::position{3, 0}));
		BOOST_TEST(frag.str == "\n");
		BOOST_TEST(frag.r.first == (text::position{2, 23}));
		BOOST_TEST(frag.r.last  == (text::position{3, 0}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{3, 3}));
		BOOST_TEST(frag.str == "int");
		BOOST_TEST(frag.r.first == (text::position{3, 0}));
		BOOST_TEST(frag.r.last  == (text::position{3, 3}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{3, 4}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{3, 3}));
		BOOST_TEST(frag.r.last  == (text::position{3, 4}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{3, 8}));
		BOOST_TEST(frag.str == "main");
		BOOST_TEST(frag.r.first == (text::position{3, 4}));
		BOOST_TEST(frag.r.last  == (text::position{3, 8}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{3, 10}));
		BOOST_TEST(frag.str == "()");
		BOOST_TEST(frag.r.first == (text::position{3, 8}));
		BOOST_TEST(frag.r.last  == (text::position{3, 10}));

		frag = parser.parse_newline();
		BOOST_TEST(parser.current_position() == (text::position{4, 0}));
		BOOST_TEST(frag.str == "\n");
		BOOST_TEST(frag.r.first == (text::position{3, 10}));
		BOOST_TEST(frag.r.last  == (text::position{4, 0}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{4, 1}));
		BOOST_TEST(frag.str == "{");
		BOOST_TEST(frag.r.first == (text::position{4, 0}));
		BOOST_TEST(frag.r.last  == (text::position{4, 1}));

		frag = parser.parse_newline();
		BOOST_TEST(parser.current_position() == (text::position{5, 0}));
		BOOST_TEST(frag.str == "\n");
		BOOST_TEST(frag.r.first == (text::position{4, 1}));
		BOOST_TEST(frag.r.last  == (text::position{5, 0}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{5, 1}));
		BOOST_TEST(frag.str == "\t");
		BOOST_TEST(frag.r.first == (text::position{5, 0}));
		BOOST_TEST(frag.r.last  == (text::position{5, 1}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{5, 4}));
		BOOST_TEST(frag.str == "int");
		BOOST_TEST(frag.r.first == (text::position{5, 1}));
		BOOST_TEST(frag.r.last  == (text::position{5, 4}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{5, 5}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{5, 4}));
		BOOST_TEST(frag.r.last  == (text::position{5, 5}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{5, 6}));
		BOOST_TEST(frag.str == "i");
		BOOST_TEST(frag.r.first == (text::position{5, 5}));
		BOOST_TEST(frag.r.last  == (text::position{5, 6}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{5, 7}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{5, 6}));
		BOOST_TEST(frag.r.last  == (text::position{5, 7}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{5, 8}));
		BOOST_TEST(frag.str == "=");
		BOOST_TEST(frag.r.first == (text::position{5, 7}));
		BOOST_TEST(frag.r.last  == (text::position{5, 8}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{5, 9}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{5, 8}));
		BOOST_TEST(frag.r.last  == (text::position{5, 9}));

		frag = parser.parse_numeric_literal();
		BOOST_TEST(parser.current_position() == (text::position{5, 19}));
		BOOST_TEST(frag.str == "0xdeadbeef");
		BOOST_TEST(frag.r.first == (text::position{5, 9}));
		BOOST_TEST(frag.r.last  == (text::position{5, 19}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{5, 20}));
		BOOST_TEST(frag.str == ";");
		BOOST_TEST(frag.r.first == (text::position{5, 19}));
		BOOST_TEST(frag.r.last  == (text::position{5, 20}));

		frag = parser.parse_newline();
		BOOST_TEST(parser.current_position() == (text::position{6, 0}));
		BOOST_TEST(frag.str == "\n");
		BOOST_TEST(frag.r.first == (text::position{5, 20}));
		BOOST_TEST(frag.r.last  == (text::position{6, 0}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{6, 1}));
		BOOST_TEST(frag.str == "\t");
		BOOST_TEST(frag.r.first == (text::position{6, 0}));
		BOOST_TEST(frag.r.last  == (text::position{6, 1}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{6, 7}));
		BOOST_TEST(frag.str == "double");
		BOOST_TEST(frag.r.first == (text::position{6, 1}));
		BOOST_TEST(frag.r.last  == (text::position{6, 7}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{6, 8}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{6, 7}));
		BOOST_TEST(frag.r.last  == (text::position{6, 8}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{6, 9}));
		BOOST_TEST(frag.str == "d");
		BOOST_TEST(frag.r.first == (text::position{6, 8}));
		BOOST_TEST(frag.r.last  == (text::position{6, 9}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{6, 10}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{6, 9}));
		BOOST_TEST(frag.r.last  == (text::position{6, 10}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{6, 11}));
		BOOST_TEST(frag.str == "=");
		BOOST_TEST(frag.r.first == (text::position{6, 10}));
		BOOST_TEST(frag.r.last  == (text::position{6, 11}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{6, 12}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{6, 11}));
		BOOST_TEST(frag.r.last  == (text::position{6, 12}));

		frag = parser.parse_numeric_literal();
		BOOST_TEST(parser.current_position() == (text::position{6, 20}));
		BOOST_TEST(frag.str == "0x1.4p+3");
		BOOST_TEST(frag.r.first == (text::position{6, 12}));
		BOOST_TEST(frag.r.last  == (text::position{6, 20}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{6, 21}));
		BOOST_TEST(frag.str == ";");
		BOOST_TEST(frag.r.first == (text::position{6, 20}));
		BOOST_TEST(frag.r.last  == (text::position{6, 21}));

		frag = parser.parse_newline();
		BOOST_TEST(parser.current_position() == (text::position{7, 0}));
		BOOST_TEST(frag.str == "\n");
		BOOST_TEST(frag.r.first == (text::position{6, 21}));
		BOOST_TEST(frag.r.last  == (text::position{7, 0}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{7, 1}));
		BOOST_TEST(frag.str == "\t");
		BOOST_TEST(frag.r.first == (text::position{7, 0}));
		BOOST_TEST(frag.r.last  == (text::position{7, 1}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{7, 5}));
		BOOST_TEST(frag.str == "auto");
		BOOST_TEST(frag.r.first == (text::position{7, 1}));
		BOOST_TEST(frag.r.last  == (text::position{7, 5}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{7, 6}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{7, 5}));
		BOOST_TEST(frag.r.last  == (text::position{7, 6}));

		frag = parser.parse_identifier();
		BOOST_TEST(parser.current_position() == (text::position{7, 7}));
		BOOST_TEST(frag.str == "c");
		BOOST_TEST(frag.r.first == (text::position{7, 6}));
		BOOST_TEST(frag.r.last  == (text::position{7, 7}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{7, 8}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{7, 7}));
		BOOST_TEST(frag.r.last  == (text::position{7, 8}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{7, 9}));
		BOOST_TEST(frag.str == "=");
		BOOST_TEST(frag.r.first == (text::position{7, 8}));
		BOOST_TEST(frag.r.last  == (text::position{7, 9}));

		frag = parser.parse_non_newline_whitespace();
		BOOST_TEST(parser.current_position() == (text::position{7, 10}));
		BOOST_TEST(frag.str == " ");
		BOOST_TEST(frag.r.first == (text::position{7, 9}));
		BOOST_TEST(frag.r.last  == (text::position{7, 10}));

		frag = parser.parse_text_literal_prefix('\'');
		BOOST_TEST(parser.current_position() == (text::position{7, 12}));
		BOOST_TEST(frag.str == "u8");
		BOOST_TEST(frag.r.first == (text::position{7, 10}));
		BOOST_TEST(frag.r.last  == (text::position{7, 12}));

		frag = parser.parse_exactly('\'');
		BOOST_TEST(parser.current_position() == (text::position{7, 13}));
		BOOST_TEST(frag.str == "'");
		BOOST_TEST(frag.r.first == (text::position{7, 12}));
		BOOST_TEST(frag.r.last  == (text::position{7, 13}));

		frag = parser.parse_escape_sequence();
		BOOST_TEST(parser.current_position() == (text::position{7, 17}));
		BOOST_TEST(frag.str == "\\xff");
		BOOST_TEST(frag.r.first == (text::position{7, 13}));
		BOOST_TEST(frag.r.last  == (text::position{7, 17}));

		frag = parser.parse_exactly('\'');
		BOOST_TEST(parser.current_position() == (text::position{7, 18}));
		BOOST_TEST(frag.str == "'");
		BOOST_TEST(frag.r.first == (text::position{7, 17}));
		BOOST_TEST(frag.r.last  == (text::position{7, 18}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{7, 19}));
		BOOST_TEST(frag.str == ";");
		BOOST_TEST(frag.r.first == (text::position{7, 18}));
		BOOST_TEST(frag.r.last  == (text::position{7, 19}));

		frag = parser.parse_newline();
		BOOST_TEST(parser.current_position() == (text::position{8, 0}));
		BOOST_TEST(frag.str == "\n");
		BOOST_TEST(frag.r.first == (text::position{7, 19}));
		BOOST_TEST(frag.r.last  == (text::position{8, 0}));

		frag = parser.parse_symbols();
		BOOST_TEST(parser.current_position() == (text::position{8, 1}));
		BOOST_TEST(frag.str == "}");
		BOOST_TEST(frag.r.first == (text::position{8, 0}));
		BOOST_TEST(frag.r.last  == (text::position{8, 1}));

		frag = parser.parse_newline();
		BOOST_TEST(parser.current_position() == (text::position{9, 0}));
		BOOST_TEST(frag.str == "\n");
		BOOST_TEST(frag.r.first == (text::position{8, 1}));
		BOOST_TEST(frag.r.last  == (text::position{9, 0}));

		BOOST_TEST(parser.has_reached_end());
	}

BOOST_AUTO_TEST_SUITE_END()
