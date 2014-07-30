all: lfo jack_oscsend

lfo: lfo.c 
	gcc lfo.c -lm -lrtosc -ljack -std=c11 -o lfo

jack_oscsend: jack_oscsend.c
	gcc jack_oscsend.c -lrtosc -ljack -std=gnu11 -o jack_oscsend
