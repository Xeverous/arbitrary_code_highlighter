#include <ach/clangd/core.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/code_tokenizer.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/spliced_text_iterator.hpp>
#include <ach/text/types.hpp>

#include "clangd_common.hpp"

#include <boost/test/tools/assertion_result.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <ostream>
#include <string_view>
#include <vector>
#include <tuple>
#include <functional>

namespace ach::clangd {

BOOST_AUTO_TEST_SUITE(find_matching_tokens_suite)

class search_state
{
public:
	search_state(std::string_view input, std::string_view pattern)
	: searcher(pattern.data(), pattern.data() + pattern.size())
	, first(input)
	{
		next();
	}

	text::fragment result() const
	{
		const auto distance = last.to_text_iterator().pointer() - first.to_text_iterator().pointer();
		return {
			first == last ? std::string_view() : std::string_view(first.to_text_iterator().pointer(), distance),
			{first.position(), last.position()}
		};
	}

	search_state& next()
	{
		std::tie(first, last) = searcher(first, last);
		return *this;
	}

private:
	std::default_searcher<const char*> searcher;
	spliced_text_iterator first;
	spliced_text_iterator last;
};

[[nodiscard]] search_state search(std::string_view input, std::string_view pattern)
{
	BOOST_TEST_REQUIRE(!input.empty(), "test is written incorrectly, input is empty");
	BOOST_TEST_REQUIRE(!pattern.empty(), "test is written incorrectly, pattern is empty");

	auto ss = search_state(input, pattern);
	BOOST_TEST_REQUIRE(!ss.result().empty(),
		"test is written incorrectly, search within tested input code failed\n"
		"input:\n" << input << "\npattern:\n" << pattern);
	return ss;
}

// Instead of supplying idx + length (which can be very fragile),
// supply fragment and syntax type, which will be used to automatically find starting token.
// This way each test can supply text-search expectation instead of hardcoding array indexes.
struct code_token_match
{
	text::fragment first_token_origin;
	syntax_element_type first_token_syntax_element;
	int match_length;
};

std::ostream& operator<<(std::ostream& os, code_token_match match)
{
	return os << match.first_token_origin << " | " << match.first_token_syntax_element
		<< " | length: " << match.match_length << "\n";
}

bool is_match_correct(utility::range<code_token*> result, code_token_match expected)
{
	if (result.size() != expected.match_length)
		return false;

	if (result.size() == 0)
		return true;

	// compare only starting position and type - the token might be split into more
	return result.first->origin.r.first == expected.first_token_origin.r.first
		&& result.first->syntax_element == expected.first_token_syntax_element;
}

[[nodiscard]] boost::test_tools::assertion_result test_find_matching_tokens_impl(
	std::string_view code,
	const std::vector<semantic_token>& sem_tokens,
	code_token_match expected_result)
{
	// tokenization - should always succeed
	std::vector<code_token> code_tokens;
	if (!fill_with_tokens(code, code_tokens))
		return false;

	// find index of expected token - should always succeed
	const auto it = std::find_if(code_tokens.begin(), code_tokens.end(), [&](code_token token) {
		// compare only starting position and type - the token might be split into more
		return token.origin.r.first == expected_result.first_token_origin.r.first
			&& token.syntax_element == expected_result.first_token_syntax_element;
	});
	if (it == code_tokens.end()) {
		boost::test_tools::assertion_result result = false;
		auto& stream = result.message().stream();
		stream << "test bug - unexpected failure when searching code tokens for:\n"
			<< expected_result;
		print_code_tokens(stream, code_tokens);
		return result;
	}

	if (sem_tokens.empty()) {
		boost::test_tools::assertion_result result = false;
		result.message().stream() << "test bug - semantic tokens are empty";
		return result;
	}

	// the tested function
	utility::range<code_token*> match_result = find_matching_tokens(
		{code_tokens.data(), code_tokens.data() + code_tokens.size()},
		sem_tokens.front().pos_begin(),
		sem_tokens.back().pos_end()
	);

	const auto expected_first_idx = it - code_tokens.begin();
	if (!is_match_correct(match_result, expected_result)) {
		boost::test_tools::assertion_result result = false;
		auto& stream = result.message().stream();

		stream << "found and expected tokens differ: \n"
			"EXPECTED:\n[" << std::setw(2) << expected_first_idx << "] length: " << expected_result.match_length << "\n"
			"ACTUAL:\n[" << std::setw(2) << match_result.first - code_tokens.data() << "] length: " << match_result.size() << "\n"
			"CODE TOKENS:\n";
		int i = 0;
		for (code_token token : code_tokens)
			stream << "[" << std::setw(2) << i++ << "]: " << token;

		stream << "SEMANTIC TOKENS:\n";
		for (semantic_token token : sem_tokens)
			stream << token;

		return result;
	}

	return true;
}

void test_find_matching_tokens(
	std::string_view code,
	const std::vector<semantic_token>& sem_tokens,
	code_token_match expected_result)
{
	BOOST_TEST(test_find_matching_tokens_impl(code, sem_tokens, expected_result));
}

void test_find_matching_tokens_negative(
	std::string_view code,
	text::position start,
	text::position stop)
{
	std::vector<code_token> code_tokens;
	if (!fill_with_tokens(code, code_tokens)) {
		BOOST_ERROR("");
		return;
	}

	const auto result = find_matching_tokens({code_tokens.data(), code_tokens.data() + code_tokens.size()}, start, stop);
	BOOST_TEST(result.empty());
}

using sti = semantic_token_info;
using stt = semantic_token_type;
using stm = semantic_token_modifiers;

BOOST_AUTO_TEST_CASE(invalid_token_fully_inside)
{
	std::string_view input =
		"class base {};\n"
		"class derived : public base {};";
	test_find_matching_tokens_negative(input, {1, 8}, {1, 11});
}

BOOST_AUTO_TEST_CASE(simple_1_to_1_identifier)
{
	std::string_view input = "void func();";
	test_find_matching_tokens(
		input, {
			semantic_token{{0, 5}, 4, sti{stt::function, stm().declaration().scope_global()}, {}}
		},
		code_token_match{search(input, "func").result(), syntax_element_type::identifier, 1}
	);
}

BOOST_AUTO_TEST_CASE(simple_1_to_2_overloaded_operator)
{
	std::string_view input =
		"#include <string>\n"
		"\n"
		"bool f(const std::string& a, const std::string& b)\n"
		"{\n"
		"\treturn a == b;\n"
		"}\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{4, 10}, 2, sti{stt::operator_, stm().user_defined()}, {}}
		},
		code_token_match{search(input, "==").result(), syntax_element_type::symbol, 2}
	);
}

