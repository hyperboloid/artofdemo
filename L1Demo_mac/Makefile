PIC_SRC := ../L1Demo_Getting_Started.X

CC      := cc
CFLAGS  := -Wall -Wno-unused-parameter -Wno-unknown-attributes -Wno-macro-redefined \
           -O2 -I. -I$(PIC_SRC) -include pic_compat.h $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs) -lm

# Host-side sources (SDL2 backend + PIC SFR stubs)
HOST_SRCS := main_sdl.c gpu_sdl.c audio_stub.c pic_sfr.c

# Sources copied straight from the PIC project (compiled via -I path above)
PIC_SRCS  := $(PIC_SRC)/scene.c $(PIC_SRC)/sprites.c $(PIC_SRC)/alu.c $(PIC_SRC)/color.c

SRCS := $(HOST_SRCS) $(PIC_SRCS)
TARGET := l1demo_mac

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
