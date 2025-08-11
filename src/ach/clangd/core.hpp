#pragma once

#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/code_token.hpp>
#include <ach/clangd/highlighter_error.hpp>
#include <ach/web/html_builder.hpp>
#include <ach/utility/range.hpp>

#include <string_view>
#include <string>
#include <variant>
#include <vector>

namespace ach::clangd {

struct highlighter_options
{
	std::string_view table_wrap_css_class = {};
	int color_variants = 6;
	bool highlight_printf_formatting = false;
};

class highlighter
{
public:
	highlighter(std::vector<std::string> keywords)
	: m_keywords(std::move(keywords))
	{}

	[[nodiscard]] std::variant<std::string, highlighter_error>
	run(
		std::string_view code,
		utility::range<const semantic_token*> sem_tokens,
		highlighter_options options = {}) const;

	std::size_t num_keywords() const { return m_keywords.size(); }
	std::size_t num_code_tokens() const { return m_code_tokens.size(); }

private:
	std::vector<std::string> m_keywords;
	// allocation reuse
	mutable std::vector<code_token> m_code_tokens;
	mutable web::html_builder m_builder;
};

[[nodiscard]] utility::range<code_token*>
find_matching_tokens(
	utility::range<code_token*> code_tokens,
	text::position start,
	text::position stop);

[[nodiscard]] std::optional<highlighter_error>
improve_code_tokens(
	std::string_view code,
	std::vector<code_token>& code_tokens,
	utility::range<const semantic_token*> sem_tokens);

}
