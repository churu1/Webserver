CXX ?= g++
CPP_FLAGS = '-std=c++11'
DEBUG ?= 1
ifeq ($(DEBUG), 1)
		CXXFLAGS += -g
else 
		CXXFLAGS += -O2
endif

server: main.cc ./Timer/Utils/Utils.cc   ./Timer/LstTimer/lst_timer.cc ./Http/http_conn.cc ./Log/log.cc ./CGImysql/sql_connection_pool.cc  webserver.cc config.cc
	$(CXX) ${CPP_FLAGS} -o server $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm -r server
