#pragma once

#include "color_token.hpp"
#include "text_extractor.hpp"

class color_tokenizer
{
public:
	color_tokenizer(std::string_view text)
	: extractor(text) {}

	color_token next_token();

	[[nodiscard]] bool has_reached_end() const noexcept { return extractor.has_reached_end(); }

private:
	text_extractor extractor;
};
