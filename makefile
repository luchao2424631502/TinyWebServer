CC		=g++
CXXFLAGS=-g -Wall -Iinclude/  
CXXLIB 	=-lpthread -lmysqlclient
SOURCE 	= main.cc config/config.cc locker/locker.cc \
		  timer/lst_timer.cc log/log.cc \
		  sql/connection_pool.cc http/http.cc \
		  web/webserver.cc
OBJ		=  main.o config.o locker.o lst_timer.o \
		   log.o connection_pool.o http.o webserver.o

all:clean $(OBJ) server 

server:$(OBJ)
	$(CC) -o server $^ $(CXXFLAGS) $(CXXLIB)

main.o:main.cc
	$(CC) -c $< $(CXXFLAGS) -o $@

config.o:config/config.cc
	$(CC) -c $< $(CXXFLAGS) -o $@

locker.o:locker/locker.cc
	$(CC) -c $< $(CXXFLAGS) -o $@

lst_timer.o:timer/lst_timer.cc
	$(CC) -c $< $(CXXFLAGS) -o $@

log.o:log/log.cc
	$(CC) -c $< $(CXXFLAGS) -o $@

connection_pool.o:sql/connection_pool.cc
	$(CC) -c $< $(CXXFLAGS) -o $@

http.o:http/http.cc
	$(CC) -c $< $(CXXFLAGS) -o $@

webserver.o:web/webserver.cc
	$(CC) -c $< $(CXXFLAGS) -o $@

clean:
	rm -f server *.o

.PHONY:clean
