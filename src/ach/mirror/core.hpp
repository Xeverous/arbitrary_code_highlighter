#pragma once

#include <ach/text/types.hpp>
#include <ach/mirror/color_options.hpp>

#include <string>
#include <string_view>
#include <variant>
#include <iosfwd>

namespace ach::mirror {

struct generation_options
{
	bool replace_underscores_to_hyphens = false;
	std::string_view table_wrap_css_class = {};
	std::string_view valid_css_classes = {};
};

struct highlighter_options
{
	generation_options generation;
	color_options color;
};

struct highlighter_error
{
	text::located_span color_location;
	text::located_span code_location;
	std::string_view reason; // should not be empty
	std::string_view extra_reason = {};
};

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view code,
	std::string_view color,
	const highlighter_options& options = {});

std::ostream& operator<<(std::ostream& os, text::located_span ls);
std::ostream& operator<<(std::ostream& os, const mirror::highlighter_error& error);

}
