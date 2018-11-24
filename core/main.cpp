#include "logs.hpp"
#include "parameters.hpp"
#include "manager.hpp"
#include "logger_imp.hpp"
#include <iostream>
#include <exception>

int main(int argc, char *argv[])
{
	try {
		logs::preinit();
		common_logger lg;
		const parameters params{ argc, argv, lg };
		manager m{params};
		m.run();
	} catch(std::exception &error) {
		std::cerr << error.what() << std::endl;
		return 1;
	}
}
