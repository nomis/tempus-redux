.PHONY: app target config clean flash erase-ota app-flash monitor cppcheck

app: | build
	idf.py build

build:
	-btrfs subvolume create build
	mkdir -p build

target: | build
	idf.py set-target esp32s3

config: | build
	idf.py menuconfig

clean: | build
	idf.py clean

flash: app
	idf.py flash

erase-ota: | build
	idf.py erase-otadata

app-flash: app
	idf.py app-flash

monitor: | build
	idf.py monitor --timestamps --timestamp-format "%Y-%m-%d %H:%M:%S.%f" --no-reset

cppcheck:
	cppcheck --enable=all --suppress=unusedFunction --suppress=useStlAlgorithm \
		--suppress=knownConditionTrueFalse --suppress=missingIncludeSystem \
		--suppress=internalAstError --inline-suppr -I src/ src/*.cpp
