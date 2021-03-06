#pragma once

#include <ach/text_location.hpp>
#include <ach/detail/html_types.hpp>

#include <variant>
#include <optional>

namespace ach::detail {

struct identifier_span
{
	css_class class_;
};

struct fixed_length_span
{
	std::optional<css_class> class_;
	std::size_t length;
	text_location name_origin;
	text_location length_origin;
};

struct line_delimited_span
{
	std::optional<css_class> class_;
};

struct number
{
	css_class class_;
};

struct symbol
{
	char expected_symbol;
};

struct empty_token {};

struct quoted_span
{
	css_class primary_class;
	css_class escape_class;
	char delimeter;
	char escape;
};

struct end_of_line {};
struct end_of_input {};

struct invalid_token
{
	const char* reason;
};

using color_token_variant = std::variant<
	identifier_span,
	fixed_length_span,
	line_delimited_span,

	number,
	symbol,
	empty_token,

	quoted_span,

	end_of_line,
	end_of_input,

	invalid_token
>;

struct color_token
{
	color_token_variant token;
	text_location origin;
};

}
