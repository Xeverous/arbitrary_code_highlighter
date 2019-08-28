#pragma once

#include <ach/detail/html_types.hpp>

#include <string>
#include <string_view>

namespace ach::detail {

class html_builder
{
public:
	explicit html_builder() = default;

	void reserve(int length) { result.reserve(length); };
	void add_span(span_element span, bool replace_underscores_to_hyphens = false);

	std::string& str() noexcept { return result; }
	std::string const& str() const noexcept { return result; }

private:
	static std::string_view to_escaped_html(char const& c) noexcept;
	static std::string_view to_escaped_html(char&& c) = delete;

	void append_raw_escaped(std::string_view text);
	void append_raw_escaped(char c);

	void append_class(css_class class_, bool replace_underscores_to_hyphens);

	std::string result;
};

}
