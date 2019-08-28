#pragma once

#include <ach/text_extractor.hpp>
#include <ach/detail/color_token.hpp>

namespace ach::detail {

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

}
