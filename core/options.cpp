#include "options.hpp"
#include "parameters.hpp"
#include "logger.hpp"

options::options(const parameters &p, logger &lg):
	n_workers(2),
	headers_size(4 * 1024),
	log{
		{log_types::console{}, log_types::severity::debug},
		{log_types::console{}}
	},
	servers{
		{
			8080,
			{
				{route::prefix{"/file/"}, "StaticFile"},
				{route::prefix{"/"}, "Testing"},
			}
		},
	}
{
	lg.debug("options initialized");
}
