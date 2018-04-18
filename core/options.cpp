#include "options.hpp"
#include "logger.hpp"

options::options(int, char *[], logger &lg) :
	n_workers(2),
	listen_port(8080),
	headers_size(4 * 1024)
{
	log.messages = { log_types::console{}, log_types::severity::debug };
	log.access = { log_types::console{} };

	routes.push_back({ route::equal{"/index.html"}, "Hello" });
	lg.debug("options initialized");
}
