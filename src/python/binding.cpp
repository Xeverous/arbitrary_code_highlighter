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

namespace ach::bind {

namespace {

std::string to_string(const mirror::highlighter_error& error)
{
	std::stringstream ss;
	ss << error;
	return ss.str();
}

py::str run_mirror_highlighter(
	// required arguments
	std::string_view code, std::string_view color,
	// optional, keyword arguments
	std::string_view num_keyword, std::string_view str_keyword, std::string_view chr_keyword,
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

	const auto result = mirror::run_highlighter(
		code,
		color,
		mirror::highlighter_options{
			mirror::generation_options{
				replace_underscores_to_hyphens,
				table_wrap_css_class,
				valid_css_classes
			},
			mirror::color_options{
				num_keyword, str_keyword, chr_keyword,
				num_class,
				str_class, str_esc_class,
				chr_class, chr_esc_class,
				escape_char.front(),
				empty_token_char.front()
			}
		});

	return std::visit(utility::visitor{
		[](const std::string& output) {
			return py::str(output);
		},
		[](mirror::highlighter_error error) -> py::str {
			throw std::runtime_error(to_string(error));
		}
	}, result);
}

std::vector<clangd::semantic_token_type> parse_semantic_token_types(const py::list& list_semantic_token_types)
{
	std::vector<clangd::semantic_token_type> result;
	result.reserve(list_semantic_token_types.size());

	for (const auto& token_type : list_semantic_token_types) {
		const auto name = token_type.cast<std::string_view>();

		std::optional<clangd::semantic_token_type> token_result =
			clangd::parse_semantic_token_type(name);

		if (token_result == std::nullopt)
			throw py::value_error(std::string("unknown SemanticToken type: ").append(name).c_str());

		result.push_back(*token_result);
	}

	return result;
}

std::vector<clangd::apply_semantic_token_modifier_f*>
parse_semantic_token_modifiers(const py::list& list_semantic_token_modifiers)
{
	std::vector<clangd::apply_semantic_token_modifier_f*> result;
	result.reserve(list_semantic_token_modifiers.size());

	for (const auto& token_modifier : list_semantic_token_modifiers) {
		const auto name = token_modifier.cast<std::string_view>();

		clangd::apply_semantic_token_modifier_f* fptr = clangd::parse_semantic_token_modifier(name);

		if (fptr == nullptr)
			throw py::value_error(std::string("unknown SemanticToken modifier: ").append(name).c_str());

		result.push_back(fptr);
	}

	return result;
}

clangd::semantic_token_info get_semantic_token_info(
	const clangd::semantic_token_decoder& decoder,
	const pybind11::handle& semantic_token)
{
	const auto token_type = semantic_token.attr("token_type").cast<std::size_t>();
	const auto token_modifiers = semantic_token.attr("token_modifiers").cast<std::size_t>();

	std::optional<clangd::semantic_token_info> decoded = decoder.decode_semantic_token(token_type, token_modifiers);
	if (!decoded) {
		throw py::value_error(("SemanticToken has token_type " + std::to_string(token_type) + " but only "
			+ std::to_string(decoder.token_types.size()) + " token types were reported!").c_str());
	}

	return *decoded;
}

std::vector<std::string> parse_keywords(const py::list& list_keywords)
{
	std::vector<std::string> result;
	result.reserve(list_keywords.size());

	for (const auto& obj : list_keywords)
		result.push_back(obj.cast<std::string>());

	return result;
}

std::string to_string(const clangd::highlighter_error& error)
{
	std::stringstream ss;
	ss << error;
	return ss.str();
}

struct clangd_highlighter
{
	clangd::semantic_token_decoder decoder;
	clangd::highlighter hl;

	// allocation reuse
	mutable std::vector<clangd::semantic_token> semantic_tokens;
};

clangd_highlighter make_clangd_highlighter(
	const py::list& legend_semantic_token_types,
	const py::list& legend_semantic_token_modifiers,
	const py::list& keywords)
{
	return clangd_highlighter{
		clangd::semantic_token_decoder{
			parse_semantic_token_types(legend_semantic_token_types),
			parse_semantic_token_modifiers(legend_semantic_token_modifiers)
		},
		clangd::highlighter(parse_keywords(keywords)),
		{}
	};
}

py::str run_clangd_highlighter(
	const clangd_highlighter& chl,
	std::string_view code,
	const py::list& list_semantic_tokens,
	std::string_view table_wrap_css_class,
	int color_variants,
	bool highlight_printf_formatting)
{
	chl.semantic_tokens.clear();
	chl.semantic_tokens.reserve(list_semantic_tokens.size());

	for (const auto& token : list_semantic_tokens) {
		chl.semantic_tokens.push_back(clangd::semantic_token{
			text::position{
				token.attr("line").cast<std::size_t>(),
				token.attr("column").cast<std::size_t>()
			},
			token.attr("length").cast<std::size_t>(),
			get_semantic_token_info(chl.decoder, token),
			clangd::semantic_token_color_variance{
				token.attr("color_variant").cast<int>(),
				token.attr("last_reference").cast<bool>()
			}
		});
	}

	const std::variant<std::string, clangd::highlighter_error> result = chl.hl.run(
		code,
		{chl.semantic_tokens.data(), chl.semantic_tokens.data() + chl.semantic_tokens.size()},
		clangd::highlighter_options{table_wrap_css_class, color_variants, highlight_printf_formatting});

	return std::visit(utility::visitor{
		[](const std::string& output) {
			return py::str(output);
		},
		[](clangd::highlighter_error error) -> py::str {
			throw std::runtime_error(to_string(error));
		}
	}, result);
}

}
}

PYBIND11_MODULE(pyach, m) {
	m.doc() = ach::utility::program_description;

	m.def("run_mirror_highlighter", &ach::bind::run_mirror_highlighter,
		py::arg("code").none(false),
		py::arg("color").none(false),
		py::arg("num_keyword")      = ach::mirror::color_options::default_num_keyword,
		py::arg("str_keyword")      = ach::mirror::color_options::default_str_keyword,
		py::arg("chr_keyword")      = ach::mirror::color_options::default_chr_keyword,
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

	py::class_<ach::bind::clangd_highlighter>(m, "ClangdHighlighter")
		.def(py::init(&ach::bind::make_clangd_highlighter),
			py::arg("semantic_token_types").none(false),
			py::arg("semantic_token_modifiers").none(false),
			py::arg("keywords").none(false))
		.def("run", &ach::bind::run_clangd_highlighter,
			py::arg("code").none(false),
			py::arg("semantic_tokens").none(false),
			py::arg("table_wrap_css_class") = "",
			py::arg("color_variants") = ach::clangd::highlighter_options{}.color_variants,
			py::arg("highlight_printf_formatting") = ach::clangd::highlighter_options{}.highlight_printf_formatting);

	m.def("version", []() {
		namespace av = ach::utility::version;
		return py::make_tuple(av::major, av::minor, av::patch);
	});
}
