#pragma once

#include "html_types.hpp"

#include <string>
#include <string_view>


class html_builder
{
public:
	explicit html_builder() = default;

	void reserve(int length) { result.reserve(length); };
	void add_span(span_element span);

	std::string& str() noexcept { return result; }
	std::string const& str() const noexcept { return result; }

private:
	static std::string_view to_escaped_html(char const& c) noexcept;
	static std::string_view to_escaped_html(char&& c) = delete;

	void append_raw_escaped(std::string_view text);
	void append_raw_escaped(char c);

	std::string result;
};
