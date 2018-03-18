test: example.elf
	./example.elf

example.elf: example.cpp gc.hpp
	$(CXX) -Os -std=c++1z -Wall -Wextra -Werror -Wno-non-template-friend -o "$@" "$<"

clean:
	rm -f example.elf
