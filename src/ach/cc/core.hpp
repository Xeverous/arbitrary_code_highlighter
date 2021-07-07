#pragma once

#include <ach/common/text_location.hpp>
#include <ach/common/options.hpp>
#include <ach/cc/color_options.hpp>

#include <string>
#include <string_view>
#include <variant>
#include <iosfwd>

namespace ach::cc {

struct highlighter_options
{
	generation_options generation;
	color_options color;
};

struct highlighter_error
{
	text_location color_location;
	text_location code_location;
	std::string_view reason; // should be initialized
	std::string_view extra_reason = {};
};

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view code,
	std::string_view color,
	highlighter_options const& options = {});

std::ostream& operator<<(std::ostream& os, highlighter_error const& error);

}
