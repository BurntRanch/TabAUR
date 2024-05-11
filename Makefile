CXX       ?= g++
PREFIX	  ?= /usr
LOCALEDIR ?= $(PREFIX)/share/locale
VARS  	  ?= -DENABLE_NLS=1

VERSION    = 0.6.4
BRANCH     = libalpm-test
SRC 	   = $(sort $(wildcard src/*.cpp))
OBJ 	   = $(SRC:.cpp=.o)
LDFLAGS   := -L./src/fmt -L./src/cpr -lcpr -lalpm -lfmt -lidn2 -lssh2 -lcurl -lssl -lcrypto -lpsl -lgssapi_krb5 -lzstd -lbrotlidec -lz
CXXFLAGS  := -ggdb -pedantic -funroll-all-loops -march=native -isystem include -Wall -std=c++20 $(VARS) -DVERSION=\"$(VERSION)\" -DBRANCH=\"$(BRANCH)\" -DLOCALEDIR=\"$(LOCALEDIR)\"

is_cpr_installed = $(shell ldconfig -p | grep libcpr > /dev/null && echo -n yes)

all: cpr fmt toml taur

cpr:
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
ifeq ($(wildcard src/fmt/libfmt.a),)
	make -C src/fmt
endif

toml:
ifeq ($(wildcard src/toml++/toml.o),)
	make -C src/toml++
endif

locale:
	scripts/make_mo.sh locale/

taur: cpr fmt toml ${OBJ}
	${CXX} $(OBJ) src/toml++/toml.o $(CPPFLAGS) -o $@ $(LDFLAGS)

clean:
	rm -rf taur $(OBJ) cpr/build
#	make -C src/fmt clean

install: taur locale
	install taur -Dm 755 -v $(DESTDIR)$(PREFIX)/bin/taur
	find locale -type f -exec install -Dm 755 "{}" "$(DESTDIR)$(PREFIX)/share/{}" \;

.PHONY: cpr taur clean fmt toml locale install all
