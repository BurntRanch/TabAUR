CXX	?= g++
SRC 	 = $(sort $(wildcard src/*.cpp))
OBJ 	 = $(SRC:.cpp=.o)
LIBS 	?= -lgit2 -lcpr
LDFLAGS  = ${LIBS}
TARGET   = taur
CPPFLAGS = -isystem include -std=c++17

all: semver cpr $(TARGET)

cpr:
	cmake -S $@ -B $@/build -DCMAKE_BUILD_TYPE=Release -DCPR_BUILD_TESTS=OFF -DCPR_USE_SYSTEM_CURL=ON
	cmake --build $@/build --parallel
	#sudo cmake --install --prefix=/usr $@/build

semver:
	cmake -S $@ -B $@/build -DCMAKE_BUILD_TYPE=Release
	cmake --build $@/build --parallel
	#sudo cmake --install --prefix=/usr $@/build

$(TARGET): semver cpr ${OBJ}
	${CXX} $(OBJ) $(CPPFLAGS) -o $@ $(LDFLAGS)

clean:
	rm -rf taur src/*.o cpr/build

.PHONY: cpr semver taur clean all
