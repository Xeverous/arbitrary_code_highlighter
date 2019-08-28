#include <ach/core.hpp>
#include <ach/utility/visitor.hpp>
#include <ach/detail/code_tokenizer.hpp>
#include <ach/detail/color_tokenizer.hpp>
#include <ach/detail/html_builder.hpp>

#include <cassert>
#include <variant>
#include <utility>

namespace {

using namespace ach;
using namespace ach::detail;

using result_t = std::variant<span_element, highlighter_error>;

result_t process_color_token(color_token color_tn, code_tokenizer& code_tr)
{
	return std::visit(utility::visitor{
		[&](identifier_span id_span) -> result_t {
			text_location const loc_code_id = code_tr.extract_identifier();

			if (loc_code_id.str().empty()) {
				return highlighter_error{color_tn.origin, loc_code_id, "expected identifier"};
			}

			return span_element{html_text{loc_code_id.str()}, id_span.class_};
		},
		[&](fixed_length_span fl_span) -> result_t {
			text_location const loc_fl_text = code_tr.extract_n_characters(fl_span.length);
			auto const extracted_chars = static_cast<int>(loc_fl_text.str().size());
			assert(extracted_chars <= fl_span.length);

			if (extracted_chars < fl_span.length) {
				return highlighter_error{color_tn.origin, loc_fl_text, "insufficient characters for fixed-length span"};
			}

			return span_element{html_text{loc_fl_text.str()}, fl_span.class_};
		},
		[&](line_delimited_span ld_span) -> result_t {
			text_location const loc_ld_text = code_tr.extract_until_end_of_line();
			return span_element{html_text{loc_ld_text.str()}, ld_span.class_};
		},
		[&](symbol s) -> result_t {
			text_location const loc_symbol = code_tr.extract_n_characters(1);

			if (loc_symbol.str().size() != 1u) {
				return highlighter_error{color_tn.origin, loc_symbol, "failed to extract a symbol"};
			}

			if (loc_symbol.str().front() != s.expected_symbol) {
				return highlighter_error{color_tn.origin, loc_symbol, "symbols do not match"};
			}

			return span_element{html_text{loc_symbol.str()}, std::nullopt};
		},
		[&](quoted_span) -> result_t {
			assert("NOT IMPLEMENTED" && false);
			return span_element{html_text{""}, std::nullopt};
		},
		[&](end_of_line) -> result_t {
			text_location const loc_eol = code_tr.extract_n_characters(1);

			if (loc_eol.str().size() != 1u) {
				return highlighter_error{color_tn.origin, loc_eol, "failed to extract line feed"};
			}

			if (loc_eol.str().front() != '\n') {
				return highlighter_error{color_tn.origin, loc_eol, "expected line feed character"};
			}

			return span_element{html_text{loc_eol.str()}, std::nullopt};
		},
		[&](end_of_input /* eoi */) -> result_t {
			if (code_tr.has_reached_end())  {
				return span_element{html_text{""}, std::nullopt};
			}

			return highlighter_error{
				color_tn.origin,
				code_tr.current_location(),
				"reached color input end, but some code remains"};
		},
		[&](invalid_token invalid) -> result_t {
			return highlighter_error{color_tn.origin, code_tr.current_location(), invalid.reason};
		}
	}, color_tn.token);
}

}

namespace ach {

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view code,
	std::string_view color,
	highlighter_options const& /* options */)
{
	color_tokenizer color_tr(color);
	code_tokenizer code_tr(code);
	html_builder builder;

	while (!color_tr.has_reached_end()) {
		result_t result = process_color_token(color_tr.next_token(), code_tr);

		if (auto ptr = std::get_if<highlighter_error>(&result); ptr != nullptr) {
			return *ptr;
		}

		builder.add_span(std::get<span_element>(result));
	}

	assert(code_tr.has_reached_end());

	return std::move(builder.str());
}

}
