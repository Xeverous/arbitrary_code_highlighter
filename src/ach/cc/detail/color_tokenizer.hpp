#pragma once

#include <ach/common/text_extractor.hpp>
#include <ach/cc/color_options.hpp>
#include <ach/cc/detail/color_token.hpp>

namespace ach::cc::detail {

class color_tokenizer
{
public:
	color_tokenizer(std::string_view text)
	: extractor(text) {}

	color_token next_token(color_options options);

	[[nodiscard]] bool has_reached_end() const noexcept { return extractor.has_reached_end(); }
	text_location current_location() const noexcept { return extractor.current_location(); }

private:
	text_extractor extractor;
};

}
