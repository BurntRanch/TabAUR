# Variables taken from {fmt} cmake build files
# cmd: cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON

CXX		?= g++
CXX_DEFINES 	= -DFMT_LIB_EXPORT -Dfmt_EXPORTS
CXX_INCLUDES 	= -I../../include
CXX_FLAGS 	= -O3 -DNDEBUG -fPIC -fvisibility=hidden -fvisibility-inlines-hidden

all: fmt

fmt: format.cc os.cc
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -c format.cc -o format.cc.o
	$(CXX) $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -c os.cc -o os.cc.o
	$(CXX) -fPIC -O3 -DNDEBUG -shared -Wl,-soname,libfmt.so.10 -o libfmt.so format.cc.o os.cc.o

clean:
	rm -f *.o *.so

.PHONY: clean all fmt
