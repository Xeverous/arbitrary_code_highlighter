#include <ach/version.hpp>
#include <ach/color_options.hpp>
#include <ach/core.hpp>

#include <pybind11/pybind11.h>

#include <optional>
#include <stdexcept>
#include <sstream>

namespace py = pybind11;

namespace {

std::optional<std::string_view> to_string_view(py::str str) noexcept
{
	auto const utf8_str = py::reinterpret_steal<py::object>(PyUnicode_AsEncodedString(str.ptr(), "utf-8", nullptr));

	if (utf8_str.ptr() == nullptr) {
		// nothing to do here, PyUnicode_AsEncodedString will raise an exception upon failure
		return std::nullopt;
	}

	char* bytes = nullptr;
	Py_ssize_t size = 0;
#if PY_MAJOR_VERSION < 3
	const int rc = PyString_AsStringAndSize(utf8_str.ptr(), &bytes, &size); // accepts both string and unicode objects
#else
	const int rc = PyBytes_AsStringAndSize(utf8_str.ptr(), &bytes, &size);
#endif

	if (rc != 0) {
		// nothing else to do, both AsStringAndSize will raise an exception upon failure
		return std::nullopt;
	}

	return std::string_view(bytes, size);
}

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
	py::str code, py::str color,
	// optional, keyword arguments
	py::str num_class,
	py::str str_class, py::str str_esc_class,
	py::str chr_class, py::str chr_esc_class,
	py::str escape_char, py::str empty_token_char,
	bool replace_underscores_to_hyphens)
{
	auto code_sv = to_string_view(code);
	if (!code_sv)
		throw py::value_error("failed to convert argument 'code' to UTF-8 string");

	auto color_sv = to_string_view(color);
	if (!color_sv)
		throw py::value_error("failed to convert argument 'color' to UTF-8 string");

	auto num_class_sv = to_string_view(num_class);
	if (!num_class_sv)
		throw py::value_error("failed to convert argument 'num_class' to UTF-8 string");

	auto str_class_sv = to_string_view(str_class);
	if (!str_class_sv)
		throw py::value_error("failed to convert argument 'str_class' to UTF-8 string");

	auto str_esc_class_sv = to_string_view(str_esc_class);
	if (!str_esc_class_sv)
		throw py::value_error("failed to convert argument 'str_esc_class' to UTF-8 string");

	auto chr_class_sv = to_string_view(chr_class);
	if (!chr_class_sv)
		throw py::value_error("failed to convert argument 'chr_class' to UTF-8 string");

	auto chr_esc_class_sv = to_string_view(chr_esc_class);
	if (!chr_esc_class_sv)
		throw py::value_error("failed to convert argument 'chr_esc_class' to UTF-8 string");

	auto escape_char_sv = to_string_view(escape_char);
	if (!escape_char_sv)
		throw py::value_error("failed to convert argument 'escape_char' to UTF-8 string");

	auto empty_token_char_sv = to_string_view(empty_token_char);
	if (!empty_token_char_sv)
		throw py::value_error("failed to convert argument 'empty_token_char' to UTF-8 string");

	if ((*escape_char_sv).size() != 1u) {
		throw py::value_error("argument 'escape_char' should be 1 character");
	}

	if ((*empty_token_char_sv).size() != 1u) {
		throw py::value_error("argument 'empty_token_char' should be 1 character");
	}

	auto const result = ach::run_highlighter(
		*code_sv,
		*color_sv,
		ach::highlighter_options{
			ach::generation_options{replace_underscores_to_hyphens},
			ach::color_options{
				*num_class_sv,
				*str_class_sv, *str_esc_class_sv,
				*chr_class_sv, *chr_esc_class_sv,
				(*escape_char_sv).front(),
				(*empty_token_char_sv).front()}});
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

PYBIND11_MODULE(arbitrary_code_highlighter, m) {
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
		auto result = py::tuple(3);
		result[0] = ach::version::major;
		result[1] = ach::version::minor;
		result[2] = ach::version::patch;
		return result;
	});
}
