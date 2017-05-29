cmake_minimum_required (VERSION 2.6)

project(xterminal C)

add_definitions(-O -Wall -Werror --std=gnu99 -g3)

add_executable(client xterminal_client.c)
add_executable(server xterminal_server.c util.c)

target_link_libraries(client ev evmongoose)
target_link_libraries(server ev util evmongoose)

install(TARGETS client server
	RUNTIME DESTINATION bin
)
