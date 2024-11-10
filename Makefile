CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -pthread

TARGET = telnirc
SRC = telnirc.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(TARGET)