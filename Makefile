CXX	?= g++
SRC 	 = $(sort $(wildcard src/*.cpp))
OBJ 	 = $(SRC:.cpp=.o)
LIBS 	?= -lcpr -lalpm
LDFLAGS  = ${LIBS}
TARGET   = taur
CPPFLAGS = -ggdb -pedantic -funroll-all-loops -march=native -isystem include -Wall -std=c++17

is_cpr_installed := $(shell ldconfig -p | grep libcpr > /dev/null && echo -n yes)

# let's not build cpr everytime if it's already installed
ifneq ($(is_cpr_installed), yes)
all: cpr $(TARGET)
else
all: $(TARGET)
endif

cpr:
	#git submodule init
	#git submodule update --init --recursive
	#git -C $@ checkout 3b15fa8
	cmake -S $@ -B $@/build -DCMAKE_BUILD_TYPE=Release -DCPR_BUILD_TESTS=OFF -DCPR_USE_SYSTEM_CURL=ON
	cmake --build $@/build --parallel
	sudo cmake --install $@/build --prefix /usr

ifneq ($(is_cpr_installed), yes)
$(TARGET): cpr ${OBJ}
else
$(TARGET): ${OBJ}
endif
	${CXX} $(OBJ) $(CPPFLAGS) -o $@ $(LDFLAGS)

clean:
	rm -rf taur $(OBJ) cpr/build

.PHONY: cpr taur clean all
