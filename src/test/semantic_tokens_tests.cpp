#include <ach/clangd/core.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/code_tokenizer.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/spliced_text_iterator.hpp>
#include <ach/text/types.hpp>
#include <ach/text/utils.hpp>

#include "clangd_common.hpp"

#include <boost/test/tools/assertion_result.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <string_view>

namespace ach::clangd {

BOOST_AUTO_TEST_SUITE(semantic_tokens_suite)

class search_state
{
public:
	search_state(std::string_view input)
	: first(input)
	{}

	text::fragment result() const
	{
		const auto distance = last.to_text_iterator().pointer() - first.to_text_iterator().pointer();
		return {
			first == last ? std::string_view() : std::string_view(first.to_text_iterator().pointer(), distance),
			{first.position(), last.position()}
		};
	}

	text::range next_search(std::string_view frag)
	{
		const auto [start, stop] = std::default_searcher<const char*>(frag.data(), frag.data() + frag.size())(first, last);
		first = stop;
		return {start.position(), stop.position()};
	}

private:
	spliced_text_iterator first;
	spliced_text_iterator last;
};

struct expected_token
{
	syntax_element_type syntax_element;
	semantic_token_info info;
	std::string_view where;
};

[[nodiscard]] boost::test_tools::assertion_result test_semantic_tokens_impl(
	std::string_view code,
	const std::vector<semantic_token>& sem_tokens,
	const std::vector<expected_token>& expected_tokens)
{
	// tokenization - should always succeed
	std::vector<code_token> code_tokens;
	if (!fill_with_tokens(code, code_tokens))
		return false;

	auto maybe_error = improve_code_tokens(code, code_tokens, {sem_tokens.data(), sem_tokens.data() + sem_tokens.size()});
	if (maybe_error) {
		boost::test_tools::assertion_result result = false;
		result.message().stream() << "test bug - token improving failed: " << *maybe_error;
		return result;
	}

	search_state ss(code);
	auto it = code_tokens.begin();
	for (expected_token token : expected_tokens) {
		text::range expected_range = ss.next_search(token.where);

		it = std::find_if(it, code_tokens.end(), [expected_range](code_token ct) { return ct.origin.r == expected_range; });

		if (it == code_tokens.end()) {
			boost::test_tools::assertion_result result = false;
			auto& stream = result.message().stream();
			stream << "test error: could not find token " << text::sanitized_whitespace(token.where) << " assumed to be at " << expected_range << "\n";
			print_code_tokens(stream, code_tokens);
			return result;
		}

		const code_token& ct = *it;
		if (ct.syntax_element != token.syntax_element || ct.semantic_info != token.info) {
			boost::test_tools::assertion_result result = false;
			result.message().stream() << "test error on token " << text::sanitized_whitespace(token.where) << " at " << expected_range << ":\n"
				"EXPECTED:\n" << token.syntax_element << " | " << token.info << "\n"
				"ACTUAL:\n" << ct.syntax_element << " | " << ct.semantic_info << "\n";
			return result;
		}
	}

	return true;
}

void test_semantic_tokens(
	std::string_view code,
	const std::vector<semantic_token>& sem_tokens,
	const std::vector<expected_token>& expected_tokens)
{
	BOOST_TEST(test_semantic_tokens_impl(code, sem_tokens, expected_tokens));
}

BOOST_AUTO_TEST_CASE(empty_input)
{
	test_semantic_tokens("", {}, {});
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

	semantic_token_info sti_br{semantic_token_type::bracket, semantic_token_modifiers{}};
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
	semantic_token_info sti_n2{semantic_token_type::parameter, semantic_token_modifiers{}.scope_function().non_const_ref_parameter()};

	std::vector<semantic_token> tokens = {
		semantic_token{{0, 19}, 1u, sti_t1, {}},
		semantic_token{{1,  0}, 3u, sti_bar, {}},
		semantic_token{{1,  4}, 3u, sti_foo, {}},
		semantic_token{{1,  8}, 1u, sti_t2, {}},
		semantic_token{{1, 12}, 1u, sti_f, {}},
		semantic_token{{1, 19}, 1u, sti_n1, {}},
		semantic_token{{3,  1}, 4u, sti_auto, {}},
		semantic_token{{3,  6}, 1u, sti_k1, {}},
		semantic_token{{4,  8}, 1u, sti_h, {}},
		semantic_token{{4, 10}, 1u, sti_t3, {}},
		semantic_token{{4, 13}, 1u, sti_v, {}},
		semantic_token{{4, 16}, 1u, sti_g, {}},
		semantic_token{{4, 19}, 1u, sti_k2, {}},
		semantic_token{{4, 22}, 1u, sti_m, {}},
		semantic_token{{4, 25}, 1u, sti_n2, {}},
	};

	std::vector<expected_token> expected_tokens = {
		expected_token{syntax_element_type::keyword, {}, std::string_view("template")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("<")},
		expected_token{syntax_element_type::keyword, {}, std::string_view("typename")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_t1, std::string_view("T")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(">")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		expected_token{syntax_element_type::identifier, sti_bar, std::string_view("bar")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_foo, std::string_view("foo")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("<")},
		expected_token{syntax_element_type::identifier, sti_t2, std::string_view("T")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(">")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(":")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(":")},
		expected_token{syntax_element_type::identifier, sti_f, std::string_view("f")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("(")},
		expected_token{syntax_element_type::keyword, {}, std::string_view("int")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("&")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_n1, std::string_view("n")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(")")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::keyword, {}, std::string_view("const")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("{")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\t")},
		expected_token{syntax_element_type::keyword, sti_auto, std::string_view("auto")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_k1, std::string_view("k")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("=")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::literal_number, {}, std::string_view("1")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(";")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\t")},
		expected_token{syntax_element_type::keyword, {}, std::string_view("return")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_h, std::string_view("h")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("(")},
		expected_token{syntax_element_type::identifier, sti_t3, std::string_view("T")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(":")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(":")},
		expected_token{syntax_element_type::identifier, sti_v, std::string_view("v")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(",")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_g, std::string_view("g")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(",")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_k2, std::string_view("k")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(",")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_m, std::string_view("m")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(",")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_n2, std::string_view("n")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(")")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(";")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("}")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		expected_token{syntax_element_type::end_of_input, {}, std::string_view()}
	};

	test_semantic_tokens(input, tokens, expected_tokens);

	// Somewhere between clangd versions 16-20 it started to report "bracket" token.
	// Test that the implementation supports both old and new versions.
	tokens.insert(tokens.begin(),     semantic_token{{0,  9}, 1, sti_br, {}});
	tokens.insert(tokens.begin() + 2, semantic_token{{0, 20}, 1, sti_br, {}});
	expected_tokens[2].info = sti_br;
	expected_tokens[6].info = sti_br;
	// (no change in test tokens because bracket tokens should be ignored)
	test_semantic_tokens(input, tokens, expected_tokens);
}

BOOST_AUTO_TEST_CASE(semantic_tokens_complex_string_literal)
{
	const auto input =
		"#include <string_view>\n"
		"\n"
		"using namespace std::literals;\n"
		"\n"
		"[[maybe_unused]] const auto sv = LR\"({\"key\": \"value\"})\"sv;\n";

	semantic_token_info sti_ns{semantic_token_type::namespace_, semantic_token_modifiers{}.from_std_lib().scope_global()};
	semantic_token_info sti_cl{semantic_token_type::class_, semantic_token_modifiers{}.deduced().from_std_lib().scope_global()};
	semantic_token_info sti_var{semantic_token_type::variable, semantic_token_modifiers{}.declaration().definition().readonly().scope_file()};

	std::vector<semantic_token> tokens = {
		semantic_token{{2, 16}, 3u, sti_ns, {}},
		semantic_token{{2, 21}, 8u, sti_ns, {}},
		semantic_token{{4, 23}, 4u, sti_cl, {}},
		semantic_token{{4, 28}, 2u, sti_var, {}},
	};

	std::vector<expected_token> expected_tokens = {
		expected_token{syntax_element_type::preprocessor_hash, {}, std::string_view("#")},
		expected_token{syntax_element_type::preprocessor_directive, {}, std::string_view("include")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::preprocessor_header_file, {}, std::string_view("<string_view>")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n\n")},

		expected_token{syntax_element_type::keyword, {}, std::string_view("using")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::keyword, {}, std::string_view("namespace")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_ns, std::string_view("std")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(":")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(":")},
		expected_token{syntax_element_type::identifier, sti_ns, std::string_view("literals")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(";")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n\n")},

		expected_token{syntax_element_type::symbol, {}, std::string_view("[")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("[")},
		expected_token{syntax_element_type::identifier, {}, std::string_view("maybe_unused")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("]")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("]")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::keyword, {}, std::string_view("const")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::keyword, sti_cl, std::string_view("auto")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::identifier, sti_var, std::string_view("sv")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::symbol, {}, std::string_view("=")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::literal_prefix, {}, std::string_view("LR")},
		expected_token{syntax_element_type::literal_string_raw_quote, {}, std::string_view("\"")},
		expected_token{syntax_element_type::literal_string_raw_paren, {}, std::string_view("(")},
		expected_token{syntax_element_type::literal_string, {}, std::string_view("{\"key\": \"value\"}")},
		expected_token{syntax_element_type::literal_string_raw_paren, {}, std::string_view(")")},
		expected_token{syntax_element_type::literal_string_raw_quote, {}, std::string_view("\"")},
		expected_token{syntax_element_type::literal_suffix, {}, std::string_view("sv")},
		expected_token{syntax_element_type::symbol, {}, std::string_view(";")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},

		expected_token{syntax_element_type::end_of_input, {}, std::string_view()}
	};

	test_semantic_tokens(input, tokens, expected_tokens);
}

BOOST_AUTO_TEST_CASE(semantic_tokens_preprocessor_disabled_code_and_macro_token)
{
	std::string_view input =
		// Somewhere between clangd versions 16-20 it started to report "MACRO" token.
		// Test that the implementation supports both old and new versions.
		"#ifdef MACRO\n"
		"#include <header.h>\n"
		"#endif\n";

	// newer clangd versions report "MACRO"
	semantic_token_info sti_macro{semantic_token_type::macro, semantic_token_modifiers{}};
	semantic_token_info sti_dc   {semantic_token_type::disabled_code, semantic_token_modifiers{}};

	std::vector<expected_token> test_tokens = {
		// line 1
		expected_token{syntax_element_type::preprocessor_hash, {}, std::string_view("#")},
		expected_token{syntax_element_type::preprocessor_directive, {}, std::string_view("ifdef")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view(" ")},
		expected_token{syntax_element_type::preprocessor_macro, {}, std::string_view("MACRO")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		// line 2
		expected_token{syntax_element_type::preprocessor_hash, sti_dc, std::string_view("#")},
		expected_token{syntax_element_type::preprocessor_directive, sti_dc, std::string_view("include")},
		expected_token{syntax_element_type::whitespace, sti_dc, std::string_view(" ")},
		expected_token{syntax_element_type::preprocessor_header_file, sti_dc, std::string_view("<header.h>")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		// line 3
		expected_token{syntax_element_type::preprocessor_hash, {}, std::string_view("#")},
		expected_token{syntax_element_type::preprocessor_directive, {}, std::string_view("endif")},
		expected_token{syntax_element_type::whitespace, {}, std::string_view("\n")},
		expected_token{syntax_element_type::end_of_input, {}, std::string_view()},
	};

	// old ("MACRO" is a syntax token, not reported by clangd)
	test_semantic_tokens(
		input, {
			semantic_token{{1, 0}, 19, sti_dc, {}},
		},
		test_tokens
	);

	// new ("MACRO" is an identifier token)
	test_tokens[3] = expected_token{syntax_element_type::preprocessor_macro, sti_macro, std::string_view("MACRO")};
	test_semantic_tokens(
		input, {
			semantic_token{{0, 7}, 5, sti_macro, {}},
			semantic_token{{1, 0}, 19, sti_dc, {}},
		},
		test_tokens
	);
}

BOOST_AUTO_TEST_SUITE_END()

}
