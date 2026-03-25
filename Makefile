CXX = g++
CXXFLAGS = -Wall -O2 -std=c++17

# totally not shamelessly copied from somewhere in the internet
OPENCV_MODULE := $(shell pkg-config --exists opencv4 && echo "opencv4" || echo "opencv")

CXXFLAGS += $(shell pkg-config --cflags $(OPENCV_MODULE))
LDFLAGS  += $(shell pkg-config --libs $(OPENCV_MODULE)) -pthread -ldl -lm


SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
TARGET = badapple

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
