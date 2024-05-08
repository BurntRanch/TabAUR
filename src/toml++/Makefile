CXX	?= g++
SRC 	 = toml.cpp
TARGET   = toml.o
CPPFLAGS = -I../../include -std=c++20

all: $(TARGET)

$(TARGET):
	${CXX} $(SRC) $(CPPFLAGS) -c -o $@

clean:
	rm -rf $(TARGET)

.PHONY: $(TARGET) clean all