BOOST_AUTO_TEST_CASE(splice_2_to_1_identifier)
{
	std::string_view input =
		"void func\\\n"
		"tion();\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{0, 5}, 5, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{1, 0}, 4, sti{stt::function, stm().declaration().scope_global()}, {}}
		},
		code_token_match{search(input, "func").result(), syntax_element_type::identifier, 1}
	);
}

BOOST_AUTO_TEST_CASE(splice_4_to_1_identifier)
{
	std::string_view input =
		"void fu\\\n"
		"nc\\\n"
		"ti\\\n"
		"on();\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{0, 5}, 3, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{1, 0}, 3, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{2, 0}, 3, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{3, 0}, 2, sti{stt::function, stm().declaration().scope_global()}, {}}
		},
		code_token_match{search(input, "fu").result(), syntax_element_type::identifier, 1}
	);
}

BOOST_AUTO_TEST_CASE(splice_empty_to_1_identifier)
{
	std::string_view input =
		"void func\\\n"
		"\\\n"
		"\\\n"
		"\\\n"
		"tion();\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{0, 5}, 5, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{1, 0}, 1, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{2, 0}, 1, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{3, 0}, 1, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{4, 0}, 4, sti{stt::function, stm().declaration().scope_global()}, {}}
		},
		code_token_match{search(input, "func").result(), syntax_element_type::identifier, 1}
	);
}

