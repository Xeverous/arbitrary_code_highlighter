#pragma once

#include <ach/text/types.hpp>
#include <ach/utility/enum.hpp>

#include <iomanip>
#include <string_view>
#include <optional>
#include <ostream>
#include <vector>

namespace ach::clangd {

/**
 * LSP spec: each semantic token has 1 type and 0+ modifiers.
 * LSP uses bitflags to represent modifiers; modifers themselves are defined by the server.
 * Here a more constrained format is used for better representation of mutually exclusive flags.
 *
 * Definitions based on clangd-20.1.8 LSP "initialize" call, reordered for clarity.
 * Comments mention flag names that differ from names in clangd output.
 */

ACH_RICH_ENUM_CLASS(semantic_token_type,
	// values
	(parameter)
	(variable)
	(property)
	(enum_member)

	// functions
	(function)
	(method)

	// types
	(class_)
	(interface)
	(enum_)
	(type)

	// templates
	(concept_)
	// the LSP defines "typeParameter" semantic token type) clangd uses it to
	// report both type parameters and non-type template parameters (NTTP)
	(template_parameter)

	(namespace_)

	// the LSP defines "comment" semantic token type but
	// clangd uses it to mark preprocessor-disabled code, not comments
	(disabled_code)

	// preprocessor
	(macro)

	(modifier) // override and final when used as intended

	(operator_)
	(bracket) // non-operator and non-preprocessor use of < > (e.g. templates)

	(label)

	(unknown)
);

inline std::optional<semantic_token_type> parse_semantic_token_type(std::string_view name)
{
	using stt = semantic_token_type;

	if (name == "variable")
		return stt::variable;
	else if (name == "parameter")
		return stt::parameter;
	else if (name == "function")
		return stt::function;
	else if (name == "method")
		return stt::method;
	else if (name == "property")
		return stt::property;
	else if (name == "class")
		return stt::class_;
	else if (name == "interface")
		return stt::interface;
	else if (name == "enum")
		return stt::enum_;
	else if (name == "enumMember")
		return stt::enum_member;
	else if (name == "type")
		return stt::type;
	else if (name == "unknown")
		return stt::unknown;
	else if (name == "namespace")
		return stt::namespace_;
	else if (name == "typeParameter")
		return stt::template_parameter;
	else if (name == "concept")
		return stt::concept_;
	else if (name == "macro")
		return stt::macro;
	else if (name == "modifier")
		return stt::modifier;
	else if (name == "operator")
		return stt::operator_;
	else if (name == "bracket")
		return stt::bracket;
	else if (name == "label")
		return stt::label;
	else if (name == "comment")
		return stt::disabled_code;
	else
		return std::nullopt;
}

// "functionScope", "classScope", "fileScope", "globalScope"
// + none because not everything (e.g. template parameters and disabled code) has any of these flags on
ACH_RICH_ENUM_CLASS(semantic_token_scope_modifier, (none)(function)(class_)(file)(global));

struct semantic_token_modifiers {
	semantic_token_modifiers& declaration(bool state = true)
	{
		is_declaration = state;
		return *this;
	}

	semantic_token_modifiers& definition(bool state = true)
	{
		is_definition = state;
		return *this;
	}

	semantic_token_modifiers& deprecated(bool state = true)
	{
		is_deprecated = state;
		return *this;
	}

	semantic_token_modifiers& deduced(bool state = true)
	{
		is_deduced = state;
		return *this;
	}

	semantic_token_modifiers& readonly(bool state = true)
	{
		is_readonly = state;
		return *this;
	}

	semantic_token_modifiers& static_(bool state = true)
	{
		is_static = state;
		return *this;
	}

	semantic_token_modifiers& abstract(bool state = true)
	{
		is_abstract = state;
		return *this;
	}

	semantic_token_modifiers& virtual_(bool state = true)
	{
		is_virtual = state;
		return *this;
	}

	semantic_token_modifiers& dependent_name(bool state = true)
	{
		is_dependent_name = state;
		return *this;
	}

	semantic_token_modifiers& from_std_lib(bool state = true)
	{
		is_from_std_lib = state;
		return *this;
	}

