CXX	?= g++
SRC 	 = $(sort $(wildcard src/*.cpp))
OBJ 	 = $(SRC:.cpp=.o)
LIBS 	?= -lgit2 -lcpr 
LDFLAGS  = ${LIBS}
TARGET   = taur
CPPFLAGS = -isystem include -std=c++17

all: $(TARGET)

$(TARGET): ${OBJ}
	${CXX} $(OBJ) $(CPPFLAGS) -o $@ $(LDFLAGS)

clean:
	rm -f taur src/*.o

.PHONY: taur clean all
