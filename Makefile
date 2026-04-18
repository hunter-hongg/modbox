all: compile

compile: 
	cmake --build build

run: 
	cmake --build build
	./target/modbox

clean:
	rm -rf build target

refresh: 
	rm -rf build target
	cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=on
	ln -f build/compile_commands.json compile_commands.json
