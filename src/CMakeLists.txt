idf_component_register(
    SRCS
		main.cpp

    REQUIRES
		driver
		espcoredump
		esp_timer
		freertos
)

target_compile_options(
	${COMPONENT_LIB}
	PRIVATE
		-Wall
		-Wextra
		-Wshadow
		-Werror
)
