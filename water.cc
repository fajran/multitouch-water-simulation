#include <stdio.h>
#include "SDL/SDL.h"
#include "SDL/SDL_image.h"
#include <string>

const int WIDTH = 640;
const int HEIGHT = 480;
const int DEPTH = 32;

const int HWIDTH = WIDTH >> 1;
const int HHEIGHT = HEIGHT >> 1;

typedef short Int16;

SDL_Surface* image = NULL;
SDL_Surface* screen = NULL;
Int16 ripplemap[WIDTH*(HEIGHT+2)*2];
Uint32 ripple[WIDTH*HEIGHT];
Uint32 texture[WIDTH*HEIGHT];
int oldind = WIDTH;
int newind = WIDTH * (HEIGHT+3);
int riprad = 3;

SDL_Surface* load_image(std::string fname)
{
	SDL_Surface* res = NULL;
	res = IMG_Load(fname.c_str());
	if (res) {
		res = SDL_DisplayFormat(res);
		if (res) {
			SDL_SetColorKey(res, SDL_SRCCOLORKEY, SDL_MapRGB(res->format, 0, 0xFF, 0xFF));
		}
	}
	return res;
}

void apply_surface(int x, int y, SDL_Surface* src, SDL_Surface *dst)
{
	SDL_Rect pos;
	pos.x = x;
	pos.y = y;
	SDL_BlitSurface(src, NULL, dst, &pos);
}

void frame()
{
	int tmp;
	tmp = oldind;
	oldind = newind;
	newind = tmp;

	int pos = 0;
	int x, y;
	int a, b;

	int mapind = oldind;
	
	Uint32* pimg = (Uint32*)image->pixels;
	Uint32* pscr = (Uint32*)screen->pixels; 

	for (y=0; y<HEIGHT; y++) {
		for (x=0; x<WIDTH; x++) {
			Int16 data = (Int16)((ripplemap[mapind-WIDTH] +
				ripplemap[mapind+WIDTH] +
				ripplemap[mapind-1] +
				ripplemap[mapind+1]) >> 1);
			data -= ripplemap[newind+pos];
			data -= data >> 5;
			ripplemap[newind+pos] = data;

			data = (Int16)(1024-data);

			a = ((x - HWIDTH) * data / 1024) + HWIDTH;
			b = ((y - HHEIGHT) * data / 1024) + HHEIGHT;

			if (a >= WIDTH) { a = WIDTH - 1; }
			if (a < 0) { a = 0; }
			if (b >= HEIGHT) { b = HEIGHT - 1; }
			if (b < 0) { b = 0; }

			pscr[pos] = pimg[a+b*WIDTH];
			//screen->pixels[pos] = image->pixels[a + b*WIDTH];
			mapind++;
			pos++;
		}
	}
}

void disturb(int x, int y)
{
	int i, j;
	for (j=y-riprad; j<y+riprad; j++) {
		for (i=x-riprad; i<x+riprad; i++) {
			if ((j>=0) && (j<HEIGHT) && (i>=0) && (i<WIDTH)) {
				ripplemap[oldind+(j*WIDTH)+i] += 512;
			}
		}
	}
}

int main(int argc, char** argv)
{
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		return 1;
	}

	screen = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_SWSURFACE);
	if (screen == NULL) {
		return 1;
	}

	image = load_image(argc > 1 ? argv[1] : "img.png");
	apply_surface(0, 0, image, screen);

	SDL_WM_SetCaption("Water Simulation", NULL);

	SDL_Event event;

	bool quit = false;
	while (!quit) {
		if (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_MOUSEMOTION:
					disturb(event.button.x, event.button.y);
					break;
				case SDL_QUIT:
					quit = true;
					break;
			}
		}

		frame();

		if (SDL_Flip(screen) == -1) {
			return 1;
		}
	}
		
	SDL_Quit();

	return 0;
}
