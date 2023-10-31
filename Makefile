CC=gcc

INCS= -Iinc -I. -ISDL2/include -Iglew/include
LIBS= -L. -lportaudio -lopengl32 -lSDL2
CFLAGS= -Wall -Wextra -Wno-stringop-overflow -Wno-stringop-overread -g -std=c99\
				-fno-diagnostics-color $(INCS) $(LIBS) -DGLEW_STATIC


SRCS= $(wildcard src/*.c) 
OBJS= $(patsubst src/%.c,obj/%.o,$(SRCS)) obj/glew.o

TARGET= aleph.exe

.PHONY: run clean all

all: $(TARGET)

obj/%.o: src/%.c | obj
	$(CC) -c -o $@ $^ $(CFLAGS)

obj/glew.o: glew/src/glew.c
	$(CC) -c -o $@ $^ -Iglew/include -DGLEW_STATIC

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

obj: 
	@mkdir -p obj

run: $(TARGET)
	@./$(TARGET)

clean: 
	rm -rf obj
	rm $(TARGET)
