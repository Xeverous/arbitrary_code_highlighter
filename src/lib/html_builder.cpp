#include "html_builder.hpp"

void html_builder::add_span(span_element span)
{
	if (!span.class_) {
		append_raw_escaped(span.text.text);
		return;
	}

	auto const css_class = (*span.class_).name;
	result += "<span class=\"";
	result.append(css_class.data(), css_class.data() + css_class.length());
	result += "\">";

	append_raw_escaped(span.text.text);

	result += "</span>";
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
		case ' ':
			return "&nbsp;";
		case '\t':
			return "&#9;";
		default:
			return {&c, 1};
	}
}
