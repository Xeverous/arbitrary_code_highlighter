#pragma once

#include <ach/common/text_location.hpp>
#include <ach/common/options.hpp>
#include <ach/ce/css_class_names.hpp>

#include <string>
#include <string_view>
#include <variant>
#include <iosfwd>

namespace ach::ce {

struct highlighter_options
{
	generation_options generation;
	css_class_names css;
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
