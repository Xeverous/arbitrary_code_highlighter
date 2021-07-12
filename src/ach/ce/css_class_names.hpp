#pragma once

#include <string_view>

namespace ach::ce {

struct css_class_names
{
	// default color
	std::string_view grey;
	// paths and code citations
	std::string_view white;

	// note
	std::string_view cyan;
	// warning
	std::string_view magenta;
	// error
	std::string_view red;

	// extra highlights
	std::string_view green;
	std::string_view blue;

	// CSS from https://gcc.gnu.org/gcc-9/changes.html has
	// this color but it's not used anywhere on the page.
	// std::string_view lime;
};

}
