CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread -Iinclude -g

# Define source files and targets
TARGET = telnirc
SRC = src/main.cpp src/telnirc.cpp src/telnerv.cpp src/config.cpp src/misc.cpp src/connection.cpp

.PHONY: all clean

all: $(TARGET)

# Rule to build telnirc executable
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f $(TARGET)
