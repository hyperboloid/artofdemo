CXX = g++
CXXFLAGS = -std=c++11 -O2 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_image
TARGET = tor
SRCS = $(wildcard *.cpp)

$(TARGET): $(SRCS) vga.h timer.h texture.h vector.h matrix.h poly.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
