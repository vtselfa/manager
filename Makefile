CPPFLAGS += -I/home/viselol/src/intel-cmt-cat/lib -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/lib/glib-2.0/include/
CPPFLAGS += -Lintel-cmt-cat/lib -Lintel-pcm/lib

CXXFLAGS += -Wall -g -O0 -std=c++0x

LIBS= -pthread -lrt -lboost_system -lboost_filesystem -lyaml-cpp -lpqos -lboost_program_options -lglib-2.0 -lpcm


manager: cat-intel.o cat-policy.o common.o config.o events-intel.o manager.o kmeans.o stats.o task.o
	make -C intel-pcm/lib
	make -C intel-cmt-cat SHARED=0
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LIBS)

clean:
	make -C intel-pcm/lib clean
	make -C intel-cmt-cat clean
	rm -rf *.o manager
