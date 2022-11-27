#pragma once

#include <cassert>
#include <string_view>
#include <tuple>
#include <ostream>

namespace ach::text {

struct position
{
	std::size_t line = 0;
	std::size_t column = 0;

	constexpr void next_line()
	{
		++line;
		column = 0;
	}

	constexpr void next(char current)
	{
		if (current == '\n')
			next_line();
		else
			++column;
	}
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

inline std::ostream& operator<<(std::ostream& os, position pos)
{
	// index is 0-based, add 1 for human output
	return os << "line: " << pos.line + 1 << ", column: " << pos.column + 1;
}

struct span
{
	position start_position() const noexcept
	{
		return position{line, column};
	}

	position end_position() const noexcept
	{
		return position{line, column + length};
	}

	[[nodiscard]] span extend(std::size_t extra_length) const noexcept
	{
		return span{line, column, length + extra_length};
	}

	std::size_t line = 0;
	std::size_t column = 0;
	std::size_t length = 0;
};

class located_span
{
public:
	located_span(std::string_view whole_line, span s)
	: m_whole_line(whole_line), m_span(s)
	{
		assert(s.column + s.length <= whole_line.size());
	}

	located_span(std::string_view whole_line, std::size_t line_number, std::size_t column, std::size_t length)
	: located_span(whole_line, text::span{line_number, column, length}) {}

	std::string_view whole_line() const noexcept { return m_whole_line; }
	struct span span() const noexcept { return m_span; }
	bool is_empty() const noexcept { return m_span.length == 0; }

	std::string_view str() const noexcept {
		if (m_span.length == 0)
			return std::string_view();

		return std::string_view(m_whole_line.data() + m_span.column, m_span.length);
	}

	static located_span merge(located_span lhs, located_span rhs) noexcept {
		assert(lhs.whole_line() == rhs.whole_line());
		assert(lhs.m_span.line == rhs.m_span.line);
		assert(lhs.m_span.column + lhs.m_span.length == rhs.m_span.column);
		return located_span(lhs.whole_line(), lhs.m_span.extend(rhs.m_span.length));
	}

private:
	std::string_view m_whole_line;
	struct span m_span;
};

struct range
{
	position first;
	position last;

	bool empty() const
	{
		return first == last;
	}
};

constexpr bool operator==(range lhs, range rhs)
{
	return lhs.first == rhs.first && lhs.last == rhs.last;
}

constexpr bool operator!=(range lhs, range rhs)
{
	return !(lhs == rhs);
}

struct fragment
{
	std::string_view str;
	range r;

	bool empty() const
	{
		assert(str.empty() == r.empty());
		return r.empty();
	}
};

constexpr bool operator==(fragment lhs, fragment rhs)
{
	return lhs.str == rhs.str && lhs.r == rhs.r;
}

constexpr bool operator!=(fragment lhs, fragment rhs)
{
	return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, fragment frag)
{
	return os << "range: [[" << frag.r.first << "], [" << frag.r.last << "]]\n"
		"str: \"" << frag.str << "\"\n";
}

class text_iterator
{
public:
	constexpr text_iterator(const char* ptr, position pos)
	: m_it(ptr), m_position(pos)
	{}

	constexpr const char* pointer() const { return m_it; }
	constexpr text::position position() const { return m_position; }

	constexpr char operator*() const
	{
		return *m_it;
	}

	constexpr text_iterator& operator++()
	{
		m_position.next(*m_it);
		++m_it;
		return *this;
	}

	constexpr text_iterator operator++(int)
	{
		auto old = *this;
		operator++();
		return old;
	}

private:
	friend constexpr bool operator==(text_iterator lhs, text_iterator rhs);

	const char* m_it;
	text::position m_position;
};

constexpr bool operator==(text_iterator lhs, text_iterator rhs)
{
	return lhs.m_it == rhs.m_it;
}
constexpr bool operator!=(text_iterator lhs, text_iterator rhs)
{
	return !(lhs == rhs);
}

}
