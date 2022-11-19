#include "ach/clangd/highlighter_error.hpp"
#include <ach/mirror/color_options.hpp>
#include <ach/mirror/core.hpp>
#include <ach/clangd/semantic_token.hpp>
#include <ach/clangd/core.hpp>
#include <ach/utility/version.hpp>
#include <ach/utility/visitor.hpp>

#include <pybind11/pybind11.h>

#include <stdexcept>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace py = pybind11;

namespace {

std::string to_string(const ach::mirror::highlighter_error& error)
{
	std::stringstream ss;
	ss << error;
	return ss.str();
}

py::str run_mirror_highlighter(
	// required arguments
	std::string_view code, std::string_view color,
	// optional, keyword arguments
	std::string_view num_class,
	std::string_view str_class, std::string_view str_esc_class,
	std::string_view chr_class, std::string_view chr_esc_class,
	std::string_view escape_char, std::string_view empty_token_char,
	std::string_view table_wrap_css_class,
	std::string_view valid_css_classes,
	bool replace_underscores_to_hyphens)
{
	if (escape_char.size() != 1u) {
		throw py::value_error("argument 'escape_char' should be 1 character");
	}

	if (empty_token_char.size() != 1u) {
		throw py::value_error("argument 'empty_token_char' should be 1 character");
	}

	const auto result = ach::mirror::run_highlighter(
		code,
		color,
		ach::mirror::highlighter_options{
			ach::mirror::generation_options{
				replace_underscores_to_hyphens,
				table_wrap_css_class,
				valid_css_classes
			},
			ach::mirror::color_options{
				num_class,
				str_class, str_esc_class,
				chr_class, chr_esc_class,
				escape_char.front(),
				empty_token_char.front()
			}
		});

	return std::visit(ach::utility::visitor{
		[](const std::string& output) {
			return py::str(output);
		},
		[](ach::mirror::highlighter_error error) -> py::str {
			throw std::runtime_error(to_string(error));
		}
	}, result);
}

ach::clangd::semantic_token_type parse_semantic_token_type(std::string_view name)
{
	using stt = ach::clangd::semantic_token_type;

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
	else if (name == "comment")
		return stt::disabled_code;

	throw py::value_error(std::string("unknown SemanticToken type: ").append(name).c_str());
}

std::vector<ach::clangd::semantic_token_type> parse_semantic_token_types(const py::list& list_semantic_token_types)
{
	std::vector<ach::clangd::semantic_token_type> result;
	result.reserve(list_semantic_token_types.size());

	for (const auto& token_type : list_semantic_token_types)
		result.push_back(parse_semantic_token_type(token_type.cast<std::string_view>()));

	return result;
}

using apply_token_mod_f = void (ach::clangd::semantic_token_modifiers&);
apply_token_mod_f* parse_semantic_token_modifier(std::string_view name)
{
	using stm = ach::clangd::semantic_token_modifiers;
	using stsm = ach::clangd::semantic_token_scope_modifier;

	if (name == "declaration")
		return +[](stm& m) { m.is_declaration = true; };
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
		return +[](stm& m) { m.is_out_parameter = true; };
	else if (name == "functionScope")
		return +[](stm& m) { m.scope = stsm::function; };
	else if (name == "classScope")
		return +[](stm& m) { m.scope = stsm::class_; };
	else if (name == "fileScope")
		return +[](stm& m) { m.scope = stsm::file; };
	else if (name == "globalScope")
		return +[](stm& m) { m.scope = stsm::global; };

	throw py::value_error(std::string("unknown SemanticToken modifier: ").append(name).c_str());
}

std::vector<apply_token_mod_f*> parse_semantic_token_modifiers(const py::list& list_semantic_token_modifiers)
{
	std::vector<apply_token_mod_f*> result;
	result.reserve(list_semantic_token_modifiers.size());

	for (const auto& token_modifier : list_semantic_token_modifiers)
		result.push_back(parse_semantic_token_modifier(token_modifier.cast<std::string_view>()));

	return result;
}

ach::clangd::semantic_token_info get_semantic_token_info(
	const std::vector<ach::clangd::semantic_token_type>& semantic_token_types,
	const std::vector<apply_token_mod_f*>& semantic_token_modifiers,
	const pybind11::handle& semantic_token)
{
	const auto token_type = semantic_token.attr("token_type").cast<std::size_t>();

	if (token_type >= semantic_token_types.size()) {
		throw py::value_error(("SemanticToken has token_type " + std::to_string(token_type) + " but only "
			+ std::to_string(semantic_token_types.size()) + " token types were reported!").c_str());
	}

	const auto token_modifiers = semantic_token.attr("token_modifiers").cast<std::size_t>();

	ach::clangd::semantic_token_modifiers mods;
	for (std::size_t i = 0; i < semantic_token_modifiers.size(); ++i)
		if (((1u << i) & token_modifiers) != 0)
			semantic_token_modifiers[i](mods);

	return ach::clangd::semantic_token_info{semantic_token_types[token_type], mods};
}

std::vector<std::string> parse_keywords(const py::list& list_keywords)
{
	std::vector<std::string> result;
	result.reserve(list_keywords.size());

	for (const auto& obj : list_keywords)
		result.push_back(obj.cast<std::string>());

	return result;
}

std::string to_string(const ach::clangd::highlighter_error& error)
{
	std::stringstream ss;
	ss << "[line: " << error.pos.line << ", column: " << error.pos.column << "]"
		" error: " << ach::clangd::to_string(error.reason) << "\n";

	if (!error.last_semantic_tokens.empty())
		ss << "last semantic tokens:\n";

	for (ach::clangd::semantic_token token : error.last_semantic_tokens) {
		ss << "\t[line: " << token.pos.line
			<< ", column: " << token.pos.column
			<< ", length" << token.length << "]\n";
	}

	return ss.str();
}

py::str run_clangd_highlighter(
	std::string_view code,
	const py::list& list_semantic_token_types,
	const py::list& list_semantic_token_modifiers,
	const py::list& list_semantic_tokens,
	const py::list& list_keywords,
	std::string_view table_wrap_css_class,
	int color_variants)
{
	const std::vector<ach::clangd::semantic_token_type> semantic_token_types =
		parse_semantic_token_types(list_semantic_token_types);

	const std::vector<apply_token_mod_f*> semantic_token_modifiers =
		parse_semantic_token_modifiers(list_semantic_token_modifiers);

	const std::vector<std::string> keywords = parse_keywords(list_keywords);

	std::vector<ach::clangd::semantic_token> semantic_tokens;
	semantic_tokens.reserve(list_semantic_tokens.size());

	for (const auto& token : list_semantic_tokens) {
		semantic_tokens.push_back(ach::clangd::semantic_token{
			ach::text::position{
				token.attr("line").cast<std::size_t>(),
				token.attr("column").cast<std::size_t>()
			},
			token.attr("length").cast<std::size_t>(),
			get_semantic_token_info(semantic_token_types, semantic_token_modifiers, token),
			ach::clangd::semantic_token_color_variance{
				token.attr("color_variant").cast<int>(),
				token.attr("last_reference").cast<bool>()
			}
		});
	}

	const std::variant<std::string, ach::clangd::highlighter_error> result = ach::clangd::run_highlighter(
		code,
		ach::utility::range<const ach::clangd::semantic_token*>{
			semantic_tokens.data(), semantic_tokens.data() + semantic_tokens.size()},
		ach::utility::range<const std::string*>{keywords.data(), keywords.data() + keywords.size()},
		ach::clangd::highlighter_options{table_wrap_css_class, color_variants});

	return std::visit(ach::utility::visitor{
		[](const std::string& output) {
			return py::str(output);
		},
		[](ach::clangd::highlighter_error error) -> py::str {
			throw std::runtime_error(to_string(error));
		}
	}, result);
}

}

