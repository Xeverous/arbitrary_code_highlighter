#pragma once

#include <string_view>

namespace ach {

struct color_options
{
	static constexpr auto default_num_token_keyword = "num";
	static constexpr auto default_str_token_keyword = "str";
	static constexpr auto default_chr_token_keyword = "chr";

	std::string_view num_token_keyword = default_num_token_keyword;
	std::string_view str_token_keyword = default_str_token_keyword;
	std::string_view chr_token_keyword = default_chr_token_keyword;
};

}
