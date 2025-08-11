#include <ach/text/types.hpp>
#include <ach/clangd/splice_utils.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/clangd/core.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/code_tokenizer.hpp>
#include <ach/web/types.hpp>
#include <ach/utility/visitor.hpp>

#include <algorithm>
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
constexpr auto pp_macro_body = "pp-macro-body";
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
constexpr auto format_sequence = "fmt-seq";

constexpr auto unknown = "unknown";

// from clangd semantic token information

constexpr auto disabled_code = "disabled-code";

constexpr auto macro = "macro"; // macro usages (outside preprocessor)

constexpr auto label = "label";

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
constexpr auto overloaded_operator = "oo";

constexpr auto type_class = "type-class";
constexpr auto type_interface = "type-interface";
constexpr auto type_enum = "type-enum";
constexpr auto type_generic = "type";

constexpr auto concept_ = "concept";
constexpr auto dependent_name = "dep-name";

constexpr auto namespace_ = "namespace";

}

struct basic_action
{
	static basic_action open_span_paste_text(std::string_view css_class, bool is_disabled_code)
	{
		basic_action action;
		action.css_class = css_class;
		action.open_span = true;
		action.is_disabled_code = is_disabled_code;
		return action;
	}

	static basic_action paste_text_close_span()
	{
		basic_action action;
		action.close_span = true;
		return action;
	}

	static basic_action open_paste_close(std::string_view css_class, bool is_disabled_code)
	{
		basic_action action;
		action.css_class = css_class;
		action.open_span = true;
		action.close_span = true;
		action.is_disabled_code = is_disabled_code;
		return action;
	}

	static basic_action paste_only()
	{
		return basic_action{};
	}

	std::string_view css_class;
	bool open_span = false;
	bool close_span = false;
	bool is_disabled_code = false;
};
struct semantic_token_action { std::string_view css_class; semantic_token_color_variance color; };
struct end_of_input {};
struct action_error
{
	error_reason reason;
	syntax_element_type syntax_element;
	semantic_token_info semantic_info;
};

using builder_action = std::variant<basic_action, semantic_token_action, end_of_input, action_error>;

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

	if (info.modifers.is_non_const_ref_parameter)
		return css::out_parameter;

	switch (info.type) {
		case semantic_token_type::parameter:
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
		case semantic_token_type::modifier:
			return css::keyword; // final and override used as intended
		case semantic_token_type::operator_:
		case semantic_token_type::bracket:
			// These 2 should not be reached here.
			// They are not coming from identifier tokens.
			break;
		case semantic_token_type::label:
			return css::label;
		case semantic_token_type::unknown:
			if (info.modifers.is_dependent_name)
				return css::dependent_name;
			else
				// unfortunately clangd does not report attributes
				// identifiers from disabled code will also land here
				return css::unknown;
	}

	return std::nullopt;
}

builder_action identifier_token_to_action(semantic_token_info token_info, semantic_token_color_variance color_variance)
{
	std::optional<std::string_view> css_class = semantic_token_info_to_css_class(token_info);
	if (css_class == std::nullopt)
		return action_error{error_reason::internal_error_token_to_action, syntax_element_type::identifier, token_info};

	// This always returns a semantic token action, which means it does not have disabled code field.
	// However, disabled code is always reported as a specific type of semantic token, replacing other semantic info.
	// Thus identifier-based semantic info never conflicts with disabled code - the latter replaces it.
	return semantic_token_action{*css_class, color_variance};
}

