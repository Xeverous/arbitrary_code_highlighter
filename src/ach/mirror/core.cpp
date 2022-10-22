#include <ach/mirror/core.hpp>
#include <ach/mirror/errors.hpp>
#include <ach/mirror/code_tokenizer.hpp>
#include <ach/mirror/color_tokenizer.hpp>
#include <ach/web/html_builder.hpp>
#include <ach/text/utils.hpp>
#include <ach/utility/visitor.hpp>

#include <cassert>
#include <variant>
#include <utility>
#include <ostream>
#include <algorithm>
#include <functional>

namespace ach::mirror {
namespace {

using result_t = std::variant<web::simple_span_element, web::quote_span_element, highlighter_error, end_of_input>;

result_t process_color_token(color_token color_tn, code_tokenizer& code_tr)
{
	return std::visit(utility::visitor{
		[&](identifier_span id_span) -> result_t {
			const text::location extracted_identifier = code_tr.extract_identifier();

			if (extracted_identifier.str().empty()) {
				return highlighter_error{color_tn.origin, extracted_identifier, errors::expected_identifier};
			}

			return web::simple_span_element{web::html_text{extracted_identifier.str()}, id_span.class_};
		},
		[&](fixed_length_span fl_span) -> result_t {
			const text::location extracted_characters = code_tr.extract_n_characters(fl_span.length);
			const auto extracted_chars = extracted_characters.str().size();
			assert(extracted_chars <= fl_span.length);

			if (extracted_chars < fl_span.length) {
				return highlighter_error{color_tn.origin, extracted_characters, errors::insufficient_characters};
			}

			return web::simple_span_element{web::html_text{extracted_characters.str()}, fl_span.class_};
		},
		[&](line_delimited_span ld_span) -> result_t {
			const text::location extracted_text = code_tr.extract_until_end_of_line();
			// no if extracted_text.str().empty() here - we want to allow empty extractions
			return web::simple_span_element{web::html_text{extracted_text.str()}, ld_span.class_};
		},
		[&](symbol s) -> result_t {
			const text::location extracted_symbol = code_tr.extract_n_characters(1);

			if (extracted_symbol.str().empty()) {
				return highlighter_error{color_tn.origin, extracted_symbol, errors::expected_symbol};
			}

			assert(extracted_symbol.str().size() == 1u);

			if (extracted_symbol.str().front() != s.expected_symbol) {
				return highlighter_error{color_tn.origin, extracted_symbol, errors::mismatched_symbol};
			}

			return web::simple_span_element{web::html_text{extracted_symbol.str()}, std::nullopt};
		},
		[&](number num) -> result_t {
			const text::location extracted_digits = code_tr.extract_digits();

			if (extracted_digits.str().empty()) {
				return highlighter_error{color_tn.origin, extracted_digits, errors::expected_number};
			}

			return web::simple_span_element{web::html_text{extracted_digits.str()}, num.class_};
		},
		[&](empty_token) -> result_t {
			return web::simple_span_element{web::html_text{}, std::nullopt};
		},
		[&](quoted_span span) -> result_t {
			const text::location extracted_text = code_tr.extract_quoted(span.delimeter, span.escape);

			if (extracted_text.str().empty()) {
				return highlighter_error{color_tn.origin, extracted_text, errors::expected_quoted};
			}

			return web::quote_span_element{
				web::html_text{extracted_text.str()}, span.primary_class, span.escape_class, span.escape
			};
		},
		[&](end_of_line) -> result_t {
			const text::location extracted_char = code_tr.extract_n_characters(1);

			if (extracted_char.str().empty()) {
				return highlighter_error{color_tn.origin, extracted_char, errors::expected_line_feed};
			}

			assert(extracted_char.str().size() == 1u);

			if (extracted_char.str().front() != '\n') {
				return highlighter_error{color_tn.origin, extracted_char, errors::expected_line_feed};
			}

			// ignore if no more lines are present
			(void) code_tr.load_next_line();

			return web::simple_span_element{web::html_text{extracted_char.str()}, std::nullopt};
		},
		[](end_of_input eoi) -> result_t {
			return eoi;
		},
		[&](invalid_token invalid) -> result_t {
			return highlighter_error{color_tn.origin, code_tr.current_location(), invalid.reason};
		}
	}, color_tn.token);
}

using invalid_class_t = std::optional<web::css_class>;

invalid_class_t check_class(web::css_class class_, std::string_view valid_classes)
{
	const auto it = std::search(
		valid_classes.begin(),
		valid_classes.end(),
		std::default_searcher(class_.name.begin(), class_.name.end()));

	if (it == valid_classes.end())
		return class_;
	else
		return std::nullopt;
}

invalid_class_t check_css_classes(web::simple_span_element el, std::string_view valid_classes)
{
	return check_class(*el.class_, valid_classes);
}

invalid_class_t check_css_classes(web::quote_span_element el, std::string_view valid_classes)
{
	invalid_class_t maybe_error = check_class(el.primary_class, valid_classes);
	if (maybe_error)
		return maybe_error;

	return check_class(el.escape_class, valid_classes);
}

}

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view code,
	std::string_view color,
	const highlighter_options& options)
{
	const bool wrap_in_table = !options.generation.table_wrap_css_class.empty();
	const auto num_lines = text::count_lines(code);
	web::html_builder builder;
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

std::ostream& operator<<(std::ostream& os, text::location tl)
{
	os << "line " << tl.line_number() << ":\n" << tl.line();

	if (tl.line().empty() || tl.line().back() != '\n')
		os << '\n';

	assert(tl.first_column() < tl.line().size());

	// match the whitespace character used - tabs have different length
	for (auto i = 0u; i < tl.first_column(); ++i) {
		if (tl.line()[i] == '\t')
			os << '\t';
		else
			os << ' ';
	}

	const auto highlight_length = tl.str().size();

	if (highlight_length == 0) {
		os << '^';
	}
	else {
		for (auto i = 0u; i < highlight_length; ++i)
			os << '~';
	}

	return os << '\n';
}

std::ostream& operator<<(std::ostream& os, const highlighter_error& error)
{
	os << error.reason;

	if (!error.extra_reason.empty())
		os << ": " << error.extra_reason;

	return os << "\nin code " << error.code_location << "in color " << error.color_location;
}

}
