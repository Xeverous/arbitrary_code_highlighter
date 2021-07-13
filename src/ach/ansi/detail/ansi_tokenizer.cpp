#include <ach/ansi/detail/ansi_tokenizer.hpp>

namespace ach::ansi::detail {

namespace {

const char escape_start = 0x1b; // ESC ^[

}

token_t ansi_tokenizer::next_token()
{
	std::optional<char> c = _extractor.peek_next_char();
	if (!c) {
		if (!_extractor.load_next_line())
			return end_of_input_token{};

		c = _extractor.peek_next_char();
		if (!c)
			return end_of_input_token{};
	}

	char const next_char = *c;

	// TODO...
}

}
