cmake_minimum_required(VERSION 3.6)
project(lemon)

include(CTest)
set(LEMON_LOG_LEVEL 3 CACHE STRING "maximal supported log level: from 1 (error) to 5 (trace)")
set_property(CACHE LEMON_LOG_LEVEL PROPERTY STRINGS 1 2 3 4 5)
option(LEMON_NO_ACCESS_LOG "disable access.log")
set(BOOST_ROOT "" CACHE PATH "specific boost installation path")
set(HTTP_PARSER_URL https://github.com/nodejs/http-parser/archive/v2.7.1.zip
	CACHE STRING "http-parser source archive (local or remote) or directory")
option(LINK_SHARED "link Boost dynamically")
option(DEVELOPER_MODE "developer build settings")
set(SANITIZER "" CACHE STRING "enables sanitizer of this type")
set_property(CACHE SANITIZER PROPERTY STRINGS "" address memory thread undefined)
set(CLANG_TIDY_EXE "" CACHE FILEPATH "clang-tidy program")

set(CMAKE_CXX_STANDARD 14)
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
	set(BOOST_DEFS BOOST_ALL_NO_LIB)
	if(LINK_SHARED)
		list(APPEND BOOST_DEFS BOOST_ALL_DYN_LINK)
	else()
		set(Boost_USE_STATIC_LIBS ON)
	endif()
	if(BUILD_TESTING)
		set(BOOST_TEST_COMPONENT unit_test_framework)
	endif()
	find_package(Boost 1.62 REQUIRED COMPONENTS system log ${BOOST_TEST_COMPONENT})
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
	api/http_error.hpp
	api/logger.hpp
	api/message.hpp
	api/request_handler.hpp
	api/string.hpp
	api/string_builder.hpp
	api/utility.hpp
)
set(CORE_SRC
	core/algorithm.hpp
	core/arena.cpp
	core/arena_imp.hpp
	core/client.cpp
	core/client.hpp
	core/leak_checked.hpp
	core/logger.cpp
	core/logger_imp.cpp
	core/logger_imp.hpp
	core/logs.cpp
	core/logs.hpp
	core/main.cpp
	core/manager.cpp
	core/manager.hpp
	core/message.cpp
	core/options.cpp
	core/options.hpp
	core/parameters.hpp
	core/parser.cpp
	core/parser.hpp
	core/request_handler.cpp
	core/rh_manager.cpp
	core/rh_manager.hpp
	core/router.cpp
	core/router.hpp
	core/server.cpp
	core/server.hpp
	core/string_builder.cpp
	core/task.cpp
	core/task.hpp
	core/task_ident.hpp
	core/task_builder.cpp
	core/task_builder.hpp
)
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
	http_parser Boost::boost Boost::system Boost::log)
enable_sanitizer(lemon)
target_compile_definitions(lemon PRIVATE LEMON_LOG_LEVEL=${LEMON_LOG_LEVEL})
if(LEMON_NO_ACCESS_LOG)
	target_compile_definitions(lemon PRIVATE LEMON_NO_ACCESS_LOG)
endif()

if(BUILD_TESTING)
	set(TEST_SRC
		unittests/stub_logger.cpp
		unittests/stub_logger.hpp
		unittests/test_main.cpp
		unittests/test_arena.cpp
		unittests/test_parser.cpp
		unittests/test_resp_it.cpp
		unittests/test_router.cpp
		core/arena.cpp
		core/message.cpp
		core/options.cpp
		core/rh_manager.cpp
		core/router.cpp
		core/parser.cpp
		core/request_handler.cpp
	)

	add_executable(test_lemon ${TEST_SRC})
	target_include_directories(test_lemon PRIVATE
		"${PROJECT_SOURCE_DIR}"
		"${PROJECT_SOURCE_DIR}/api"
		"${PROJECT_SOURCE_DIR}/core"
	)
	target_link_libraries(test_lemon PRIVATE
		http_parser Boost::boost Boost::system Boost::unit_test_framework)
	enable_sanitizer(test_lemon)
	add_test(NAME test_lemon COMMAND test_lemon)
	enable_testing()
endif(BUILD_TESTING)