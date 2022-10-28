#pragma once

#include <cassert>
#include <string_view>
#include <tuple>

namespace ach::text {

struct position
{
	std::size_t line = 0;
	std::size_t column = 0;
};

constexpr bool operator==(position lhs, position rhs) noexcept
{
	return lhs.line == rhs.line && lhs.column == rhs.column;
}

constexpr bool operator!=(position lhs, position rhs) noexcept
{
	return !(lhs == rhs);
}

constexpr bool operator<(position lhs, position rhs) noexcept
{
	return std::tie(lhs.line, lhs.column) < std::tie(rhs.line, rhs.column);
}

constexpr bool operator>(position lhs, position rhs) noexcept
{
	return std::tie(lhs.line, lhs.column) > std::tie(rhs.line, rhs.column);
}

constexpr bool operator<=(position lhs, position rhs) noexcept
{
	return std::tie(lhs.line, lhs.column) <= std::tie(rhs.line, rhs.column);
}

constexpr bool operator>=(position lhs, position rhs) noexcept
{
	return std::tie(lhs.line, lhs.column) >= std::tie(rhs.line, rhs.column);
}

struct range
{
	position start_position() const noexcept
	{
		return position{line, column};
	}

	position end_position() const noexcept
	{
		return position{line, column + length};
	}

	[[nodiscard]] range extend(std::size_t extra_length) const noexcept
	{
		return range{line, column, length + extra_length};
	}

	std::size_t line = 0;
	std::size_t column = 0;
	std::size_t length = 0;
};

class location
{
public:
	location(std::string_view whole_line, range range)
	: m_whole_line(whole_line), m_range(range)
	{
		assert(range.column + range.length <= whole_line.size());
	}

	location(std::string_view whole_line, std::size_t line_number, std::size_t column, std::size_t length)
	: location(whole_line, text::range{line_number, column, length}) {}

	std::string_view whole_line() const noexcept { return m_whole_line; }
	struct range range() const noexcept { return m_range; }
	bool is_empty() const noexcept { return m_range.length == 0; }

	std::string_view str() const noexcept {
		if (m_range.length == 0)
			return std::string_view();

		return std::string_view(m_whole_line.data() + m_range.column, m_range.length);
	}

	static location merge(location lhs, location rhs) noexcept {
		assert(lhs.whole_line() == rhs.whole_line());
		assert(lhs.m_range.line == rhs.m_range.line);
		assert(lhs.m_range.column + lhs.m_range.length == rhs.m_range.column);
		return location(lhs.whole_line(), lhs.m_range.extend(rhs.m_range.length));
	}

private:
	std::string_view m_whole_line;
	struct range m_range;
};

}
