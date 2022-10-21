#include <ach/detail/html_builder.hpp>
#include <cassert>

namespace ach::detail {

void html_builder::open_table(std::size_t lines, std::string_view code_class)
{
	result +=
		"<table class=\"codetable\">"
		"<tbody><tr><td class=\"linenos\"><div class=\"linenodiv\"><pre>";

	for (std::size_t i = 1; i <= lines; ++i) {
		result.append(std::to_string(i));
		result += "\n";
	}

	result += "</pre></div></td><td class=\"code\"><pre class=\"code ";
	result += code_class;
	result += "\">";
}

void html_builder::close_table()
{
	result += "</pre></td></tr></tbody></table>";
}

void html_builder::add_span(simple_span_element span, bool replace_underscores_to_hyphens)
{
	if (!span.class_) {
		append_raw(span.text.text);
		return;
	}

	open_span(*span.class_, replace_underscores_to_hyphens);
	append_raw(span.text.text);
	close_span();
}

void html_builder::add_span(quote_span_element span, bool replace_underscores_to_hyphens)
{
	open_span(span.primary_class, replace_underscores_to_hyphens);
	append_quoted(span.text.text, span.escape, span.escape_class, replace_underscores_to_hyphens);
	close_span();
}

void html_builder::open_span(css_class class_, bool replace_underscores_to_hyphens)
{
	result += "<span class=\"";
	append_class(class_, replace_underscores_to_hyphens);
	result += "\">";
}

void html_builder::close_span()
{
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
		const auto css_class = class_.name;
		result.append(css_class.data(), css_class.data() + css_class.length());
	}
}

void html_builder::append_quoted(
	std::string_view text,
	char escape_char,
	css_class escape_span_class,
	bool replace_underscores_to_hyphens)
{
	assert(text.empty() || text.front() == text.back());
	assert(text.empty() || text.front() != escape_char);

	bool inside_escape = false;
	bool escape_opened = false;
	for (char c : text) {
		if (inside_escape) {
			append_raw(c);
			inside_escape = false;
			continue;
		}

		if (c == escape_char) {
			if (!escape_opened) {
				open_span(escape_span_class, replace_underscores_to_hyphens);
				escape_opened = true;
			}
			inside_escape = true;
			append_raw(c);
			continue;
		}

		if (escape_opened) {
			close_span();
			escape_opened = false;
		}

		append_raw(c);
	}

	assert(!inside_escape && "valid input text should not end with opened escape");

	if (escape_opened) {
		close_span();
	}
}

void html_builder::append_raw(std::string_view text)
{
	for (char c : text)
		append_raw(c);
}

void html_builder::append_raw(char c)
{
	const std::string_view escaped = to_escaped_html(c);
	result.append(escaped.data(), escaped.data() + escaped.length());
}

std::string_view html_builder::to_escaped_html(const char& c) noexcept
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