	semantic_token_modifiers& non_const_ref_parameter(bool state = true)
	{
		is_non_const_ref_parameter = state;
		return *this;
	}

	semantic_token_modifiers& non_const_ptr_parameter(bool state = true)
	{
		is_non_const_ptr_parameter = state;
		return *this;
	}

	semantic_token_modifiers& ctor_or_dtor(bool state = true)
	{
		is_ctor_or_dtor = state;
		return *this;
	}

	semantic_token_modifiers& user_defined(bool state = true)
	{
		is_user_defined = state;
		return *this;
	}

	semantic_token_modifiers& scope_function()
	{
		scope = semantic_token_scope_modifier::function;
		return *this;
	}

	semantic_token_modifiers& scope_class()
	{
		scope = semantic_token_scope_modifier::class_;
		return *this;
	}

	semantic_token_modifiers& scope_file()
	{
		scope = semantic_token_scope_modifier::file;
		return *this;
	}

	semantic_token_modifiers& scope_global()
	{
		scope = semantic_token_scope_modifier::global;
		return *this;
	}

	bool is_declaration    = false; // "declaration"
	bool is_definition     = false; // "definition"
	bool is_deprecated     = false; // "deprecated"
	bool is_deduced        = false; // "deduced"
	bool is_readonly       = false; // "readonly"
	bool is_static         = false; // "static"
	bool is_abstract       = false; // "abstract"
	bool is_virtual        = false; // "virtual"
	bool is_dependent_name = false; // "dependentName"
	bool is_from_std_lib   = false; // "defaultLibrary"
	bool is_non_const_ref_parameter = false; // "usedAsMutableReference"
	bool is_non_const_ptr_parameter = false; // "usedAsMutablePointer"
	bool is_ctor_or_dtor   = false; // "constructorOrDestructor"
	bool is_user_defined   = false; // "userDefined"
	semantic_token_scope_modifier scope = semantic_token_scope_modifier::none;
};

constexpr bool operator==(semantic_token_modifiers lhs, semantic_token_modifiers rhs)
{
	return
		lhs.is_declaration == rhs.is_declaration &&
		lhs.is_definition == rhs.is_definition &&
		lhs.is_deprecated == rhs.is_deprecated &&
		lhs.is_deduced == rhs.is_deduced &&
		lhs.is_readonly == rhs.is_readonly &&
		lhs.is_static == rhs.is_static &&
		lhs.is_abstract == rhs.is_abstract &&
		lhs.is_virtual == rhs.is_virtual &&
		lhs.is_dependent_name == rhs.is_dependent_name &&
		lhs.is_from_std_lib == rhs.is_from_std_lib &&
		lhs.is_non_const_ref_parameter == rhs.is_non_const_ref_parameter &&
		lhs.is_non_const_ptr_parameter == rhs.is_non_const_ptr_parameter &&
		lhs.is_ctor_or_dtor == rhs.is_ctor_or_dtor &&
		lhs.is_user_defined == rhs.is_user_defined &&
		lhs.scope == rhs.scope;
}

constexpr bool operator!=(semantic_token_modifiers lhs, semantic_token_modifiers rhs)
{
	return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, semantic_token_modifiers token_modifiers)
{
	os << "scope: " << std::setw(8) << utility::to_string(token_modifiers.scope) << " | modifiers: ";

	if (token_modifiers.is_declaration)
		os << "declaration, ";

	if (token_modifiers.is_definition)
		os << "definition, ";

	if (token_modifiers.is_deprecated)
		os << "deprecated, ";

	if (token_modifiers.is_deduced)
		os << "deduced, ";

	if (token_modifiers.is_readonly)
		os << "readonly, ";

	if (token_modifiers.is_static)
		os << "static, ";

	if (token_modifiers.is_abstract)
		os << "abstract, ";

	if (token_modifiers.is_virtual)
		os << "virtual, ";

	if (token_modifiers.is_dependent_name)
		os << "dependent_name, ";

	if (token_modifiers.is_from_std_lib)
		os << "from_std_lib, ";

	if (token_modifiers.is_non_const_ref_parameter)
		os << "non_const_ref_parameter, ";

	if (token_modifiers.is_non_const_ptr_parameter)
		os << "non_const_ptr_parameter, ";

	if (token_modifiers.is_ctor_or_dtor)
		os << "ctor_or_dtor, ";

	if (token_modifiers.is_user_defined)
		os << "user_defined, ";

	return os;
}

// Return a function pointer instead of a value because tokens can have multiple modifiers.
// This makes it easy to apply multiple modifications to a default-constructed
// semantic_token_modifiers object (whch has no modifiers set).
using apply_semantic_token_modifier_f = void (semantic_token_modifiers&);
inline apply_semantic_token_modifier_f* parse_semantic_token_modifier(std::string_view name)
{
	using stm = semantic_token_modifiers;
	using stsm = semantic_token_scope_modifier;

	if (name == "declaration")
		return +[](stm& m) { m.is_declaration = true; };
	else if (name == "definition")
		return +[](stm& m) { m.is_definition = true; };
	else if (name == "deprecated")
		return +[](stm& m) { m.is_deprecated = true; };
	else if (name == "deduced")
		return +[](stm& m) { m.is_deduced = true; };
	else if (name == "readonly")
		return +[](stm& m) { m.is_readonly = true; };
	else if (name == "static")
		return +[](stm& m) { m.is_static = true; };
	else if (name == "abstract")
		return +[](stm& m) { m.is_abstract = true; };
	else if (name == "virtual")
		return +[](stm& m) { m.is_virtual = true; };
	else if (name == "dependentName")
		return +[](stm& m) { m.is_dependent_name = true; };
	else if (name == "defaultLibrary")
		return +[](stm& m) { m.is_from_std_lib = true; };
	else if (name == "usedAsMutableReference")
		return +[](stm& m) { m.is_non_const_ref_parameter = true; };
	else if (name == "usedAsMutablePointer")
		return +[](stm& m) { m.is_non_const_ptr_parameter = true; };
	else if (name == "constructorOrDestructor")
		return +[](stm& m) { m.is_ctor_or_dtor = true; };
	else if (name == "userDefined")
		return +[](stm& m) { m.is_user_defined = true; };
	else if (name == "functionScope")
		return +[](stm& m) { m.scope = stsm::function; };
	else if (name == "classScope")
		return +[](stm& m) { m.scope = stsm::class_; };
	else if (name == "fileScope")
		return +[](stm& m) { m.scope = stsm::file; };
	else if (name == "globalScope")
		return +[](stm& m) { m.scope = stsm::global; };
	else
		return nullptr;
}

// fields from LSP specification (token type/mods restricted for clangd implementation)
struct semantic_token_info
{
	semantic_token_type type = semantic_token_type::unknown;
	semantic_token_modifiers modifers = {};
};

inline std::ostream& operator<<(std::ostream& os, semantic_token_info info)
{
	return os << "type: " << std::setw(18) << utility::to_string(info.type) << " | " << info.modifers;
}

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

constexpr bool operator==(semantic_token_color_variance lhs, semantic_token_color_variance rhs)
{
	return lhs.color_variant == rhs.color_variant && lhs.last_reference == rhs.last_reference;
}

constexpr bool operator!=(semantic_token_color_variance lhs, semantic_token_color_variance rhs)
{
	return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, semantic_token_color_variance cv)
{
	return os << cv.color_variant << (cv.last_reference ? " last" : "");
}

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

inline std::ostream& operator<<(std::ostream& os, semantic_token token)
{
	return os << "[" << token.pos << ", len: " << std::setw(2) << token.length << "] "
		<< token.info << "cv: " << token.color_variance << "\n";
}

struct semantic_token_decoder
{
	std::optional<semantic_token_info> decode_semantic_token(std::size_t type, std::size_t modifiers) const
	{
		if (type >= token_types.size())
			return std::nullopt;

		semantic_token_modifiers mods;
		for (std::size_t i = 0; i < token_modifiers.size(); ++i)
			if (((1u << i) & modifiers) != 0)
				token_modifiers[i](mods);

		return semantic_token_info{token_types[type], mods};
	}

	std::vector<semantic_token_type> token_types;
	std::vector<apply_semantic_token_modifier_f*> token_modifiers;
};

}
