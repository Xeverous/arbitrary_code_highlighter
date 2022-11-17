#pragma once

#include <type_traits>
#include <utility>

namespace ach::utility {

template <typename F>
auto function_to_function_object(F f) noexcept
{
	return [=](auto&&... params) noexcept(noexcept(f(std::forward<std::decay_t<decltype(params)>>(params)...))) {
		return f(std::forward<std::decay_t<decltype(params)>>(params)...);
	};
}

}
