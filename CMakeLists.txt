cmake_minimum_required(VERSION 3.5)
project(lemon)

set(HTTP_PARSER_URL https://github.com/nodejs/http-parser/archive/v2.7.1.zip
	CACHE STRING "http-parser source archive (local or remote) or directory")
option(LINK_SHARED "link Boost dynamically")
option(DEVELOPER_MODE "developer build settings")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DEVELOPER_MODE)
	if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang")
		list(APPEND CMAKE_CXX_FLAGS -Wall)
		set(CMAKE_VERBOSE_MAKEFILE ON)
	elseif(MSVC)
		list(APPEND CMAKE_CXX_FLAGS /W3)
	endif()
endif(DEVELOPER_MODE)

function(find_boost)
	set(BOOST_DEFS BOOST_ALL_NO_LIB)
	if(LINK_SHARED)
		list(APPEND BOOST_DEFS BOOST_ALL_DYN_LINK)
	else()
		set(Boost_USE_STATIC_LIBS ON)
	endif()
	find_package(Boost REQUIRED COMPONENTS system log)
	if(WIN32)
		list(APPEND BOOST_DEFS _WIN32_WINNT=0x0600)
	endif()
	set_target_properties(Boost::boost PROPERTIES
		INTERFACE_COMPILE_DEFINITIONS "${BOOST_DEFS}")
endfunction()

function(build_http_parser)
	set(HP_PREFIX "${CMAKE_BINARY_DIR}/http-parser")
	set(HP_SRC_DIR "${HP_PREFIX}/src/http-parser-ex")
	set(HP_INC_DIR "${HP_PREFIX}/include")
	set(HP_LIB_DIR "${HP_PREFIX}/lib")
	set(HP_LIB_NAME ${CMAKE_STATIC_LIBRARY_PREFIX}http_parser${CMAKE_STATIC_LIBRARY_SUFFIX})
	set(HP_LIB_NAME_DEBUG ${CMAKE_STATIC_LIBRARY_PREFIX}http_parserd${CMAKE_STATIC_LIBRARY_SUFFIX})
	if("${HTTP_PARSER_URL}" MATCHES "://")
		set(HTTP_PARSER_URL_INTERN "${HTTP_PARSER_URL}")
	else()
		file(TO_CMAKE_PATH "${HTTP_PARSER_URL}" HTTP_PARSER_URL_INTERN)
	endif()
	include(ExternalProject)
	ExternalProject_Add(http-parser-ex
		PREFIX "${HP_PREFIX}"
		URL "${HTTP_PARSER_URL_INTERN}"
		PATCH_COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/http_parser/CMakeLists.txt" "${HP_SRC_DIR}"
		CMAKE_ARGS "-DCMAKE_INSTALL_PREFIX=${HP_PREFIX}" "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
	)
	add_library(http_parser STATIC IMPORTED)
	file(MAKE_DIRECTORY "${HP_INC_DIR}")
	set_target_properties(http_parser PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${HP_INC_DIR}"
		IMPORTED_LOCATION "${HP_LIB_DIR}/${HP_LIB_NAME}"
		IMPORTED_LOCATION_DEBUG "${HP_LIB_DIR}/${HP_LIB_NAME_DEBUG}"
	)
	add_dependencies(http_parser http-parser-ex)
endfunction()

set(api_src
	api/arena.hpp
	api/logger.hpp
	api/message.hpp
	api/request_handler.hpp
	api/response_status.hpp
	api/string_builder.hpp
	api/utility.hpp
)
set(core_src
	core/arena.cpp
	core/arena_ex.hpp
	core/client.cpp
	core/client.hpp
	core/http_error.hpp
	core/logger.cpp
	core/logger_imp.hpp
	core/logs.cpp
	core/logs.hpp
	core/main.cpp
	core/manager.cpp
	core/manager.hpp
	core/message.cpp
	core/options.cpp
	core/options.hpp
	core/parser.cpp
	core/parser.hpp
	core/request_handler.cpp
	core/response_status.cpp
	core/rh_manager.cpp
	core/rh_manager.hpp
	core/router.cpp
	core/router.hpp
	core/task.cpp
	core/task.hpp
	core/task_builder.cpp
	core/task_builder.hpp
)
set(module_src
	modules/hello.cpp
	modules/hello.hpp
)

find_boost()
build_http_parser()

add_executable(lemon ${api_src} ${core_src} ${module_src})
target_include_directories(lemon PRIVATE "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/api")
target_link_libraries(lemon PRIVATE http_parser Boost::boost Boost::system Boost::log)