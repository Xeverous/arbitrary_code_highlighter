#include <ach/detail/html_builder.hpp>

namespace ach::detail {

void html_builder::add_span(span_element span, bool replace_underscores_to_hyphens)
{
	if (!span.class_) {
		append_raw_escaped(span.text.text);
		return;
	}

	result += "<span class=\"";
	append_class(*span.class_, replace_underscores_to_hyphens);
	result += "\">";

	append_raw_escaped(span.text.text);

	result += "</span>";
}

void html_builder::append_class(css_class class_, bool replace_underscores_to_hyphens)
{
	if (replace_underscores_to_hyphens) {
		for (char c : class_.name) {
			result.push_back(c == '_' ? '-' : c);
		}
	}
	else {
		auto const css_class = class_.name;
		result.append(css_class.data(), css_class.data() + css_class.length());
		return;
	}
}

void html_builder::append_raw_escaped(std::string_view text)
{
	for (char c : text)
		append_raw_escaped(c);
}

void html_builder::append_raw_escaped(char c)
{
	std::string_view const escaped = to_escaped_html(c);
	result.append(escaped.data(), escaped.data() + escaped.length());
}

std::string_view html_builder::to_escaped_html(char const& c) noexcept
{
	switch (c)
	{
		case '&':
			return "&amp;";
		case '<':
			return "&lt;";
		case '>':
			return "&gt;";
		default:
			return {&c, 1};
	}
}

}
