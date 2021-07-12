#pragma once

#include <ach/common/text_extractor.hpp>

#include <string_view>
#include <optional>
#include <queue>

namespace ach::ce::detail {

/*

A bad-printf.cc:6:19: warning: format '%ld' expects argument of type 'long int', but argument 4 has type 'double' [-Wformat=]
C    6 |   printf ("%s: %*ld ", fieldname, column - width, value);
U      |                ~~~^                               ~~~~~
S      |                   |                               |
H      |                   long int                        double
P      |                %*f

A operator.cc: In member function 'boxed_ptr& boxed_ptr::operator=(const boxed_ptr&)':
A operator.cc:7:3: warning: no return statement in function returning non-void [-Wreturn-type]
C    6 |     m_ptr = other.m_ptr;
D  +++ |+    return *this;
C    7 |   }
U      |   ^

A - admonitiom
C - code
U - underline
S - separator
H - hint
P - proposition
D - diff

*/

enum class line_classification
{
	// In file included from path/to/file,
	//                  from path/to/file,
	//                  from path/to/file:
	include,

	// /usr/include/c++/8/bits/stl_iterator.h:392:5: note:   template argument deduction/substitution failed:
	// /usr/include/c++/8/bits/stl_algo.h:1969:22: note:   ‘std::_List_iterator<int>’ is not derived from ‘const std::move_iterator<_IteratorL>’
	// [...]: warning: [...]
	// [...]: error: [...]
	admonition,

	//           return width * height;
	//    14 |   return width * height; // (GCC 9+ outputs line number)
	code,

	//                        ~^~~
	//                        ~^~~               ~~~~~~~~~~~~~~
	//       |                ~^~~               ~~~~~~~~~~~~~~ // (with line numbering)
	underline,

	//                         |                        |
	//       |                 |                        | // (with line numbering)
	separator,

	//                         long int                 double
	//       |                 long int                 double // (with line numbering)
	hint,

	//                        %*f
	//       |                %*f // (with line numbering)
	proposition,

	//   +++ |+    return *this;
	diff
};

enum class admonition_type { none, note, warning, error };

struct underline
{
	explicit operator bool() const noexcept { return first_column != 0 && length != 0; }

	std::size_t first_column = 0;
	std::size_t length = 0;
};

struct underline_info
{
	underline main;
	std::size_t main_pivot;
	underline extra1;
	underline extra2;
};

enum class token_color
{
	normal,
	highlight,
	note,
	warning,
	error,
	extra_highlight1,
	extra_highlight2
};

struct gcc_token
{
	text_location location;
	token_color color;
};

struct token_error
{
	text_location location;
	std::string_view description;
};

class gcc_tokenizer
{
public:
	gcc_tokenizer(std::string_view text)
	: _extractor(text) {}

	std::optional<gcc_token> next_token()
	{
		if (_tokens.empty())
			parse_more_tokens();

		if (_tokens.empty())
			return std::nullopt;

		gcc_token token = _tokens.front();
		_tokens.pop();
		return token;
	}

	[[nodiscard]] bool has_reached_end() const noexcept { return _extractor.has_reached_end(); }
	text_location current_location() const noexcept { return _extractor.current_location(); }
	std::optional<token_error> error() const noexcept { return _error; }

private:
	void parse_more_tokens();

	text_extractor _extractor;
	std::queue<gcc_token> _tokens;
	std::optional<line_classification> _previous_line_classification;
	admonition_type _last_admonition_type = admonition_type::none;
	std::optional<text_location> _pending_code_line;
	std::optional<text_location> _pending_underline_line;
	std::optional<std::size_t> _line_numbering_length;
	std::optional<token_error> _error;
};

}
