#pragma once

#include <string_view>

namespace ach::cc {

struct color_options
{
	static constexpr auto default_num_class = "num";

	static constexpr auto default_str_class = "str";
	static constexpr auto default_str_esc_class = "str_esc";

	static constexpr auto default_chr_class = "chr";
	static constexpr auto default_chr_esc_class = "chr_esc";

	static constexpr auto default_escape_char = '\\';
	static constexpr auto default_empty_token_char = '`';

	std::string_view num_class = default_num_class;

	std::string_view str_class = default_str_class;
	std::string_view str_esc_class = default_str_esc_class;

	std::string_view chr_class = default_chr_class;
	std::string_view chr_esc_class = default_chr_esc_class;

	char escape_char = default_escape_char;
	char empty_token_char = default_empty_token_char;
};

}
