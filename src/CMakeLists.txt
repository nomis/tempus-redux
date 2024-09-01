idf_component_register(
	SRCS
		calendar.cpp
		main.cpp
		network.cpp
		time_signal.cpp
		transmit.cpp
		ui.cpp

	REQUIRES
		driver
		espcoredump
		esp_timer
		esp_wifi
		freertos
		nvs_flash
)

target_compile_options(
	${COMPONENT_LIB}
	PRIVATE
		-Wall
		-Wextra
		-Wshadow
		-Werror
		-Wsign-compare
)

target_link_libraries(
	${COMPONENT_LIB}
	INTERFACE
	-Wl,--undefined=__wrap_adjtime
	-Wl,--wrap=adjtime
)