PYBIND11_MODULE(pyach, m) {
	m.doc() = ach::utility::program_description;

	m.def("run_mirror_highlighter", &run_mirror_highlighter,
		py::arg("code").none(false),
		py::arg("color").none(false),
		py::arg("num_class")        = ach::mirror::color_options::default_num_class,
		py::arg("str_class")        = ach::mirror::color_options::default_str_class,
		py::arg("str_esc_class")    = ach::mirror::color_options::default_str_esc_class,
		py::arg("chr_class")        = ach::mirror::color_options::default_chr_class,
		py::arg("chr_esc_class")    = ach::mirror::color_options::default_chr_esc_class,
		py::arg("escape_char")      = ach::mirror::color_options::default_escape_char,
		py::arg("empty_token_char") = ach::mirror::color_options::default_empty_token_char,
		py::arg("table_wrap_css_class") = "",
		py::arg("valid_css_classes")    = "",
		py::arg("replace") = false);

	m.def("run_clangd_highlighter", &run_clangd_highlighter,
		py::arg("code").none(false),
		py::arg("semantic_token_types").none(false),
		py::arg("semantic_token_modifiers").none(false),
		py::arg("semantic_tokens").none(false),
		py::arg("keywords").none(false),
		py::arg("table_wrap_css_class") = "",
		py::arg("color_variants") = ach::clangd::highlighter_options{}.color_variants);

	m.def("version", []() {
		namespace av = ach::utility::version;
		return py::make_tuple(av::major, av::minor, av::patch);
	});
}
