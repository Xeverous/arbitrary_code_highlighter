#pragma once

#include <string_view>

namespace ach {

struct color_options
{
	static constexpr auto default_num_token_keyword = "num";

	static constexpr auto default_str_token_keyword = "str";
	static constexpr auto default_str_esc_token_keyword = "str_esc";

	static constexpr auto default_chr_token_keyword = "chr";
	static constexpr auto default_chr_esc_token_keyword = "chr_esc";

	static constexpr auto default_escape_char = '\\';
	static constexpr auto default_empty_token_char = '`';

	std::string_view num_token_keyword = default_num_token_keyword;

	std::string_view str_token_keyword = default_str_token_keyword;
	std::string_view str_esc_token_keyword = default_str_esc_token_keyword;
	char str_escape = default_escape_char;

	std::string_view chr_token_keyword = default_chr_token_keyword;
	std::string_view chr_esc_token_keyword = default_chr_esc_token_keyword;
	char chr_escape = default_escape_char;

	char empty_token_char = default_empty_token_char;
};

}
