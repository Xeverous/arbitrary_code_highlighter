#include "parse_args.hpp"

#include <ach/version.hpp>

#include <boost/program_options.hpp>
#include <boost/optional.hpp>

#include <cstdlib>
#include <iostream>
#include <exception>
#include <string>

namespace
{

void print_help(const boost::program_options::options_description& options)
{
	std::cout << ach::program_description;

	options.print(std::cout);

	std::cout << "\n"
		"ACH is maintained by Xeverous\n"
		"view documentation and report bugs/feature requests/questions "
		"on github.com/Xeverous/arbitrary_code_highlighter\n"
		"contact: /u/Xeverous on reddit, Xeverous#2151 on Discord\n";
}

}

int run(int argc, char* argv[])
{
	try {
		namespace po = boost::program_options;

		boost::optional<std::string> path_input_code;
		boost::optional<std::string> path_input_color;
		boost::optional<std::string> path_output_html;
		constexpr auto str_input_code = "input-code";
		constexpr auto str_input_color = "input-color";
		constexpr auto str_output = "output";
		po::options_description positional_options("positional options (all mandatory, argument names not required)");
		positional_options.add_options()
			(str_input_code, po::value(&path_input_code))
			(str_input_color, po::value(&path_input_color))
			(str_output, po::value(&path_output_html))
		;

		// boost::optional<std::string> path_input_classes;
		bool replace_underscore_to_hyphen = false;
		po::options_description extra_options("generation options");
		extra_options.add_options()
			// ("check,c",  po::value(&path_input_classes),  "allow only classes that are specified in file")
			("replace,r", po::bool_switch(&replace_underscore_to_hyphen), "replace _ to - in class names")
		;

		bool show_help = false;
		bool show_version = false;
		po::options_description other_options("other options");
		other_options.add_options()
			("help,h",         po::bool_switch(&show_help),    "print this message")
			("version,v",      po::bool_switch(&show_version), "print version information")
		;

		po::positional_options_description positional_options_description;
		positional_options_description
			.add(str_input_code, 1)
			.add(str_input_color, 1)
			.add(str_output, 1);

		po::options_description all_options;
		all_options.add(positional_options).add(extra_options).add(other_options);

		if (argc == 0 || argc == 1)
		{
			print_help(all_options);
			return EXIT_SUCCESS;
		}

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(all_options).positional(positional_options_description).run(), vm);
		po::notify(vm);
	}
	catch (const std::exception& e) {
		std::cout << "Error: " << e.what();
		return EXIT_FAILURE;
	}
	catch (...) {
		std::cout << "Unknown error occured.\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

