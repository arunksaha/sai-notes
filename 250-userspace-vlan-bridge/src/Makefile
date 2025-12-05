CXX = g++
CXXFLAGS = -std=c++17 -O2

EXE = userspace_switch
LDFLAGS = -pthread

OBJS = switch_state.o \
       switch_dataplane.o \
       switch_main.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(EXE)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o $(EXE)
