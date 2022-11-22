#include <ach/clangd/code_tokenizer.hpp>

#include <boost/test/unit_test.hpp>

using namespace ach;

BOOST_AUTO_TEST_SUITE(code_tokenizer_suite)

	clangd::code_tokenizer make_code_tokenizer(std::string_view input, utility::range<const clangd::semantic_token*> tokens)
	{
		auto result = clangd::code_tokenizer::create(input, tokens);
		BOOST_TEST_REQUIRE(result.has_value());
		return *std::move(result);
	}

	BOOST_AUTO_TEST_CASE(empty_input)
	{
		auto tokenizer = make_code_tokenizer("", {nullptr, nullptr});
		auto token_or_error = tokenizer.next_code_token({nullptr, nullptr});
		BOOST_TEST_REQUIRE(std::holds_alternative<clangd::code_token>(token_or_error));
		const auto& token = std::get<clangd::code_token>(token_or_error);
		BOOST_TEST_REQUIRE(std::holds_alternative<clangd::syntax_token>(token.token_type));
		const auto& token_type = std::get<clangd::syntax_token>(token.token_type);
		BOOST_TEST((token_type == clangd::syntax_token::end_of_input));
		BOOST_TEST(tokenizer.has_reached_end());
	}

BOOST_AUTO_TEST_SUITE_END()
