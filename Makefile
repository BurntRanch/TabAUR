CXX	?= g++
SRC 	 = $(sort $(wildcard src/*.cpp))
OBJ 	 = $(SRC:.cpp=.o)
LDFLAGS  = -L./src/fmt -L./src/cpr -lcpr -lalpm -lfmt -lssl -lcrypto -lcurl -lz -lidn2
TARGET   = taur
CPPFLAGS = -ggdb -pedantic -funroll-all-loops -march=native -isystem include -Wall -std=c++17

is_cpr_installed := $(shell ldconfig -p | grep libcpr > /dev/null && echo -n yes)

all: cpr fmt $(TARGET)

cpr:
# let's not build cpr everytime if it's already installed
ifneq ($(is_cpr_installed), yes)
	#git submodule init
	#git submodule update --init --recursive
	#git -C $@ checkout 3b15fa8
	mkdir -p src/cpr
	cmake -S $@ -B $@/build -DCMAKE_BUILD_TYPE=Release -DCPR_BUILD_TESTS=OFF -DCPR_USE_SYSTEM_CURL=OFF -DBUILD_SHARED_LIBS=OFF
	cmake --build $@/build --parallel
	mv -f $@/build/lib/*.a src/cpr/
# the absence of this one line didn't matter for a long time, despite it being critical, this caused toni to go mentally insane when trying to make changes to the way the project is being built.
	mv -f $@/build/cpr_generated_includes/cpr/cprver.h include/cpr/
	#sudo cmake --install $@/build --prefix /usr
endif

fmt:
ifeq (,$(wildcard ./src/fmt/libfmt.a))
	make -C src/fmt
endif

$(TARGET): cpr fmt ${OBJ}
	${CXX} $(OBJ) $(CPPFLAGS) -o $@ $(LDFLAGS)

clean:
	rm -rf taur $(OBJ) cpr/build
	make -C src/fmt clean

.PHONY: cpr $(TARGET) clean fmt all
