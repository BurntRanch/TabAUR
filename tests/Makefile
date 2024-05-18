CXX       	?= g++
PREFIX	  	?= /usr
LOCALEDIR 	?= $(PREFIX)/share/locale
VARS  	  	?= -DENABLE_NLS=1
DEBUG 		?= 1

VERSION    	 = 0.6.7
BRANCH     	 = main
SRC              = $(sort $(wildcard *.cpp))
OBJ              = $(SRC:.cpp=)
LDFLAGS   	:= -lcpr -lalpm -lfmt -lidn2 -lssh2 -lcurl -lssl -lcrypto -lpsl -lgssapi_krb5 -lzstd -lbrotlidec -lz
CXXFLAGS  	:= -funroll-all-loops -mtune=generic -march=native -I../include -std=c++20 $(VARS) -DVERSION=\"$(VERSION)\" -DBRANCH=\"$(BRANCH)\" -DLOCALEDIR=\"$(LOCALEDIR)\"

# https://stackoverflow.com/a/1079861
# WAY easier way to build debug and release builds
ifeq ($(DEBUG), 1)
	BUILDDIR  = build/debug
	CXXFLAGS := -ggdb -pedantic -Wall $(CXXFLAGS)
else
	BUILDDIR  = build/release
	CXXFLAGS := -O2 $(CXXFLAGS)
endif

LDFLAGS  := -L./$(BUILDDIR)/fmt -L./$(BUILDDIR)/cpr $(LDFLAGS)

is_cpr_installed = $(shell ldconfig -p | grep libcpr > /dev/null && echo -n yes)

all: cpr fmt toml catch2 bin

cpr:
ifneq ($(is_cpr_installed), yes)
	mkdir -p $(BUILDDIR)/cpr
	cmake -S ../$@ -B ../$@/build -DCMAKE_BUILD_TYPE=Release -DCPR_BUILD_TESTS=OFF -DCPR_USE_SYSTEM_CURL=OFF -DBUILD_SHARED_LIBS=OFF
	cmake --build ../$@/build --parallel
	mv -f ../$@/build/lib/*.a $(BUILDDIR)/cpr
        # the absence of this one line didn't matter for a long time, despite it being critical, this caused toni to go mentally insane when trying to make changes to the way the project is being built.
	mv -f ../$@/build/cpr_generated_includes/cpr/cprver.h include/cpr/
endif

fmt:
ifeq ($(wildcard $(BUILDDIR)/fmt/libfmt.a),)
	mkdir -p $(BUILDDIR)/fmt
	make -C ../src/fmt BUILDDIR=tests/$(BUILDDIR)/fmt
endif

toml:
ifeq ($(wildcard $(BUILDDIR)/toml++/toml.o),)
	mkdir -p $(BUILDDIR)/toml++
	make -C ../src/toml++ BUILDDIR=tests/$(BUILDDIR)/toml++
endif

locale:
	../scripts/make_mo.sh locale/

catch2:
ifeq ($(wildcard $(BUILDDIR)/catch2/catch.o),)
	mkdir -p $(BUILDDIR)/catch2
	$(CXX) $(CXXFLAGS) catch2/catch_amalgamated.cpp -c -o $(BUILDDIR)/catch2/catch.o
endif

$(OBJ): $(BUILDDIR)/bin/% : $(SRC)
	mkdir -p $(BUILDDIR)/bin
	$(CXX) $(CXXFLAGS) $(BUILDDIR)/catch2/catch.o $@.cpp -o $(BUILDDIR)/bin/$@ $(LDFLAGS)

bin: catch2 cpr fmt toml $(OBJ)

clean:
	rm -rf $(BUILDDIR)/bin ../cpr/build

.PHONY: cpr clean fmt toml locale bin all
