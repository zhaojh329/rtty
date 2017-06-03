cmake_minimum_required (VERSION 2.6)

project(xterminal C)

add_definitions(-O -Wall -Werror --std=gnu99 -g3)

add_executable(xterminal_device xterminal_device.c)
add_executable(xterminal_broker xterminal_broker.c)

target_link_libraries(xterminal_broker ev evmongoose)
target_link_libraries(xterminal_device ev util evmongoose)

install(TARGETS xterminal_device xterminal_broker
	RUNTIME DESTINATION bin
)
