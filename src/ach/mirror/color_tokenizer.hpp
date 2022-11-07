#pragma once

#include <ach/text/extractor.hpp>
#include <ach/mirror/color_options.hpp>
#include <ach/mirror/color_token.hpp>

namespace ach::mirror {

class color_tokenizer
{
public:
	color_tokenizer(std::string_view text)
	: extractor(text) {}

	color_token next_token(color_options options);

	[[nodiscard]] bool has_reached_end() const noexcept { return extractor.has_reached_end(); }
	text::located_span current_location() const noexcept { return extractor.current_location(); }

private:
	text::extractor extractor;
};

}
