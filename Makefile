#TabAUR - description

SRC:=$(wildcard src/*.c)
OBJ = ${SRC:.c=.o}
LIBS = -lgit2
LDFLAGS  = ${LIBS}

all: taur

taur: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}
