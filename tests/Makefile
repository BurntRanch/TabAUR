CXX       	?= g++
PREFIX	  	?= /usr
LOCALEDIR 	?= $(PREFIX)/share/locale
VARS  	  	?= -DENABLE_NLS=1
DEBUG 		?= 0

SRC_DIR = ../src
TEST_DIR = .

VERSION    	 = 0.6.9
BRANCH     	 = main
SRC 		 = $(filter-out $(SRC_DIR)/main.cpp, $(wildcard $(SRC_DIR)/*.cpp))
OBJ 		 = $(patsubst $(SRC_DIR)/%.cpp, $(BUILDDIR)/%.o, $(SRC))
TESTS 		 = $(patsubst $(SRC_DIR)/%.cpp, $(TEST_DIR)/test_%, $(SRC))
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
#ifeq ($(wildcard $(BUILDDIR)/catch2/catch.o),)
ifneq ($(shell test -e $(BUILDDIR)/catch2/catch.o && echo -n yes), yes)
	mkdir -p $(BUILDDIR)/catch2
	$(CXX) $(CXXFLAGS) catch2/catch_amalgamated.cpp -c -o $(BUILDDIR)/catch2/catch.o
endif

$(BUILDDIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_DIR)/test_%: $(BUILDDIR)/catch2/catch.o $(OBJ) $(TEST_DIR)/test_%.cpp
	mkdir -p $(TEST_DIR)
	$(CXX) $(CXXFLAGS) $(BUILDDIR)/toml++/toml.o $^ -o $@ $(LDFLAGS)

bin: $(TESTS)

clean:
	rm -rf $(TESTS) $(BUILDDIR)/*.o ../cpr/build

.PHONY: cpr clean catch2 fmt toml locale bin all
