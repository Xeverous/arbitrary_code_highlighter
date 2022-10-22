#pragma once

namespace ach::mirror::errors {

	// errors against code
	constexpr auto expected_identifier = "expected identifier";
	constexpr auto expected_symbol = "expected symbol";
	constexpr auto expected_number = "expected number";
	constexpr auto expected_quoted = "expected quoted text";
	constexpr auto expected_line_feed = "expected line feed";
	constexpr auto mismatched_symbol = "mismatched symbol";
	constexpr auto invalid_number = "invalid number";
	constexpr auto insufficient_characters = "insufficient characters for fixed-length span";
	constexpr auto exhausted_color = "color input exhausted but code remains";

	// errors against color
	constexpr auto expected_span_class = "expected span class name";
	constexpr auto invalid_css_class = "invalid CSS class";

}
