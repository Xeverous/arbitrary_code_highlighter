#include <ach/clangd/core.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/code_tokenizer.hpp>
#include <ach/web/types.hpp>
#include <ach/web/html_builder.hpp>
#include <ach/utility/visitor.hpp>

#include <cassert>
#include <optional>
#include <string_view>
#include <variant>

namespace ach::clangd {

namespace {

namespace css {

// from syntax

constexpr auto pp_hash = "pp-hash";
constexpr auto pp_directive = "pp-directive";
constexpr auto pp_header_file = "pp-header";
constexpr auto pp_macro = "pp-macro";
constexpr auto pp_macro_param = "pp-macro-param";
constexpr auto pp_macro_body = "pp-macro-body"; // TODO implement and use
constexpr auto pp_other = "pp-other";

constexpr auto comment_single = "com-single";
constexpr auto comment_single_doxygen = "com-single-dox";
constexpr auto comment_multi = "com-multi";
constexpr auto comment_multi_doxygen = "com-multi-dox";
constexpr auto comment_tag_todo = "com-tag-todo";
constexpr auto comment_tag_doxygen = "com-tag-dox";

constexpr auto keyword = "keyword";

constexpr auto literal_number = "lit-num";
constexpr auto literal_character = "lit-chr";
constexpr auto literal_string = "lit-str";
constexpr auto literal_string_raw_delimeter = "lit-str-raw-delim";
constexpr auto literal_prefix = "lit-pre";
constexpr auto literal_suffix = "lit-suf";
constexpr auto escape_sequence = "esc-seq";
constexpr auto format_sequence = "fmt-seq"; // TODO implement and use

constexpr auto unknown = "unknown";

// from clangd semantic token information

constexpr auto disabled_code = "disabled-code";

constexpr auto macro = "macro"; // macro usages (outside preprocessor)

constexpr auto parameter = "param";
constexpr auto out_parameter = "param-out";
constexpr auto template_parameter = "param-tmpl";

constexpr auto variable_local = "var-local";
constexpr auto variable_global = "var-global";
constexpr auto variable_member = "var-member";
constexpr auto enumerator = "enum";

constexpr auto function_free = "func-free";
constexpr auto function_member = "func-member";
constexpr auto function_virtual = "func-virtual";

constexpr auto type_class = "type-class";
constexpr auto type_interface = "type-interface";
constexpr auto type_enum = "type-enum";
constexpr auto type_generic = "type";

constexpr auto concept_ = "concept";
constexpr auto dependent_name = "dep-name";

constexpr auto namespace_ = "namespace";

}

struct open_span_paste_text { std::string_view css_class; };
struct paste_text_close_span {};
struct open_paste_close { std::string_view css_class; };
struct paste_only {};
struct semantic_token_action { std::string_view css_class; semantic_token_color_variance color; };
struct end_of_input {};
struct action_error {};

using builder_action = std::variant<
	open_span_paste_text,
	paste_text_close_span,
	open_paste_close,
	paste_only,
	semantic_token_action,
	end_of_input,
	action_error
>;

builder_action token_to_action(syntax_token token)
{
	switch (token) {
		case syntax_token::preprocessor_hash:
			return open_paste_close{css::pp_hash};
		case syntax_token::preprocessor_directive:
			return open_paste_close{css::pp_directive};
		case syntax_token::preprocessor_header_file:
			return open_paste_close{css::pp_header_file};
		case syntax_token::preprocessor_macro:
			return open_paste_close{css::pp_macro};
		case syntax_token::preprocessor_macro_param:
			return open_paste_close{css::pp_macro_param};
		case syntax_token::preprocessor_other:
			return open_paste_close{css::pp_other};
		case syntax_token::disabled_code_begin:
			return open_span_paste_text{css::disabled_code};
		case syntax_token::disabled_code_end:
			return paste_text_close_span{};
		case syntax_token::comment_begin_single:
			return open_span_paste_text{css::comment_single};
		case syntax_token::comment_begin_single_doxygen:
			return open_span_paste_text{css::comment_single_doxygen};
		case syntax_token::comment_begin_multi:
			return open_span_paste_text{css::comment_multi};
		case syntax_token::comment_begin_multi_doxygen:
			return open_span_paste_text{css::comment_multi_doxygen};
		case syntax_token::comment_end:
			return paste_text_close_span{};
		case syntax_token::comment_tag_todo:
			return open_paste_close{css::comment_tag_todo};
		case syntax_token::comment_tag_doxygen:
			return open_paste_close{css::comment_tag_doxygen};
		case syntax_token::keyword:
			return open_paste_close{css::keyword};
		case syntax_token::identifier_unknown:
			return open_paste_close{css::unknown};
		case syntax_token::literal_prefix:
			return open_paste_close{css::literal_prefix};
		case syntax_token::literal_suffix:
			return open_paste_close{css::literal_suffix};
		case syntax_token::literal_number:
			return open_paste_close{css::literal_number};
		case syntax_token::literal_string:
			return open_paste_close{css::literal_string};
		case syntax_token::literal_char_begin:
			return open_span_paste_text{css::literal_character};
		case syntax_token::literal_string_begin:
			return open_span_paste_text{css::literal_string};
		case syntax_token::literal_text_end:
			return paste_text_close_span{};
		case syntax_token::literal_string_raw_quote:
			return open_paste_close{css::literal_string};
		case syntax_token::literal_string_raw_delimeter:
			return open_paste_close{css::literal_string_raw_delimeter};
		case syntax_token::literal_string_raw_paren:
			return open_paste_close{css::literal_string_raw_delimeter};
		case syntax_token::escape_sequence:
			return open_paste_close{css::escape_sequence};
		case syntax_token::whitespace:
			return paste_only{};
		case syntax_token::nothing_special:
			return paste_only{};
		case syntax_token::end_of_input:
			return end_of_input{};
	}

	return action_error{};
}

std::optional<std::string_view> semantic_token_info_to_css_class(semantic_token_info info)
{
	auto handle_variable = [&](std::string_view css_class) -> std::string_view {
		if (info.modifers.is_static
			|| info.modifers.scope == semantic_token_scope_modifier::file
			|| info.modifers.scope == semantic_token_scope_modifier::global)
		{
			return css::variable_global;
		}

		return css_class;
	};

	switch (info.type) {
		case semantic_token_type::parameter:
			if (info.modifers.is_out_parameter)
				return css::out_parameter;
			else
				return css::parameter;
		case semantic_token_type::variable:
			return handle_variable(css::variable_local);
		case semantic_token_type::property:
			return handle_variable(css::variable_member);
		case semantic_token_type::enum_member:
			return css::enumerator;
		case semantic_token_type::function:
			return css::function_free;
		case semantic_token_type::method:
			if (info.modifers.is_virtual)
				return css::function_virtual;
			else
				return css::function_member;
		case semantic_token_type::class_:
			return css::type_class;
		case semantic_token_type::interface:
			return css::type_interface;
		case semantic_token_type::enum_:
			return css::type_enum;
		case semantic_token_type::type:
			return css::type_generic;
		case semantic_token_type::concept_:
			return css::concept_;
		case semantic_token_type::template_parameter:
			return css::template_parameter;
		case semantic_token_type::namespace_:
			return css::namespace_;
		case semantic_token_type::disabled_code:
			return css::disabled_code;
		case semantic_token_type::macro:
			return css::macro;
		case semantic_token_type::unknown:
			if (info.modifers.is_dependent_name)
				return css::dependent_name;
			else
				break;
	}

	return std::nullopt;
}

builder_action token_to_action(identifier_token token)
{
	std::optional<std::string_view> css_class = semantic_token_info_to_css_class(token.info);
	if (css_class == std::nullopt)
		return action_error{};

	return semantic_token_action{*css_class, token.color_variance};
}

}

std::variant<std::string, highlighter_error> run_highlighter(
	std::string_view code,
	utility::range<const semantic_token*> tokens,
	utility::range<const std::string*> keywords,
	highlighter_options options)
{
	std::optional<code_tokenizer> maybe_tokenizer = code_tokenizer::create(code, tokens);
	if (maybe_tokenizer == std::nullopt)
		return highlighter_error{{}, {}, error_reason::invalid_semantic_token_data};

	code_tokenizer& tokenizer = *maybe_tokenizer;

	const bool wrap_in_table = !options.table_wrap_css_class.empty();
	const auto num_lines = text::count_lines(code);
	web::html_builder builder;
	// TODO use builder.reserve()
	if (wrap_in_table)
		builder.open_table(num_lines, options.table_wrap_css_class);

	bool done = false;
	while (!done) {
		// Save current progress before calling next_token.
		// While next_code_token doesn't modify the state when it fails,
		// the fail can occur later in token_to_action and the error
		// should point to semantic tokens before the operation.
		const auto current_semantic_tokens = tokenizer.current_semantic_tokens();
		const text::position current_position = tokenizer.current_position();

		std::variant<code_token, highlighter_error> token_or_error = tokenizer.next_code_token(keywords);
		if (std::holds_alternative<highlighter_error>(token_or_error))
			return std::get<highlighter_error>(token_or_error);

		const auto& token = std::get<code_token>(token_or_error);
		builder_action action = std::visit([](auto token) {
			return token_to_action(token);
		}, token.token_type);

		std::optional<highlighter_error> maybe_error = std::visit(utility::visitor{
			[&](open_span_paste_text action) -> std::optional<highlighter_error> {
				builder.open_span(web::css_class{action.css_class});
				builder.append_raw(token.origin.str);
				return std::nullopt;
			},
			[&](paste_text_close_span /* action */) -> std::optional<highlighter_error> {
				builder.append_raw(token.origin.str);
				builder.close_span();
				return std::nullopt;
			},
			[&](open_paste_close action) -> std::optional<highlighter_error> {
				builder.add_span(web::simple_span_element{
					web::html_text{token.origin.str},
					web::css_class{action.css_class}
				});
				return std::nullopt;
			},
			[&](paste_only /* action */) -> std::optional<highlighter_error> {
				builder.append_raw(token.origin.str);
				return std::nullopt;
			},
			[&](semantic_token_action action) -> std::optional<highlighter_error> {
				// TODO use action.color for color variance feature
				builder.add_span(web::simple_span_element{
					web::html_text{token.origin.str},
					web::css_class{action.css_class}
				});
				return std::nullopt;
			},
			[&](end_of_input /* action */) -> std::optional<highlighter_error> {
				done = true;
				return std::nullopt;
			},
			[&](action_error /* error */) -> std::optional<highlighter_error> {
				return highlighter_error{
					current_position,
					current_semantic_tokens,
					error_reason::internal_error_token_to_action
				};
			}
		}, action);

		if (maybe_error)
			return *maybe_error;
	}

	if (!tokenizer.has_reached_end()) {
		return highlighter_error{
			tokenizer.current_position(),
			tokenizer.current_semantic_tokens(),
			error_reason::internal_error_unhandled_end_of_input
		};
	}

	if (wrap_in_table)
		builder.close_table();

	return std::move(builder.str());
}

}
