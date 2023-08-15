LIBS := -lm
DEPS := \
https://raw.githubusercontent.com/seha-bot/packages/main/nec/nec.c \
https://raw.githubusercontent.com/seha-bot/packages/main/nec/nec.h

all: build build/main

build:
	@echo -Iinc > compile_flags.txt && echo -Ibuild/deps >> compile_flags.txt
	@mkdir -p build/deps && cd build/deps && curl -s $(addprefix -O ,$(DEPS))
	make $(patsubst %.c,build/deps/%.o,$(filter %.c,$(notdir $(DEPS))))

build/deps/%.o: build/deps/%.c
	@gcc -c -Iinc -Ibuild/deps $^ -o build/$(notdir $@)
build/%.o: src/%.c
	@gcc -g -c -Iinc -Ibuild/deps $^ -o build/$(notdir $@)

build/main: $(patsubst src/%.c,build/%.o,$(wildcard src/*.c))
	@gcc -g build/*.o $(LIBS) -o $@

clean:
	rm -rf build compile_flags.txt
