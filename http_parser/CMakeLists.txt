cmake_minimum_required(VERSION 3.1)
project(http_parser)

add_library(http_parser STATIC
	http_parser.h
	http_parser.c
)

set_target_properties(http_parser PROPERTIES
	PUBLIC_HEADER http_parser.h
	DEBUG_POSTFIX d
)
	
install(TARGETS http_parser
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include
)