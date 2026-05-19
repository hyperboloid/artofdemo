CXX = g++
CXXFLAGS = -std=c++11 -O2 $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs)
TARGET = openai_snake
SRCS = $(wildcard *.cpp)

$(TARGET): $(SRCS) vga.h timer.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
