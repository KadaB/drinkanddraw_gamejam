CC = clang
CFLAGS = -g -I ./include -I /usr/local/include/SDL3 # $(shell pkg-config --cflags sdl3)
LDFLAGS = -lm -L /usr/local/lib -lSDL3 # $(shell pkg-config --libs sdl3)

BUILD_DIR = build
SRC = code/main.c
OUT = $(BUILD_DIR)/game

all: $(OUT)

$(OUT): $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(OUT)

clean:
	rm -rf $(BUILD_DIR) # $(OUT)
