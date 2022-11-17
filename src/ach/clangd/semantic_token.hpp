#pragma once

#include <ach/text/types.hpp>

namespace ach::clangd {

/**
 * LSP spec: each semantic token has 1 type and 0+ modifiers.
 * LSP uses bitflags to represent modifiers; modifers themselves are defined by the server.
 * Here a more constrained format is used for better representation of mutually exclusive flags.
 *
 * Definitions based on clangd-15.0.2 LSP "initialize" call, reordered for clarity.
 * Comments mention flag names that differ from names in clangd output.
 */

enum class semantic_token_type {
	// values
	parameter,
	variable,
	property,
	enum_member,

	// functions
	function,
	method,

	// types
	class_,
	interface,
	enum_,
	type,

	// templates
	concept_,
	// the LSP defines "typeParameter" semantic token type, clangd uses it to
	// report both type parameters and non-type template parameters (NTTP)
	template_parameter,

	namespace_,

	// the LSP defines "comment" semantic token type but
	// clangd uses it to mark preprocessor-disabled code, not comments
	disabled_code,

	// preprocessor
	macro,

	unknown
};

// "functionScope", "classScope", "fileScope", "globalScope"
// + none because not everything (e.g. template parameters and disabled code) has any of these flags on
enum class semantic_token_scope_modifier { none, function, class_, file, global };

struct semantic_token_modifiers {
	bool is_declaration    = false; // "declaration"
	bool is_deprecated     = false; // "deprecated"
	bool is_deduced        = false; // "deduced"
	bool is_readonly       = false; // "readonly" TODO this might not be 1:1 with const - verify
	bool is_static         = false; // "static"
	bool is_abstract       = false; // "abstract"
	bool is_virtual        = false; // "virtual"
	bool is_dependent_name = false; // "dependentName"
	bool is_from_std_lib   = false; // "defaultLibrary"
	bool is_out_parameter  = false; // "usedAsMutableReference"
	semantic_token_scope_modifier scope = semantic_token_scope_modifier::none;
};

constexpr bool operator==(semantic_token_modifiers lhs, semantic_token_modifiers rhs)
{
	return
		lhs.is_declaration == rhs.is_declaration &&
		lhs.is_deprecated == rhs.is_deprecated &&
		lhs.is_deduced == rhs.is_deduced &&
		lhs.is_readonly == rhs.is_readonly &&
		lhs.is_static == rhs.is_static &&
		lhs.is_abstract == rhs.is_abstract &&
		lhs.is_virtual == rhs.is_virtual &&
		lhs.is_dependent_name == rhs.is_dependent_name &&
		lhs.is_from_std_lib == rhs.is_from_std_lib &&
		lhs.is_out_parameter == rhs.is_out_parameter &&
		lhs.scope == rhs.scope;
}

constexpr bool operator!=(semantic_token_modifiers lhs, semantic_token_modifiers rhs)
{
	return !(lhs == rhs);
}

// fields from LSP specification (token type/mods restricted for clangd implementation)
struct semantic_token_info
{
	semantic_token_type type = semantic_token_type::unknown;
	semantic_token_modifiers modifers = {};
};

constexpr bool operator==(semantic_token_info lhs, semantic_token_info rhs)
{
	return lhs.type == rhs.type && lhs.modifers == rhs.modifers;
}

constexpr bool operator!=(semantic_token_info lhs, semantic_token_info rhs)
{
	return !(lhs == rhs);
}

// extension for object-specific color variant highlight
// (e.g. different shade of "parameter color" for different function parameters)
struct semantic_token_color_variance
{
	int color_variant = 0;
	bool last_reference = false;
};

struct semantic_token
{
	text::position pos = {};
	std::size_t length = 0;
	semantic_token_info info = {};
	semantic_token_color_variance color_variance;

	text::position pos_begin() const
	{
		return pos;
	}

	text::position pos_end() const
	{
		text::position result = pos;
		result.column += length;
		return result;
	}
};

}
