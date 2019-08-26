#pragma once

#include "text_location.hpp"
#include "html_types.hpp"

#include <variant>

struct identifier_span
{
	css_class class_;
};

struct fixed_length_span
{
	css_class class_;
	int length;
	text_location name_origin;
	text_location length_origin;
};

struct line_delimited_span
{
	css_class class_;
};

struct symbol
{
	char expected_symbol;
};

struct quoted_span
{
	char delimeter;
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

	symbol,

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
