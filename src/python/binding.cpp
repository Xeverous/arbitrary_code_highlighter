#include <ach/version.hpp>
#include <ach/cc/color_options.hpp>
#include <ach/cc/core.hpp>
#include <ach/utility/visitor.hpp>

#include <pybind11/pybind11.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <sstream>

namespace py = pybind11;

namespace {

std::string to_string(ach::cc::highlighter_error const& error)
{
	std::stringstream ss;
	ss << error;
	return ss.str();
}

py::str run_highlighter(
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

	auto const result = ach::cc::run_highlighter(
		code,
		color,
		ach::cc::highlighter_options{
			ach::generation_options{
				replace_underscores_to_hyphens,
				table_wrap_css_class,
				valid_css_classes
			},
			ach::cc::color_options{
				num_class,
				str_class, str_esc_class,
				chr_class, chr_esc_class,
				escape_char.front(),
				empty_token_char.front()
			}
		});

	return std::visit(ach::utility::visitor{
		[](std::string const& output) {
			return py::str(output);
		},
		[](ach::cc::highlighter_error error) -> py::str {
			throw std::runtime_error(to_string(error));
		}
	}, result);
}

}

PYBIND11_MODULE(pyach, m) {
	m.doc() = ach::program_description;

	m.def("run_highlighter", &run_highlighter, py::arg("code").none(false), py::arg("color").none(false),
		py::arg("num_class")        = ach::cc::color_options::default_num_class,
		py::arg("str_class")        = ach::cc::color_options::default_str_class,
		py::arg("str_esc_class")    = ach::cc::color_options::default_str_esc_class,
		py::arg("chr_class")        = ach::cc::color_options::default_chr_class,
		py::arg("chr_esc_class")    = ach::cc::color_options::default_chr_esc_class,
		py::arg("escape_char")      = ach::cc::color_options::default_escape_char,
		py::arg("empty_token_char") = ach::cc::color_options::default_empty_token_char,
		py::arg("table_wrap_css_class") = "",
		py::arg("valid_css_classes")    = "",
		py::arg("replace") = false);

	m.def("version", []() {
		namespace av = ach::version;
		return py::make_tuple(av::major, av::minor, av::patch);
	});
}
