#pragma once

#include <ach/text/types.hpp>
#include <ach/clangd/splice_utils.hpp>

#include <string_view>
#include <cassert>

namespace ach::clangd {

class spliced_text_iterator
{
public:
	spliced_text_iterator() {} // end iterator ctor

	spliced_text_iterator(std::string_view text, text::position pos = {})
	: m_text(text), m_position(pos)
	{
		remove_front_splices(m_text, m_position);
	}

	text::position position() const { return m_position; }
	std::string_view remaining_text() const { return m_text; }
	text::text_iterator to_text_iterator() const
	{
		return {m_text.data(), m_position};
	}

	char operator*() const
	{
		assert(!m_text.empty());
		return m_text.front();
	}

	spliced_text_iterator& operator++()
	{
		assert(!m_text.empty());
		assert(front_splice_length(m_text) == 0);

		m_position.next(m_text.front());
		m_text.remove_prefix(1u);

		remove_front_splices(m_text, m_position);

		return *this;
	}

	spliced_text_iterator operator++(int)
	{
		auto old = *this;
		operator++();
		return old;
	}

private:
	friend bool operator==(spliced_text_iterator lhs, spliced_text_iterator rhs);

	std::string_view m_text;
	text::position m_position;
};

inline bool operator==(spliced_text_iterator lhs, spliced_text_iterator rhs)
{
	return lhs.m_text == rhs.m_text;
}
inline bool operator!=(spliced_text_iterator lhs, spliced_text_iterator rhs)
{
	return !(lhs == rhs);
}

inline std::string_view str_from_range(spliced_text_iterator first, spliced_text_iterator last)
{
	if (first == spliced_text_iterator()) {
		assert(last == spliced_text_iterator());
		return {};
	}

	if (last == spliced_text_iterator())
		return first.remaining_text();

	auto distance = last.remaining_text().data() - first.remaining_text().data();
	if (distance <= 0)
		return {};

	return std::string_view(first.remaining_text().data(), distance);
}

}
