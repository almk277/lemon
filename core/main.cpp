#include "logs.hpp"
#include "options.hpp"
#include "manager.hpp"
#include "logger_imp.hpp"
#include <iostream>
#include <exception>

int main(int argc, char *argv[])
{
	try {
		logs::preinit();
		logger_imp lg;
		options opt{ argc, argv, lg };
		logs::init(opt);
		manager m{std::move(opt)};
		m.run();
	} catch(std::exception &error) {
		std::cerr << error.what() << std::endl;
	}
}
