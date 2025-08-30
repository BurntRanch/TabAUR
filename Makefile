CXX       	?= g++
PREFIX	  	?= /usr
LOCALEDIR 	?= $(PREFIX)/share/locale
VARS  	  	?= -DENABLE_NLS=1

DEBUG 		?= 0
# https://stackoverflow.com/a/1079861
# WAY easier way to build debug and release builds
ifeq ($(DEBUG), 1)
        BUILDDIR  = build/debug
        CXXFLAGS := -ggdb -pedantic -Wall $(DEBUG_CXXFLAGS) $(CXXFLAGS)
else
        BUILDDIR  = build/release
        CXXFLAGS := -O2 $(CXXFLAGS)
endif

VERSION    	 = 0.6.9
BRANCH     	?= $(shell git rev-parse --abbrev-ref HEAD)
SRC 	   	 = $(sort $(wildcard src/*.cpp))
OBJ 	   	 = $(SRC:.cpp=.o)
CURL_LIBS	?= -lcurl -lnghttp3 -lnghttp2 -lidn2 -lssh2 -lssl -lcrypto -lpsl -lgssapi_krb5 -lzstd -lbrotlidec -lz
LDFLAGS   	+= -L./$(BUILDDIR)/fmt/lib -L./$(BUILDDIR)/cpr/lib -lcpr -lalpm -lfmt $(CURL_LIBS)
CXXFLAGS  	?= -mtune=generic -march=native
CXXFLAGS	+= -funroll-all-loops -isystem include -std=c++23 $(VARS) -DVERSION=\"$(VERSION)\" -DBRANCH=\"$(BRANCH)\" -DLOCALEDIR=\"$(LOCALEDIR)\"

is_cpr_installed = $(shell ldconfig -p | grep libcpr > /dev/null || test -d $(BUILDDIR)/cpr && echo -n yes)

all: cpr fmt toml taur

cpr:
ifeq ($(wildcard $(BUILDDIR)/cpr/lib/libcpr.a),)
	mkdir -p $(BUILDDIR)/cpr
	mkdir -p include/cpr
	cmake -S external/cpr -B external/cpr/build -DCMAKE_BUILD_TYPE=Release -DCPR_BUILD_TESTS=OFF -DCPR_USE_SYSTEM_CURL=OFF -DBUILD_SHARED_LIBS=OFF
	cmake --build external/cpr/build
	cmake --install external/cpr/build --prefix $(BUILDDIR)/cpr
	cp external/cpr/build/cpr_generated_includes/cpr/cprver.h include/cpr/
endif

fmt:
ifeq ($(wildcard $(BUILDDIR)/fmt/libfmt.a),)
	mkdir -p $(BUILDDIR)/fmt
	cmake -S external/fmt -B external/fmt/build -DCMAKE_BUILD_TYPE=Release -DFMT_TEST=OFF
	cmake --build external/fmt/build
	cmake --install external/fmt/build --prefix $(BUILDDIR)/fmt
endif

toml:
ifeq ($(wildcard $(BUILDDIR)/toml++/toml.o),)
	mkdir -p $(BUILDDIR)/toml++
	cmake -S external/toml++ -B external/toml++/build -DCMAKE_BUILD_TYPE=Release
	cmake --build external/toml++/build
	# installs include files
	cp external/toml++/build/include/toml++ include
endif

locale:
	scripts/make_mo.sh locale/

taur: cpr fmt toml $(OBJ)
	mkdir -p $(BUILDDIR)
	$(CXX) $(OBJ) -o $(BUILDDIR)/taur $(LDFLAGS)

dist: taur locale
	bsdtar --zstd -cf TabAUR-v$(VERSION).tar.zst LICENSE README.md locale/ -C $(BUILDDIR) taur

clean:
	rm -rf $(BUILDDIR)/taur $(OBJ) external/cpr/build

distclean:
	rm -rf $(BUILDDIR) ./tests/$(BUILDDIR) $(OBJ) external/cpr/build
	find . -type f -name "*.tar.zst" -exec rm -rf "{}" \;
	find . -type f -name "*.o" -exec rm -rf "{}" \;
	find . -type f -name "*.a" -exec rm -rf "{}" \;
	make -C tests/ clean

install: taur locale
	install $(BUILDDIR)/taur -Dm 755 -v $(DESTDIR)$(PREFIX)/bin/taur
	find locale -type f -exec install -Dm 755 "{}" "$(DESTDIR)$(PREFIX)/share/{}" \;

test:
	make -C tests

.PHONY: cpr taur clean fmt toml locale install all
