#include <ach/clangd/code_token.hpp>
#include <ach/clangd/code_tokenizer.hpp>

#include <boost/test/tools/assertion_result.hpp>
#include <boost/test/unit_test.hpp>

#include <initializer_list>
#include <ostream>
#include <string>
#include <optional>
#include <vector>

namespace ach::clangd {

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

[[nodiscard]] inline boost::test_tools::assertion_result
fill_with_tokens(std::string_view code, std::vector<code_token>& code_tokens)
{
	std::optional<highlighter_error> maybe_error =
		code_tokenizer(code, {keywords.begin(), keywords.end()})
		.fill_with_tokens(true, code_tokens);

	if (maybe_error) {
		boost::test_tools::assertion_result result = false;
		result.message().stream() << "test bug - unexpected tokenization failure: " << *maybe_error;
		return result;
	}

	return true;
}

inline void print_code_tokens(std::ostream& os, const std::vector<code_token>& code_tokens)
{
	os << "CODE TOKENS:\n";
	int i = 0;
	for (code_token token : code_tokens)
		os << "[" << std::setw(2) << i++ << "]: " << token;

}

}
