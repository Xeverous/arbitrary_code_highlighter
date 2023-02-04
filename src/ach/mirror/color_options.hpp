#pragma once

#include <string_view>

namespace ach::mirror {

struct color_options
{
	static constexpr auto default_num_keyword = "num";
	static constexpr auto default_str_keyword = "str";
	static constexpr auto default_chr_keyword = "chr";

	static constexpr auto default_num_class = "lit_num";

	static constexpr auto default_str_class = "lit_str";
	static constexpr auto default_str_esc_class = "esc_seq";

	static constexpr auto default_chr_class = "lit_chr";
	static constexpr auto default_chr_esc_class = default_str_esc_class;

	static constexpr auto default_escape_char = '\\';
	static constexpr auto default_empty_token_char = '`';

	std::string_view num_keyword = default_num_keyword;
	std::string_view str_keyword = default_str_keyword;
	std::string_view chr_keyword = default_chr_keyword;

	std::string_view num_class = default_num_class;

	std::string_view str_class = default_str_class;
	std::string_view str_esc_class = default_str_esc_class;

	std::string_view chr_class = default_chr_class;
	std::string_view chr_esc_class = default_chr_esc_class;

	char escape_char = default_escape_char;
	char empty_token_char = default_empty_token_char;
};

}
