CC=gcc
CFLAGS=-g -Wall 

proxy: proxy_server_with_cache.c proxy_parse.c proxy_parse.h
	$(CC) $(CFLAGS) -c proxy_parse.c
	$(CC) $(CFLAGS) -c proxy_server_with_cache.c -o proxy_server.o
	$(CC) $(CFLAGS) proxy_parse.o proxy_server.o -o proxy -lpthread

clean:
	rm -f proxy *.o

tar:
	tar -cvzf ass1.tgz proxy_server_with_cache.c README Makefile proxy_parse.c proxy_parse.h