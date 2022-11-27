#include <ach/clangd/code_token.hpp>
#include <ach/clangd/code_tokenizer.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/clangd/semantic_token.hpp>
#include <ach/text/types.hpp>

#include <boost/test/tools/assertion_result.hpp>
#include <boost/test/unit_test.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace ach;
using namespace ach::clangd;

const std::initializer_list<std::string> keywords = {
	"__restrict",
	"abstract",
	"alignas",
	"alignof",
	"and",
	"and_eq",
	"asm",
	"atomic_cancel",
	"atomic_commit",
	"atomic_noexcept",
	"auto",
	"bitand",
	"bitor",
	"bool",
	"break",
	"case",
	"catch",
	"char",
	"char8_t",
	"char16_t",
	"char32_t",
	"class",
	"compl",
	"concept",
	"const",
	"consteval",
	"constexpr",
	"constinit",
	"const_cast",
	"continue",
	"co_await",
	"co_return",
	"co_yield",
	"decltype",
	"default",
	"delete",
	"do",
	"double",
	"dynamic_cast",
	"else",
	"enum",
	"explicit",
	"export",
	"extern",
	"false",
	"float",
	"for",
	"friend",
	"goto",
	"if",
	"inline",
	"int",
	"interface",
	"let",
	"long",
	"mutable",
	"namespace",
	"new",
	"noexcept",
	"not",
	"not_eq",
	"nullptr",
	"operator",
	"or",
	"or_eq",
	"private",
	"protected",
	"public",
	"reflexpr",
	"register",
	"reinterpret_cast",
	"requires",
	"restrict",
	"return",
	"short",
	"signed",
	"sizeof",
	"static",
	"static_assert",
	"static_cast",
	"struct",
	"switch",
	"synchronized",
	"template",
	"this",
	"thread_local",
	"throw",
	"true",
	"try",
	"typedef",
	"typeid",
	"typename",
	"typeof",
	"union",
	"unsigned",
	"using",
	"var",
	"virtual",
	"void",
	"volatile",
	"wchar_t",
	"while",
	"with",
	"xor",
	"xor_eq"
};

BOOST_AUTO_TEST_SUITE(code_tokenizer_suite)

	code_tokenizer make_code_tokenizer(std::string_view input, utility::range<const semantic_token*> tokens)
	{
		auto result = code_tokenizer::create(input, tokens);
		BOOST_TEST_REQUIRE(result.has_value());
		return *std::move(result);
	}

	boost::test_tools::assertion_result compare_tokens(code_token expected, code_token actual)
	{
		BOOST_TEST(expected.origin == actual.origin);

		if (expected.token_type != actual.token_type) {
			boost::test_tools::assertion_result result = false;
			result.message().stream() << "tokens differ\nEXPECTED:\n" << expected << "ACTUAL:\n" << actual;
			return result;
		}

		return true;
	}

	boost::test_tools::assertion_result test_code_tokenizer(
		std::string_view input,
		const std::vector<semantic_token>& sem_tokens,
		const std::vector<code_token>& expected_code_tokens)
	{
		auto tokenizer = make_code_tokenizer(input, {sem_tokens.data(), sem_tokens.data() + sem_tokens.size()});

		for (std::size_t i = 0; i < expected_code_tokens.size(); ++i) {
			std::variant<code_token, highlighter_error> token_or_error = tokenizer.next_code_token(
				{keywords.begin(), keywords.begin() + keywords.size()});

			if (std::holds_alternative<highlighter_error>(token_or_error)) {
				boost::test_tools::assertion_result result = false;
				result.message().stream() << "unexpected error on iteration " << i
					<< ": " << std::get<highlighter_error>(token_or_error);
				return result;
			}

			BOOST_TEST(compare_tokens(expected_code_tokens[i], std::get<code_token>(token_or_error)));
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

	BOOST_AUTO_TEST_CASE(empty_input)
	{
		test_code_tokenizer("", {}, {
			code_token{syntax_token::end_of_input, {std::string_view(), text::range{}}}}
		);
	}

BOOST_AUTO_TEST_SUITE_END()