builder_action token_to_action(code_token token)
{
	const bool is_disabled_code = token.semantic_info.type == semantic_token_type::disabled_code;

	switch (token.syntax_element) {
		case syntax_element_type::preprocessor_hash:
			return basic_action::open_paste_close(css::pp_hash, is_disabled_code);
		case syntax_element_type::preprocessor_directive:
			return basic_action::open_paste_close(css::pp_directive, is_disabled_code);
		case syntax_element_type::preprocessor_header_file:
			return basic_action::open_paste_close(css::pp_header_file, is_disabled_code);
		case syntax_element_type::preprocessor_macro:
			return basic_action::open_paste_close(css::pp_macro, is_disabled_code);
		case syntax_element_type::preprocessor_macro_param:
			return basic_action::open_paste_close(css::pp_macro_param, is_disabled_code);
		case syntax_element_type::preprocessor_macro_body:
			return basic_action::open_paste_close(css::pp_macro_body, is_disabled_code);
		case syntax_element_type::preprocessor_other:
			return basic_action::open_paste_close(css::pp_other, is_disabled_code);
		// don't apply disabled-code-style to comments inside disabled code
		// (comments are not compilable code anyway and they already use very distinct style)
		case syntax_element_type::comment_begin_single:
			return basic_action::open_span_paste_text(css::comment_single, false);
		case syntax_element_type::comment_begin_single_doxygen:
			return basic_action::open_span_paste_text(css::comment_single_doxygen, false);
		case syntax_element_type::comment_begin_multi:
			return basic_action::open_span_paste_text(css::comment_multi, false);
		case syntax_element_type::comment_begin_multi_doxygen:
			return basic_action::open_span_paste_text(css::comment_multi_doxygen, false);
		case syntax_element_type::comment_end:
			return basic_action::paste_text_close_span();
		case syntax_element_type::comment_tag_todo:
			return basic_action::open_paste_close(css::comment_tag_todo, false);
		case syntax_element_type::comment_tag_doxygen:
			return basic_action::open_paste_close(css::comment_tag_doxygen, false);
		case syntax_element_type::keyword: {
			// Clangd generally doesn't report keywords but
			// - it will report "auto" when used as a type deduction, e.g. "auto x = f();"
			//   (with semantic info about deduced type, as if auto wasn't used)
			// - it won't report "auto" return type, e.g. "auto f() { return /* ... */; }"
			// - it won't report "auto" in trailing return type syntax, e.g. "auto f() -> T;"
			// - it will report "override", "final" etc. when used as entity names (type depends on entity)
			// - it will report "override", "final" etc. when used as intended (type = modifier)
			//   Technically these aren't keywords but identifiers with special meaning,
			//   which act as keywords in selected places. ACH assumes these are keywords unless
			//   there is some semantic information about them, indicating non-keyword use.
			if (token.semantic_info.type != semantic_token_type::unknown) {
				const auto special_meaning_identifiers = {
					// C++11
					"final",
					"override",
					// TM TS (Transactional Memory Technical Specification)
					"transaction_safe",
					"transaction_safe_dynamic",
					// C++20
					"import",
					"module",
					// C++26
					"pre",
					"post",
					"trivially_relocatable_if_eligible",
					"replaceable_if_eligible"
				};
				for (auto identifier : special_meaning_identifiers) {
					if (compare_spliced_with_raw(token.origin.str, identifier)) {
						return identifier_token_to_action(token.semantic_info, token.color_variance);
					}
				}
			}

			return basic_action::open_paste_close(css::keyword, is_disabled_code);
		}
		case syntax_element_type::identifier:
			return identifier_token_to_action(token.semantic_info, token.color_variance);
		case syntax_element_type::literal_prefix:
			return basic_action::open_paste_close(css::literal_prefix, is_disabled_code);
		case syntax_element_type::literal_suffix:
			return basic_action::open_paste_close(css::literal_suffix, is_disabled_code);
		case syntax_element_type::literal_number:
			return basic_action::open_paste_close(css::literal_number, is_disabled_code);
		case syntax_element_type::literal_string:
			return basic_action::open_paste_close(css::literal_string, is_disabled_code);
		case syntax_element_type::literal_char_begin:
			return basic_action::open_span_paste_text(css::literal_character, is_disabled_code);
		case syntax_element_type::literal_string_begin:
			return basic_action::open_span_paste_text(css::literal_string, is_disabled_code);
		case syntax_element_type::literal_text_end:
			return basic_action::paste_text_close_span();
		case syntax_element_type::literal_string_raw_quote:
			return basic_action::open_paste_close(css::literal_string, is_disabled_code);
		case syntax_element_type::literal_string_raw_delimeter:
			return basic_action::open_paste_close(css::literal_string_raw_delimeter, is_disabled_code);
		case syntax_element_type::literal_string_raw_paren:
			return basic_action::open_paste_close(css::literal_string_raw_delimeter, is_disabled_code);
		case syntax_element_type::escape_sequence:
			return basic_action::open_paste_close(css::escape_sequence, is_disabled_code);
		case syntax_element_type::format_sequence:
			return basic_action::open_paste_close(css::format_sequence, is_disabled_code);
		case syntax_element_type::overloaded_operator:
			return basic_action::open_paste_close(css::overloaded_operator, is_disabled_code);
		case syntax_element_type::whitespace:
		case syntax_element_type::nothing_special:
			return basic_action::paste_only();
		case syntax_element_type::symbol:
			if (is_disabled_code)
				// no CSS class to assign so replace initial one with disabled code
				return basic_action::open_paste_close(css::disabled_code, false);
			else
				return basic_action::paste_only();
		case syntax_element_type::end_of_input:
			return end_of_input{};
	}

	return action_error{error_reason::internal_error_token_to_action, token.syntax_element, token.semantic_info};
}

