CXX      := g++
CXXFLAGS := -g -std=c++17 -O2 -Wall -Isrc \
            $(shell pkg-config --cflags sndfile lv2 lilv-0 serd-0 sord-0 sratom-0 ogg vorbis)

LDFLAGS  := $(shell pkg-config --libs sndfile lilv-0 serd-0 sord-0 sratom-0 ogg vorbis vorbisenc) \
            -lopus -lopusenc -lmp3lame -lFLAC -ldl -lpthread

SRCDIR   := src
TARGET   := opiqo

SRCS     := $(SRCDIR)/main.cpp \
            $(SRCDIR)/LiveEffectEngine.cpp \
            $(SRCDIR)/FileWriter.cpp \
            $(SRCDIR)/LockFreeQueue.cpp

OBJS     := $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)