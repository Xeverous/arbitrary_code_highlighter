#pragma once

#include <iterator>

namespace ach::utility {

template <typename Iterator>
struct range
{
	constexpr bool empty() const { return first == last; }
	constexpr Iterator begin() const { return first; }
	constexpr Iterator end() const { return last; }
	constexpr auto size() const { return last - first; }

	constexpr auto& front() const { return *first; }
	constexpr auto& back() const { return *std::prev(last); }

	Iterator first;
	Iterator last;
};

template <typename Iterator>
constexpr bool operator==(range<Iterator> lhs, range<Iterator> rhs)
{
	return lhs.first == rhs.first && lhs.last == rhs.last;
}

template <typename Iterator>
constexpr bool operator!=(range<Iterator> lhs, range<Iterator> rhs)
{
	return !(lhs == rhs);
}

}
