all: compile

compile:
	cmake --build build

lint:
	cmake --build build --target lint

run:
	cmake --build build
	./target/modbox

clean:
	rm -rf build target

refresh:
	rm -rf build target
	cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=on
	ln -f build/compile_commands.json compile_commands.json

refresh-tidy:
	rm -rf build target
	cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=on -DENABLE_CLANG_TIDY=ON
	ln -f build/compile_commands.json compile_commands.json

.PHONY: all compile lint run clean refresh refresh-tidy