BOOST_AUTO_TEST_CASE(splice_2_to_2_overloaded_operator)
{
	std::string_view input =
		"#include <string>\n"
		"\n"
		"bool f(const std::string& a, const std::string& b)\n"
		"{\n"
		"\treturn a =\\\n"
		"= b;\n"
		"}\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{4, 10}, 2, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{5,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}}
		},
		code_token_match{search(input, "=").result(), syntax_element_type::symbol, 2}
	);
}

BOOST_AUTO_TEST_CASE(splice_4_to_2_overloaded_operator)
{
	std::string_view input =
		"#include <string>\n"
		"\n"
		"bool f(const std::string& a, const std::string& b)\n"
		"{\n"
		"\treturn a =\\\n"
		"\\\n"
		"\\\n"
		"= b;\n"
		"}\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{4, 10}, 2, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{5,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{6,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{7,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}}
		},
		code_token_match{search(input, "=").result(), syntax_element_type::symbol, 2}
	);
}


BOOST_AUTO_TEST_CASE(enclosing_splice_3_to_1_identifier)
{
	std::string_view input =
		"void \\\n"
		"func\\\n"
		"tion\\\n"
		"();\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{0, 5}, 1, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{1, 0}, 5, sti{stt::function, stm().declaration().scope_global()}, {}},
			semantic_token{{2, 0}, 4, sti{stt::function, stm().declaration().scope_global()}, {}}
		},
		code_token_match{search(input, "func").result(), syntax_element_type::identifier, 1}
	);
}

BOOST_AUTO_TEST_CASE(enclosing_splice_7_to_2_operator)
{
	std::string_view input =
		"#include <string>\n"
		"\n"
		"bool f(const std::string& a, const std::string& b)\n"
		"{\n"
		"\treturn a \\\n"
		"\\\n"
		"\\\n"
		"=\\\n"
		"\\\n"
		"\\\n"
		"=\\\n"
		"\\\n"
		"\\\n"
		" b;\n"
		"}\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{ 4, 10}, 1, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{ 5,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{ 6,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{ 7,  0}, 2, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{ 8,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{ 9,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}},
			semantic_token{{10,  0}, 1, sti{stt::operator_, stm().user_defined()}, {}}
		},
		code_token_match{search(input, "=").result(), syntax_element_type::symbol, 2}
	);
}

BOOST_AUTO_TEST_CASE(disabled_code)
{
	std::string_view input =
		"#ifdef TEST\n"
		"int test();\n"
		"#include <test.h>\n"
		"int test()\n"
		"{\n"
		"\treturn \\\n"
		"TE\\\n"
		"ST\\\n"
		";\n"
		"}\n"
		"#endif\n"
		"void f();\n";
	test_find_matching_tokens(
		input, {
			semantic_token{{ 1, 0}, 11, sti{stt::disabled_code, stm()}, {}},
			semantic_token{{ 2, 0}, 17, sti{stt::disabled_code, stm()}, {}},
			semantic_token{{ 3, 0}, 10, sti{stt::disabled_code, stm()}, {}},
			semantic_token{{ 4, 0},  1, sti{stt::disabled_code, stm()}, {}},
			semantic_token{{ 5, 0},  9, sti{stt::disabled_code, stm()}, {}},
			semantic_token{{ 6, 0},  3, sti{stt::disabled_code, stm()}, {}},
			semantic_token{{ 7, 0},  3, sti{stt::disabled_code, stm()}, {}},
			semantic_token{{ 8, 0},  1, sti{stt::disabled_code, stm()}, {}},
			semantic_token{{ 9, 0},  1, sti{stt::disabled_code, stm()}, {}},
		},
		code_token_match{search(input, "int").result(), syntax_element_type::keyword, 27}
	);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace ach::clangd
