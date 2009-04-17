# should be either OSC_HOST_BIG_ENDIAN or OSC_HOST_LITTLE_ENDIAN
# Apple: OSC_HOST_BIG_ENDIAN
# Win32: OSC_HOST_LITTLE_ENDIAN
# i386 LinuX: OSC_HOST_LITTLE_ENDIAN

ENDIANESS=OSC_HOST_LITTLE_ENDIAN

SDL_CFLAGS=$(shell sdl-config --cflags)
SDL_LDFLAGS=$(shell sdl-config --libs) -lSDL_image

INCLUDES=-I./TUIO -I./oscpack
CFLAGS=-Wall -O3 $(SDL_CFLAGS)
CXXFLAGS=$(CFLAGS) $(INCLUDES) -D$(ENDIANESS)
LDFLAGS=$(SDL_LDFLAGS) -lpthread

TUIO_SOURCES = ./TUIO/TuioClient.cpp ./TUIO/TuioServer.cpp ./TUIO/TuioTime.cpp
OSC_SOURCES = ./oscpack/osc/OscTypes.cpp ./oscpack/osc/OscOutboundPacketStream.cpp ./oscpack/osc/OscReceivedElements.cpp ./oscpack/osc/OscPrintReceivedElements.cpp ./oscpack/ip/posix/NetworkingUtils.cpp ./oscpack/ip/posix/UdpSocket.cpp

COMMON_SOURCES = $(TUIO_SOURCES) $(OSC_SOURCES)
COMMON_OBJECTS = $(COMMON_SOURCES:.cpp=.o)

all : water

water : $(COMMON_OBJECTS) water.cc
	g++ -o water $+ $(CXXFLAGS) $(LDFLAGS)

