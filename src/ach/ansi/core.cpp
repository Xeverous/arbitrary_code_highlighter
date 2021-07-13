#include <ach/ansi/core.hpp>
#include <ach/common/html_builder.hpp>
#include <ach/utility/text.hpp>

namespace ach::ansi {

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view text,
	highlighter_options const& options = {})
{
	bool const wrap_in_table = !options.generation.table_wrap_css_class.empty();
	html_builder builder;
	builder.reserve(text.size() + text.size() / 2); // (very rough estimate)

	if (wrap_in_table) {
		builder.open_table(utility::count_lines(text), options.generation.table_wrap_css_class);
	}



	if (wrap_in_table)
		builder.close_table();
}

std::ostream& operator<<(std::ostream& os, highlighter_error const& error)
{
	return os << error.reason << "\n" << error.location << "\n";
}

}
