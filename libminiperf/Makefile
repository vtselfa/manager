CPPFLAGS += -I. -I../linux/tools/include -I../linux/tools/lib -I../linux/tools/perf -I../linux/tools/perf/util

CFLAGS += -Wall -g
CXXFLAGS += -Wall -g

LIBS = -lpthread -lrt -lm -ldl -lbfd

TARGET = libminiperf.a
TARGET_TEST = $(TARGET:.a=-test)

OBJS = $(TARGET:.a=.o) dummy.o ../linux/tools/perf/libperf-in.o ../linux/tools/lib/api/libapi-in.o ../linux/tools/perf/pmu-events/pmu-events-in.o ../linux/tools/lib/traceevent/libtraceevent-in.o ../linux/tools/lib/subcmd/libsubcmd-in.o


all: $(TARGET) $(TARGET_TEST)

$(TARGET): $(OBJS)
	ar rcs $@ $^

$(TARGET_TEST): $(addsuffix .o, $(TARGET_TEST)) $(TARGET)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o *.a $(TARGET_TEST)

include ../rules.mk