[[nodiscard]] std::variant<std::string, highlighter_error>
generate_html(
	web::html_builder& builder,
	const std::vector<code_token>& code_tokens,
	std::size_t code_lines,
	std::string_view table_wrap_css_class,
	int /* color_variants */)
{
	const bool wrap_in_table = !table_wrap_css_class.empty();
	if (wrap_in_table)
		builder.open_table(code_lines, table_wrap_css_class);

	for (const code_token& token : code_tokens) {
		const text::position current_position = token.origin.r.first;
		std::optional<highlighter_error> maybe_error = std::visit(utility::visitor{
			[&](basic_action action) -> std::optional<highlighter_error> {
				if (action.open_span) {
					if (action.is_disabled_code)
						builder.open_span(web::css_class{action.css_class}, web::css_class{css::disabled_code});
					else
						builder.open_span(web::css_class{action.css_class});
				}

				builder.append_raw(token.origin.str);

				if (action.close_span)
					builder.close_span();

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
				return std::nullopt;
			},
			[&](action_error error) -> std::optional<highlighter_error> {
				return highlighter_error::from_semantic(
					error.reason,
					current_position,
					error.syntax_element,
					error.semantic_info
				);
			}
		}, token_to_action(token));

		if (maybe_error)
			return *maybe_error;
	}

	if (wrap_in_table)
		builder.close_table();

	return std::move(builder.str());
}

class semantic_token_processor
{
public:
	semantic_token_processor(std::string_view code)
	: m_code(code)
	, m_text_it(code.data())
	{}

