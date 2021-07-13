#pragma once

#include <ach/common/text_location.hpp>
#include <ach/common/options.hpp>
// #include <ach/ansi/color_options.hpp>

#include <string>
#include <string_view>
#include <variant>
#include <iosfwd>

namespace ach::ansi {

struct highlighter_options
{
	generation_options generation;
	// color_options color;
};

struct highlighter_error
{
	text_location location;
	std::string_view reason;
};

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view text,
	highlighter_options const& options = {});

std::ostream& operator<<(std::ostream& os, highlighter_error const& error);

}
