CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror -I../include

all: preview test_scene

preview: preview.cpp ../include/retro_scene.h
	$(CXX) $(CXXFLAGS) preview.cpp -o preview

test_scene: test_scene.cpp ../include/retro_scene.h
	$(CXX) $(CXXFLAGS) test_scene.cpp -o test_scene

run: preview
	./preview

test: test_scene
	./test_scene

clean:
	rm -f preview test_scene preview-*.ppm
