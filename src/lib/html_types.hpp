#pragma once

#include <string_view>
#include <optional>

struct html_text
{
	std::string_view text;
};

struct css_class
{
	std::string_view name;
};

struct span_element
{
	html_text text;
	std::optional<css_class> class_;
};
