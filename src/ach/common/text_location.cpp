#include <ach/common/text_location.hpp>

#include <ostream>

namespace ach {

std::ostream& operator<<(std::ostream& os, text_location tl)
{
	os << "line " << tl.line_number() << ":\n" << tl.line();

	if (tl.line().empty() || tl.line().back() != '\n')
		os << '\n';

	assert(tl.first_column() < tl.line().size());

	// match the whitespace character used - tabs have different length
	for (auto i = 0u; i < tl.first_column(); ++i) {
		if (tl.line()[i] == '\t')
			os << '\t';
		else
			os << ' ';
	}

	auto const highlight_length = tl.str().size();

	if (highlight_length == 0) {
		os << '^';
	}
	else {
		for (auto i = 0u; i < highlight_length; ++i)
			os << '~';
	}

	return os << '\n';
}

}
