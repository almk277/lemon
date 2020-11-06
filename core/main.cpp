#include "cmdline_parser.hpp"
#include "parameters.hpp"
#include "logs.hpp"
#include "manager.hpp"
#include <iostream>
#include <exception>

namespace
{
struct RequestToQuit : std::exception {};

auto make_parameters(int argc, char *argv[])
{
	const auto parser = CommandLineParser{};
	const auto cmdline = parser.parse(argc, argv);
	if (cmdline.has("help")) {
		parser.print_options(std::cerr);
		throw RequestToQuit{};
	}
	if (cmdline.has("version")) {
		std::cerr << "Lemon 0.01";
		throw RequestToQuit{};
	}

	return cmdline.to_parameters();
}
}

int main(int argc, char *argv[])
{
	try {
		auto params = make_parameters(argc, argv);

		logs::preinit();
		Manager man{ params };
		man.run();
	} catch(RequestToQuit&) {
		// nothing to do
	} catch(std::exception &error) {
		std::cerr << argv[0] << ": " << error.what() << std::endl;
		return 1;
	}
}
