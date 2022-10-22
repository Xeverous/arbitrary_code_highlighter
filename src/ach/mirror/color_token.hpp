#pragma once

#include <ach/text/location.hpp>
#include <ach/web/types.hpp>

#include <variant>
#include <optional>

namespace ach::mirror {

struct identifier_span
{
	web::css_class class_;
};

struct fixed_length_span
{
	std::optional<web::css_class> class_;
	std::size_t length;
	text::location name_origin;
	text::location length_origin;
};

struct line_delimited_span
{
	std::optional<web::css_class> class_;
};

struct number
{
	web::css_class class_;
};

struct symbol
{
	char expected_symbol;
};

struct empty_token {};

struct quoted_span
{
	web::css_class primary_class;
	web::css_class escape_class;
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
	text::location origin;
};

}
