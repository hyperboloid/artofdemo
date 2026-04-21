CXX = g++
CXXFLAGS = -std=c++11 -O2 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_mixer
TARGET = demo
SRCS = $(wildcard *.cpp)

$(TARGET): $(SRCS) vga.h timer.h texture.h vector.h matrix.h poly.h fire.h plasma.h roto.h hole.h particle.h flubber.h bump.h zoom.h tor.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
