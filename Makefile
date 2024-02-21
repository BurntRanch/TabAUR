#TabAUR - description

SRC:=$(wildcard src/*.c)

all: taur

taur:
	${CC} -o $@ ${SRC}
