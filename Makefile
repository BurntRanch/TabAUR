CXX       	?= g++
PREFIX	  	?= /usr
LOCALEDIR 	?= $(PREFIX)/share/locale
VARS  	  	?= -DENABLE_NLS=1
DEBUG 		?= 0

VERSION    	 = 0.6.4
BRANCH     	 = main
SRC 	   	 = $(sort $(wildcard src/*.cpp))
OBJ 	   	 = $(SRC:.cpp=.o)
LDFLAGS   	:= -lcpr -lalpm -lfmt -lidn2 -lssh2 -lcurl -lssl -lcrypto -lpsl -lgssapi_krb5 -lzstd -lbrotlidec -lz
CXXFLAGS  	:= -funroll-all-loops -mtune=generic -march=native -isystem include -std=c++20 $(VARS) -DVERSION=\"$(VERSION)\" -DBRANCH=\"$(BRANCH)\" -DLOCALEDIR=\"$(LOCALEDIR)\"

# https://stackoverflow.com/a/1079861
# WAY easier way to build debug and release builds
ifeq ($(DEBUG), 1)
	BUILDDIR  = build/debug
	CXXFLAGS := -ggdb -pedantic -Wall $(CXXFLAGS)
else
	BUILDDIR  = build/release
	CXXFLAGS := -O3 $(CXXFLAGS)
endif

LDFLAGS  := -L./$(BUILDDIR)/fmt -L./$(BUILDDIR)/cpr $(LDFLAGS)

is_cpr_installed = $(shell ldconfig -p | grep libcpr > /dev/null && echo -n yes)

all: cpr fmt toml taur

cpr:
ifneq ($(is_cpr_installed), yes)
        #git submodule init
        #git submodule update --init --recursive
        #git -C $@ checkout 3b15fa8
	mkdir -p $(BUILDDIR)/cpr
	cmake -S $@ -B $@/build -DCMAKE_BUILD_TYPE=Release -DCPR_BUILD_TESTS=OFF -DCPR_USE_SYSTEM_CURL=OFF -DBUILD_SHARED_LIBS=OFF
	cmake --build $@/build --parallel
	mv -f $@/build/lib/*.a $(BUILDDIR)/cpr
        # the absence of this one line didn't matter for a long time, despite it being critical, this caused toni to go mentally insane when trying to make changes to the way the project is being built.
	mv -f $@/build/cpr_generated_includes/cpr/cprver.h include/cpr/
        #sudo cmake --install $@/build --prefix /usr
endif

fmt:
ifeq ($(wildcard $(BUILDDIR)/fmt/libfmt.a),)
	mkdir -p $(BUILDDIR)/fmt
	make -C src/fmt BUILDDIR=$(BUILDDIR)/fmt
endif

toml:
ifeq ($(wildcard $(BUILDDIR)/toml++/toml.o),)
	mkdir -p $(BUILDDIR)/toml++
	make -C src/toml++ BUILDDIR=$(BUILDDIR)/toml++
endif

locale:
	scripts/make_mo.sh locale/

taur: cpr fmt toml $(OBJ)
	mkdir -p $(BUILDDIR)
	$(CXX) $(OBJ) $(BUILDDIR)/toml++/toml.o -o $(BUILDDIR)/taur $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)/taur $(OBJ) cpr/build
#	make -C src/fmt clean

install: taur locale
	install $(BUILDDIR)/taur -Dm 755 -v $(DESTDIR)$(PREFIX)/bin/taur
	find locale -type f -exec install -Dm 755 "{}" "$(DESTDIR)$(PREFIX)/share/{}" \;

.PHONY: cpr taur clean fmt toml locale install all
