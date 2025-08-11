#pragma once

#include <ach/web/types.hpp>

#include <string>
#include <string_view>

namespace ach::web {

class html_builder
{
public:
	explicit html_builder() = default;

	void reset() { result.clear(); }
	void reserve(std::size_t n) { result.reserve(n); };
	void add_span(simple_span_element span, bool replace_underscores_to_hyphens = false);
	void add_span(quote_span_element span, bool replace_underscores_to_hyphens = false);

	void open_table(std::size_t lines, std::string_view code_class);
	void close_table();

	std::string& str() noexcept { return result; }
	const std::string& str() const noexcept { return result; }

	void open_span(css_class class_, bool replace_underscores_to_hyphens = false);
	void open_span(css_class class1, css_class class2, bool replace_underscores_to_hyphens = false);
	void close_span();
	void append_raw(std::string_view text);

private:
	void append_raw(char c);
	void append_class(css_class class_, bool replace_underscores_to_hyphens);

	/*
	 * text must be a valid quoted string (eg text == "\"abc\""):
	 * - .empty() || (.front() == .back() && escape != .front())
	 * - every escape character must be followed by some character (escape can not be the last char)
	 *
	 * otherwise the behavior is undefined
	 */
	void append_quoted(
		std::string_view text,
		char escape_char,
		css_class escape_span_class,
		bool replace_underscores_to_hyphens);

	std::string result;
};

}
