#include <ach/version.hpp>
#include <ach/color_options.hpp>
#include <ach/core.hpp>
#include <ach/utility/visitor.hpp>

#include <pybind11/pybind11.h>

#include <optional>
#include <stdexcept>
#include <sstream>

namespace py = pybind11;

namespace {

std::string to_string(const ach::highlighter_error& error)
{
	std::stringstream ss;

	auto const append = [&ss](ach::text_location tl) {
		ss << "line " << tl.line_number() << ":\n" << tl.line() << '\n';

		for (auto i = 0; i < tl.first_column(); ++i)
			ss << ' ';

		for (auto i = 0u; i < tl.str().size(); ++i)
			ss << '~';

		ss << '\n';
	};
	ss << "ERROR:\n" << error.reason << "\nin code ";
	append(error.code_location);
	ss << "in color ";
	append(error.color_location);
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
	bool replace_underscores_to_hyphens)
{
	if (escape_char.size() != 1u) {
		throw py::value_error("argument 'escape_char' should be 1 character");
	}

	if (empty_token_char.size() != 1u) {
		throw py::value_error("argument 'empty_token_char' should be 1 character");
	}

	auto const result = ach::run_highlighter(
		code,
		color,
		ach::highlighter_options{
			ach::generation_options{replace_underscores_to_hyphens},
			ach::color_options{
				num_class,
				str_class, str_esc_class,
				chr_class, chr_esc_class,
				escape_char.front(),
				empty_token_char.front()}});
	return std::visit(ach::utility::visitor{
		[](std::string const& output) {
			return py::str(output);
		},
		[](ach::highlighter_error error) -> py::str {
			throw std::runtime_error(to_string(error));
		}
	}, result);
}

}

PYBIND11_MODULE(pyach, m) {
	m.doc() = ach::program_description;

	m.def("run_highlighter", &run_highlighter, py::arg("code").none(false), py::arg("color").none(false),
		py::arg("num_class")        = ach::color_options::default_num_class,
		py::arg("str_class")        = ach::color_options::default_str_class,
		py::arg("str_esc_class")    = ach::color_options::default_str_esc_class,
		py::arg("chr_class")        = ach::color_options::default_chr_class,
		py::arg("chr_esc_class")    = ach::color_options::default_chr_esc_class,
		py::arg("escape_char")      = ach::color_options::default_escape_char,
		py::arg("empty_token_char") = ach::color_options::default_empty_token_char,
		py::arg("replace") = false);

	m.def("version", []() {
		namespace av = ach::version;
		return py::make_tuple(av::major, av::minor, av::patch);
	});
}
