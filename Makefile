all: compile

compile:
	PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) cmake --build build

lint:
	PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) cmake --build build --target lint

run:
	PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) cmake --build build
	./target/modbox

clean:
	rm -rf build target

refresh:
	rm -rf build target
	PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=on
	ln -f build/compile_commands.json compile_commands.json

refresh-tidy:
	rm -rf build target
	PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=on -DENABLE_CLANG_TIDY=ON
	ln -f build/compile_commands.json compile_commands.json

.PHONY: all compile lint run clean refresh refresh-tidy
