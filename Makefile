
LDFLAGS= -lm -lglut -lGL -lGLU -lGLEW $(shell sdl2-config --libs) -lSDL2_image
CFLAGS= -O3 $(shell sdl2-config --cflags )

.PHONY: all
all: sdlcast

sdlcast: main.c map.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) 
