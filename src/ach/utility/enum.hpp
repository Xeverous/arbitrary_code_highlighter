#pragma once

#include <enum.hpp/enum.hpp>

#define ACH_RICH_ENUM_CLASS(type_name, values) \
	ENUM_HPP_CLASS_DECL(type_name, char, values) \
	ENUM_HPP_REGISTER_TRAITS(type_name) \
	[[maybe_unused]] void noop()

namespace ach::utility {

template <typename Enum>
constexpr std::string_view to_string(Enum e) noexcept
{
	return enum_hpp::to_string(e).value_or("(unknown)");
}

}
