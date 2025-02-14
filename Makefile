CC := armv7l-linux-musleabihf-gcc
CXX := armv7l-linux-musleabihf-g++
LD := armv7l-linux-musleabihf-ld

# Directories
SRC_DIR = .

# Source files
SRCS = $(SRC_DIR)/cinematic_brightness/cinematic_brightness.c $(SRC_DIR)/common/functions.c
OBJS = $(SRCS:.c=.o)

# Target executable
TARGET = cinematic_brightness/cinematic_brightness

# Build target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET)

# Rebuild everything
rebuild: clean all

