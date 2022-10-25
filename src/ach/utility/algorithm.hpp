#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>

namespace ach::utility {

// like std::search but the match must be a whole-word match
// the predicate determines whether a value is a part of the word
template <typename ForwardIterator1, typename ForwardIterator2, typename UnaryPredicate>
ForwardIterator1 search_whole_word(
	ForwardIterator1 first_input,
	ForwardIterator1 last_input,
	ForwardIterator2 first_pattern,
	ForwardIterator2 last_pattern,
	UnaryPredicate is_word_element)
{
	assert(std::all_of(first_pattern, last_pattern, is_word_element));

	// if the pattern is empty then return immediately a successful match
	// (the pattern must be non-empty to properly check word boundaries)
	if (first_pattern == last_pattern)
		return first_input;

	auto it = first_input;
	while (true) {
		it = std::search(it, last_input, first_pattern, last_pattern);
		if (it == last_input)
			return it; // no match

		// there is a match, check whether the next value is...
		auto next = it;
		std::advance(next, std::distance(first_pattern, last_pattern));

		// A) absent (success if input exhausted)
		if (next == last_input)
			return it;

		// B) part of a word (success if not a word element)
		if (!is_word_element(*next))
			return it;

		// failure: continue searching on the next word
		// advance it to the next word
		// if there are multiple non-word elements then initial search will skip them
		it = std::find_if_not(next, last_input, is_word_element);
	}
}

}
