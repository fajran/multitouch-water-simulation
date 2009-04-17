all : water

water : water.cc
	g++ -o water water.cc -lSDL -lSDL_image
