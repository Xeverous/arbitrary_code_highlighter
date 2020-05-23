#pragma once

namespace ach {

constexpr auto program_description =
	"Arbitrary Code Highlighter - embed arbitrary code in HTML <span> tags with\n"
	"CSS classes of your choice for rich syntax highlightning.\n"
	"This program generates HTML only. CSS is your responsibility.\n"
	"See documentation for examples.\n";

}

namespace ach::version {

constexpr int major = 1;
constexpr int minor = 3;
constexpr int patch = 0;

}
