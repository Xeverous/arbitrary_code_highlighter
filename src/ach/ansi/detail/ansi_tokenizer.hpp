#pragma once

#include <ach/common/text_extractor.hpp>

#include <string_view>
#include <variant>

namespace ach::ansi::detail {

struct text_token
{
	text_location location;
};

struct formatting_token
{
	// ...
};

struct end_of_input_token {};

struct error_token
{
	text_location location;
	std::string_view reason;
};

using token_t = std::variant<text_token, formatting_token, end_of_input_token, error_token>;

class ansi_tokenizer
{
public:
	ansi_tokenizer(std::string_view text)
	: _extractor(text) {}

	token_t next_token();

	[[nodiscard]] bool has_reached_end() const noexcept { return _extractor.has_reached_end(); }

private:
	text_extractor _extractor;
};

}
