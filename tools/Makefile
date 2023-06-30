all : graph_values_over_time per_line_graph

LDFLAGS:=-lX11 -lpthread
CFLAGS:=-s -Os

per_line_graph : per_line_graph.c
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

graph_values_over_time : graph_values_over_time.c
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf *.o *~

install : all
	sudo cp per_line_graph /usr/local/bin/
	sudo cp graph_values_over_time /usr/local/bin/


