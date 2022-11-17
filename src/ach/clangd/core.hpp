#pragma once

#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/utility/range.hpp>

#include <string_view>
#include <string>
#include <variant>

namespace ach::clangd {

struct highlighter_options
{
	std::string_view table_wrap_css_class = {};
	int color_variants = 6;
};

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view code,
	utility::range<const semantic_token*> tokens,
	utility::range<const std::string*> keywords,
	highlighter_options options = {});

}
