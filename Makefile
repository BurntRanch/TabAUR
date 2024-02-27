CXX	?= g++
SRC 	 = $(sort $(wildcard src/*.cpp))
OBJ 	 = $(SRC:.cpp=.o)
LIBS 	?= -lgit2 -lcpr -ltar -lz
LDFLAGS  = ${LIBS}
TARGET   = taur
CPPFLAGS = -g -isystem include

all: $(TARGET)

$(TARGET): ${OBJ}
	${CXX} $(OBJ) $(CPPFLAGS) -o $@ $(LDFLAGS)

clean:
	rm -f taur src/*.o

.PHONY: taur clean all
