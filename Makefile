CPPFLAGS += -I. -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/lib/glib-2.0/include/ -Ifmt
CPPFLAGS += -Lintel-cmt-cat/lib -Lintel-pcm/lib -Lfmt/fmt
CPPFLAGS += -DBOOST_LOG_DYN_LINK

CXXFLAGS += -Wall -g -O0 -std=c++0x

LIBS= -lpthread -lrt -lboost_system -lboost_log -lboost_log_setup -lboost_thread -lboost_filesystem -lyaml-cpp -lpqos -lboost_program_options -lglib-2.0 -lpcm -lfmt


manager: cat-intel.o cat-policy.o common.o config.o events-intel.o log.o manager.o kmeans.o stats.o task.o
	make -C intel-pcm/lib
	make -C intel-cmt-cat SHARED=0
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LIBS)


clean:
	rm -rf *.o manager


distclean: clean
	make -C intel-pcm/lib clean
	make -C intel-cmt-cat clean