	[[nodiscard]] std::optional<highlighter_error>
	improve_code_tokens(
		std::vector<code_token>& code_tokens,
		utility::range<const semantic_token*> sem_tokens)
	{
		utility::range<const semantic_token*> current = {sem_tokens.first, sem_tokens.first};

		while (current.first != sem_tokens.last) {
			// Find a range of tokens that represent the same entity.
			// The only case where the resulting range will contain multiple elements is for spliced entities.
			auto it = current.first;
			while (it != sem_tokens.last) {
				if (ends_with_backslash_whitespace(semantic_token_str(*it))) {
					// the token is spliced
					if (it->info == current.first->info) {
						++it; // spliced parts should have the same info...
					}
					else {
						// ...if not, the semantic token data is invalid
						return highlighter_error::from_semantic(
							error_reason::internal_error_unsupported_spliced_token,
							it->pos_begin(),
							syntax_element_type::end_of_input, // no good option here
							it->info
						);
					}
				}
				else {
					// no more splice: accept the token and stop the loop
					// no test of info here as 2 adjacent tokens may have the same info by coincidence
					++it;
					break;
				}
			}

			current.last = it;
			if (current.empty())
				return highlighter_error::from_semantic(error_reason::internal_error_improve_code_tokens, {}, {}, {});

			auto maybe_error = improve_code_tokens_internal(
				code_tokens,
				current.front().pos_begin(),
				current.back().pos_end(),
				current.front().info,
				current.front().color_variance);

			if (maybe_error)
				return maybe_error;

			current.first = current.last;
		}

		return std::nullopt;
	}

private:
	// A single iteration - for 1 (potentially spliced) entity.
	// Otherwise (no splice) the current range should have size 1.
	[[nodiscard]] std::optional<highlighter_error>
	improve_code_tokens_internal(
		std::vector<code_token>& code_tokens,
		text::position start,
		text::position stop,
		semantic_token_info info,
		semantic_token_color_variance color_variance)
	{
		utility::range<code_token*> matching_tokens =
			find_matching_tokens(
				{code_tokens.data(), code_tokens.data() + code_tokens.size()},
				start,
				stop);

		if (matching_tokens.empty()) {
			return highlighter_error::from_semantic(
				error_reason::internal_error_find_matching_tokens,
				start,
				std::nullopt,
				info
			);
		}

		for (code_token& token : matching_tokens) {
			token.semantic_info = info;
			token.color_variance = color_variance;
		}

		return std::nullopt;
	}

	// Not const-qualified because it advances internal iterator.
	// All semantic tokens should be processed in order, in single pass.
	std::string_view semantic_token_str(semantic_token sem_token)
	{
		assert(m_text_it.position() <= sem_token.pos_begin());
		while (m_text_it.position() < sem_token.pos_begin())
			++m_text_it;

		return {m_text_it.pointer(), sem_token.length};
	}

	std::string_view m_code;
	text::text_iterator m_text_it;
};

}

utility::range<code_token*> find_matching_tokens(
	utility::range<code_token*> code_tokens,
	text::position start,
	text::position stop)
{
	utility::range<code_token*> result{
		std::lower_bound(
			code_tokens.begin(),
			code_tokens.end(),
			start,
			[](const code_token& token, text::position pos) {
				return token.origin.r.first < pos;
			}),
		std::upper_bound(
			code_tokens.begin(),
			code_tokens.end(),
			stop,
			[](text::position pos, const code_token& token) {
				return pos < token.origin.r.last;
			})
	};

	// rare case when binary search fails hard - semantic token is within one code token
	if (result.first > result.last)
		return {nullptr, nullptr};

	// Ugly corner cases because of splice
	// 1. the parser ignores leading splice but  parses trailing one
	// 2.     clangd  parses leading splice but ignores trailing one
	// leading: works by coincidence (clang reports n+1 column for leading splice, moving lower_bound 1 ahead)
	// trailing: is handled here - increase match length by 1 if code token ends with splice
	if (result.last != code_tokens.last && ends_with_backslash_whitespace(result.last->origin.str)) {
		++result.last;
	}

	return result;
}

std::optional<highlighter_error>
improve_code_tokens(
	std::string_view code,
	std::vector<code_token>& code_tokens,
	utility::range<const semantic_token*> sem_tokens)
{
	return semantic_token_processor(code).improve_code_tokens(code_tokens, sem_tokens);
}

std::variant<std::string, highlighter_error> highlighter::run(
	std::string_view code,
	utility::range<const semantic_token*> sem_tokens,
	highlighter_options options) const
{
	std::optional<highlighter_error> maybe_error =
		code_tokenizer(code, {m_keywords.data(), m_keywords.data() + m_keywords.size()})
		.fill_with_tokens(options.highlight_printf_formatting, m_code_tokens);

	if (maybe_error)
		return *maybe_error;

	maybe_error = improve_code_tokens(code, m_code_tokens, sem_tokens);

	if (maybe_error)
		return *maybe_error;

	m_builder.reset();
	// based on measuring mirror highlight which has very similar output size
	m_builder.reserve(code.size() * 5u);

	return generate_html(
		m_builder, m_code_tokens, text::count_lines(code), options.table_wrap_css_class, options.color_variants);
}

}
