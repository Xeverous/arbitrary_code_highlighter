#pragma once

#include <string_view>

namespace ach {

struct generation_options
{
	bool replace_underscores_to_hyphens = false;
	std::string_view table_wrap_css_class = {};
	std::string_view valid_css_classes = {};
};

}
