#pragma once

#include <ach/text_location.hpp>
#include <ach/color_options.hpp>

#include <string>
#include <string_view>
#include <variant>

namespace ach {

struct generation_options
{
	bool replace_underscores_to_hyphens = false;
};

struct highlighter_options
{
	generation_options generation;
	color_options color;
};

struct highlighter_error
{
	text_location color_location;
	text_location code_location;
	const char* reason;
};

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view code,
	std::string_view color,
	highlighter_options const& options = {});

}
