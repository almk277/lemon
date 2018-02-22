#include "options.hpp"
#include "logger_imp.hpp"

options::options(int, char *[]) :
	n_workers(2),
	listen_port(8080),
	headers_size(4 * 1024)
{
	logger_imp lg;

	log.messages = { log_types::console{}, log_types::severity::debug };
	log.access = { log_types::console{} };

	routes.push_back({ route::equal{"/index.html"}, "Hello" });
	lg.debug("options initialized");
}
