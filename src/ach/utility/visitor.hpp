#pragma once

namespace ach::utility {

template<typename... Ts>
struct visitor : Ts...
{
	using Ts::operator()...;
};

template<typename... Ts>
visitor(Ts...) -> visitor<Ts...>;

}
