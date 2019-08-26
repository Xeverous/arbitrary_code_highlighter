#pragma once

#include "text_location.hpp"

#include <string>
#include <string_view>
#include <optional>
#include <variant>
#include <vector>

struct highlighter_options
{
	bool replace_underscores_to_hyphens = false;
	// empty optional => any name is allowed
	// otherwise => any used class name must exist in the vector
	std::optional<std::vector<std::string>> allowed_class_names;
};

struct highlighter_error
{
	text_location color_location;
	text_location code_location;
	const char* reason;
};

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view code,
	std::string_view color,
	highlighter_options const& options);
