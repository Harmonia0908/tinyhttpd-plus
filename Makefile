all: httpd client benchmark threadpool
LIBS = -pthread #-lsocket
HTTPD_SRCS = main.c server.c request.c response.c static_file.c cgi.c utils.c log.c config.c mime.c threadpool.c

httpd: $(HTTPD_SRCS)
	gcc -g3 -W -Wall $(LIBS) -o $@ $^

client: simpleclient.c
	gcc -W -Wall -o $@ $<

benchmark: benchmark.c
	gcc -W -Wall $(LIBS) -o $@ $<

threadpool: threadpool.c threadpool.h
	gcc -W -Wall $(LIBS) -c $< -o $@.o

clean:
	rm -f httpd client benchmark threadpool *.o
