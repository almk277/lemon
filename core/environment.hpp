#pragma once
#include "router.hpp"
#include <memory>
#include <utility>
class options;
class rh_manager;

struct environment
{
	environment(std::shared_ptr<const options> opt, std::shared_ptr<const rh_manager> rh_man):
		opt{move(opt)},
		rh_man{move(rh_man)},
		rout{*this->rh_man, *this->opt}
	{
	}

	const std::shared_ptr<const options> opt;
	const std::shared_ptr<const rh_manager> rh_man;
	const router rout;
};
