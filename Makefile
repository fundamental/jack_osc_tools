all: jack_osclfo jack_oscsend

jack_osclfo: lfo.c Makefile
	gcc lfo.c -lm -lrtosc -ljack -std=c11 -o jack_osclfo -g

jack_oscsend: jack_oscsend.c Makefile
	gcc jack_oscsend.c -lrtosc -ljack -std=gnu11 -o jack_oscsend -g
