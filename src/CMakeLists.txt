idf_component_register(
	SRCS
		main.cpp
		network.cpp

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
)
