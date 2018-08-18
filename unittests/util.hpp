#pragma once

#include "arena.hpp"
#include "stub_logger.hpp"
#include <memory>

using arena_ptr = std::unique_ptr<arena, void(*)(arena*)>;

inline arena_ptr make_arena()
{
	return arena_ptr{ arena::make(&slg), arena::remove };
}
