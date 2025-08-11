#include "parse_args.hpp"

#include <ach/clangd/code_tokenizer.hpp>
#include <ach/utility/version.hpp>

#include <boost/program_options.hpp>
#include <boost/optional.hpp>

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <exception>
#include <string>
#include <system_error>
#include <variant>

namespace
{

namespace sfs = std::filesystem;

std::string load_file(const sfs::path& path, std::error_code& ec)
{
	sfs::file_status status = sfs::status(path, ec);
	if (ec)
		return {};

	if (sfs::is_directory(status)) {
		ec = std::make_error_code(std::errc::is_a_directory);
		return {};
	}

	const auto file_size = sfs::file_size(path, ec);
	if (ec)
		return {};

	std::ifstream file(path, std::ios::binary);

	if (!file.good()) {
		ec = std::make_error_code(std::io_errc::stream);
		return {};
	}

	std::string file_contents(file_size, '\0');
	file.read(file_contents.data(), static_cast<std::streamsize>(file_size));
	return file_contents;
}

bool dump_clangd_tokens(const boost::optional<std::string>& path_input_code)
{
	if (!path_input_code) {
		std::cout << "Error: missing input code\n";
		return false;
	}

	std::error_code ec;
	std::string input_code = load_file(*path_input_code, ec);
	if (ec) {
		std::cout << "Error: failed to load file: " << ec.message() << ".\n";
	}

	auto ct = ach::clangd::code_tokenizer(input_code, /* no keywords for now */ {});
	while (true) {
		using namespace ach::clangd;
		std::variant<code_token, highlighter_error> token_or_error = ct.next_code_token(true);

		if (std::holds_alternative<highlighter_error>(token_or_error)) {
			std::cout << std::get<highlighter_error>(token_or_error);
			return false;
		}

		const auto& token = std::get<code_token>(token_or_error);
		std::cout << token;

		if (token.syntax_element == syntax_element_type::end_of_input)
			return true;
	}
}

void print_help(const boost::program_options::options_description& options)
{
	std::cout << ach::utility::program_description << "\n";

	options.print(std::cout);

	std::cout << "\n"
		"ACH is maintained by Xeverous\n"
		"view documentation and report bugs/feature requests/questions "
		"on github.com/Xeverous/arbitrary_code_highlighter\n"
		"contact: /u/Xeverous on reddit, Xeverous_2151 on Discord\n";
}

}

int run(int argc, char* argv[])
{
	try {
		namespace po = boost::program_options;

		boost::optional<std::string> path_input_code;
		boost::optional<std::string> path_input_color;
		boost::optional<std::string> path_output;
		po::options_description io_options("input/output options (required by some highlight options)");
		io_options.add_options()
			("input-code", po::value(&path_input_code))
			("input-color", po::value(&path_input_color))
			("output", po::value(&path_output), "output file path, if not present print to stdout")
		;

		// boost::optional<std::string> path_input_classes;
		bool replace_underscore_to_hyphen = false;
		po::options_description mirror_options("mirror highlight options (NOT IMPLEMENTED)");
		mirror_options.add_options()
			// ("check,c",  po::value(&path_input_classes),  "allow only classes that are specified in file")
			("replace,r", po::bool_switch(&replace_underscore_to_hyphen), "replace _ to - in class names")
		;

		bool clangd_dump_tokens = false;
		po::options_description clangd_options("clangd highlight options");
		clangd_options.add_options()
			("dump-tokens,d", po::bool_switch(&clangd_dump_tokens), "dump tokens from input-code")
		;

		bool show_help = false;
		bool show_version = false;
		po::options_description other_options("other options");
		other_options.add_options()
			("help,h",         po::bool_switch(&show_help),    "print this message")
			("version,v",      po::bool_switch(&show_version), "print version information")
		;

		po::options_description all_options;
		all_options.add(io_options).add(mirror_options).add(clangd_options).add(other_options);

		if (argc == 0 || argc == 1) {
			print_help(all_options);
			return EXIT_SUCCESS;
		}

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(all_options).run(), vm);
		po::notify(vm);

		if (show_help) {
			print_help(all_options);
			return EXIT_SUCCESS;
		}

		if (show_version) {
			namespace av = ach::utility::version;
			std::cout << av::major << "." << av::minor << "." << av::patch << "\n";
			return EXIT_SUCCESS;
		}

		if (clangd_dump_tokens)
			return dump_clangd_tokens(path_input_code) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
	catch (...) {
		std::cout << "Unknown error occured.\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
