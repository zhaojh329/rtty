cmake_minimum_required (VERSION 2.6)

project(xterminal C)

add_definitions(-O -Wall -Werror --std=gnu99)

add_executable(xtu xterminal_user.c util.c)
add_executable(xtd xterminal_dev.c util.c)
add_executable(xts xterminal_server.c util.c)

target_link_libraries(xtu ev)
target_link_libraries(xtd ev util)
target_link_libraries(xts ev)

install(TARGETS xtu xtd xts
	RUNTIME DESTINATION bin
)
