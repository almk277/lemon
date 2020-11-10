cmake_minimum_required(VERSION 3.6)
project(lemon)

include(CTest)
set(LEMON_LOG_LEVEL 3 CACHE STRING "maximal supported log level: from 1 (error) to 5 (trace)")
set_property(CACHE LEMON_LOG_LEVEL PROPERTY STRINGS 1 2 3 4 5)
option(LEMON_NO_ACCESS_LOG "disable access.log")
option(LEMON_NO_CONFIG "disable config file")
set(LEMON_CONFIG_PATH "./lemon.ini" CACHE FILEPATH "config file path")
set(BOOST_ROOT "" CACHE PATH "specific boost installation path")
set(HTTP_PARSER_URL https://github.com/nodejs/http-parser/archive/v2.7.1.zip
	CACHE STRING "http-parser source archive (local or remote) or directory")
option(LINK_SHARED "link Boost dynamically")
option(DEVELOPER_MODE "developer build settings")
set(SANITIZER "" CACHE STRING "enables sanitizer of this type")
set_property(CACHE SANITIZER PROPERTY STRINGS "" address memory thread undefined)
set(CLANG_TIDY_EXE "" CACHE FILEPATH "clang-tidy program")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(DEVELOPER_MODE)
	if(CLANG_TIDY_EXE)
		set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
	endif()
	if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU|Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
		set(CMAKE_VERBOSE_MAKEFILE ON)
	elseif(MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
	endif()
endif(DEVELOPER_MODE)

function(find_boost)
	set(BOOST_DEFS BOOST_ALL_NO_LIB BOOST_FILESYSTEM_NO_DEPRECATED BOOST_ASIO_NO_DEPRECATED _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
	if(LINK_SHARED)
		list(APPEND BOOST_DEFS BOOST_ALL_DYN_LINK)
	else()
		set(Boost_USE_STATIC_LIBS ON)
	endif()
	if(BUILD_TESTING)
		set(BOOST_TEST_COMPONENT unit_test_framework)
	endif()
	find_package(Boost 1.65 REQUIRED COMPONENTS system log filesystem program_options ${BOOST_TEST_COMPONENT})
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

function(enable_sanitizer target)
	if(SANITIZER)
		target_compile_options(${target} PRIVATE -fsanitize=${SANITIZER})
		target_link_libraries(${target} PRIVATE -fsanitize=${SANITIZER})
	endif()
endfunction()

set(API_SRC
	api/arena.hpp
	api/config.hpp
	api/http_error.hpp
	api/http_message.hpp
	api/http_request_handler.hpp
	api/logger.hpp
	api/string.hpp
	api/string_builder.hpp
	api/string_view.hpp
)
set(CORE_SRC
	core/algorithm.hpp
	core/arena.cpp
	core/arena_imp.hpp
	core/cmdline_parser.cpp
	core/cmdline_parser.hpp
	core/config.cpp
	core/http_message.cpp
	core/http_parser_.cpp
	core/http_parser_.hpp
	core/http_request_handler.cpp
	core/http_rh_manager.cpp
	core/http_rh_manager.hpp
	core/http_router.cpp
	core/http_router.hpp
	core/http_task.cpp
	core/http_task.hpp
	core/http_task_builder.cpp
	core/http_task_builder.hpp
	core/leak_checked.hpp
	core/logger.cpp
	core/logger_imp.cpp
	core/logger_imp.hpp
	core/logs.cpp
	core/logs.hpp
	core/main.cpp
	core/manager.cpp
	core/manager.hpp
	core/options.cpp
	core/options.hpp
	core/parameters.hpp
	core/string_builder.cpp
	core/task_ident.hpp
	core/tcp_client.cpp
	core/tcp_client.hpp
	core/tcp_server.cpp
	core/tcp_server.hpp
	core/visitor.hpp
)
if(NOT LEMON_NO_CONFIG)
	set(CORE_SRC ${CORE_SRC}
		core/config_parser.hpp
		core/config_parser.cpp
	)
endif()

set(MODULE_SRC
	modules/static_file.cpp
	modules/static_file.hpp
	modules/testing.cpp
	modules/testing.hpp
)

find_boost()
build_http_parser()

add_executable(lemon ${API_SRC} ${CORE_SRC} ${MODULE_SRC})
target_include_directories(lemon PRIVATE
	"${PROJECT_SOURCE_DIR}"
	"${PROJECT_SOURCE_DIR}/api"
)
target_link_libraries(lemon PRIVATE
	http_parser Boost::boost Boost::system Boost::log Boost::filesystem Boost::program_options)
enable_sanitizer(lemon)
target_compile_definitions(lemon PRIVATE LEMON_LOG_LEVEL=${LEMON_LOG_LEVEL})
target_compile_definitions(lemon PRIVATE LEMON_CONFIG_PATH=${LEMON_CONFIG_PATH})
if(LEMON_NO_ACCESS_LOG)
	target_compile_definitions(lemon PRIVATE LEMON_NO_ACCESS_LOG)
endif()
if(LEMON_NO_CONFIG)
	target_compile_definitions(lemon PRIVATE LEMON_NO_CONFIG)
endif()

if(BUILD_TESTING)
	set(TEST_SRC
		unittests/stub_logger.cpp
		unittests/stub_logger.hpp
		unittests/test_main.cpp
		unittests/test_arena.cpp
		unittests/test_cmdline_parser.cpp
		unittests/test_config.cpp
		unittests/test_config.hpp
		unittests/test_parser.cpp
		unittests/test_resp_it.cpp
		unittests/test_router.cpp
		unittests/test_string_builder.cpp
		core/arena.cpp
		core/cmdline_parser.cpp
		core/config.cpp
		core/http_message.cpp
		core/http_parser_.cpp
		core/http_request_handler.cpp
		core/http_rh_manager.cpp
		core/http_router.cpp
		core/options.cpp
		core/string_builder.cpp
	)
	if(NOT LEMON_NO_CONFIG)
		set(TEST_SRC ${TEST_SRC}
			core/config_parser.cpp
			unittests/test_config_parser.cpp
		)
	endif()
	
	add_executable(test_lemon ${TEST_SRC})
	target_include_directories(test_lemon PRIVATE
		"${PROJECT_SOURCE_DIR}"
		"${PROJECT_SOURCE_DIR}/api"
		"${PROJECT_SOURCE_DIR}/core"
	)
	target_link_libraries(test_lemon PRIVATE
		http_parser Boost::boost Boost::system Boost::filesystem Boost::program_options Boost::unit_test_framework)
	target_compile_definitions(test_lemon PRIVATE)
	enable_sanitizer(test_lemon)
	add_test(NAME test_lemon COMMAND test_lemon)
	enable_testing()
endif(BUILD_TESTING)