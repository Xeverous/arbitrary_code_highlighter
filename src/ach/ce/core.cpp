#include <ach/ce/core.hpp>
#include <ach/utility/text.hpp>
#include <ach/common/html_builder.hpp>

#include <ostream>

namespace ach::ce {

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view text,
	highlighter_options const& options)
{
	bool const wrap_in_table = !options.generation.table_wrap_css_class.empty();
	auto const num_lines = utility::count_lines(text);
	html_builder builder;
	// TODO use builder.reserve()
	if (wrap_in_table) {
		builder.open_table(num_lines, options.generation.table_wrap_css_class);
	}

	color_tokenizer color_tr(color);
	code_tokenizer code_tr(code);

	bool done = false;
	while (!done) {
		color_token color_tn = color_tr.next_token(options.color);
		result_t result = process_color_token(color_tn, code_tr);

		auto maybe_error = std::visit(utility::visitor{
			[](highlighter_error error) -> std::optional<highlighter_error> {
				return error;
			},
			[&](auto span) -> std::optional<highlighter_error> {
				if (!options.generation.valid_css_classes.empty()) {
					invalid_class_t maybe_error = check_css_classes(
						span, options.generation.valid_css_classes);

					if (maybe_error)
						return highlighter_error{
							color_tn.origin,
							code_tr.current_location(),
							errors::invalid_css_class,
							(*maybe_error).name};
				}

				builder.add_span(span, options.generation.replace_underscores_to_hyphens);
				return std::nullopt;
			},
			[&](end_of_input) -> std::optional<highlighter_error> {
				done = true; // exit the loop when color input ends
				return std::nullopt;
			}
		}, std::move(result));

		if (maybe_error)
			return *maybe_error;
	}

	if (!code_tr.has_reached_end()) {
		// underline remaining code
		return highlighter_error{
			color_tr.current_location(),
			code_tr.remaining_line_text(),
			errors::exhausted_color
		};
	}

	if (wrap_in_table)
		builder.close_table();

	return std::move(builder.str());
}

std::ostream& operator<<(std::ostream& os, highlighter_error const& error)
{
	return os << error.reason << "\n" << error.location;
}

}
