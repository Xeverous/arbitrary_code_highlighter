#pragma once

#include <string_view>
#include <optional>

namespace ach {

struct html_text
{
	std::string_view text;
};

struct css_class
{
	std::string_view name;
};

struct simple_span_element
{
	html_text text;
	std::optional<css_class> class_;
};

struct quote_span_element
{
	html_text text;
	css_class primary_class;
	css_class escape_class;
	char escape;
};

}
