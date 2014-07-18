#include <stdio.h>
#include <strings.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <string.h>
#include <rtosc/rtosc.h>
#include "jack_osc.h"

int   shape;
float freq;
float amp;
float offset;
char  type;
int   oversample;

char *app;
char *address;
char buffer[1024];

const char *help =
   "Invalid number of parameters\n"
   "Required parameters\n"
   "\tType(sin)\n"
   "\tFrequency in Hertz\n"
   "\tTotal Amplitude\n"
   "\tOffset\n"
   "\tOSC Address\n"
   "\tType Class\n"
   "\tOversample rate (events per JACK frame)\n";

//Jack
jack_port_t   *osc_out;
jack_client_t *client;
int process(jack_nframes_t nframes, void *arg);
void jack_shutdown (void *arg);

sig_atomic_t do_run = 1;
void sighandler(int sig)
{
    (void) sig;
    do_run = 0;
}


//    Amplitude (Total)
//                    |
//prog Shape freq(Hz) | Offset Address       Type Oversample
//         |        | |     |        |          |          |
//argv[0] sin      1 10     0  blop:/foo/bar    f          1
int main(int argc, char **argv)
{
    if(argc != 8)
    {
        fprintf(stderr, "%s", help);
        return 1;
    }

    //XXX Currently unread params are type/type/oversample
    shape      = 0;
    type       = 'c';
    oversample = 1;
    freq   = atof(argv[2]);
    amp    = atof(argv[3]);
    offset = atof(argv[4]);
    printf("%fHz %f->%f\n", freq, offset-amp/2, offset+amp/2);
    if(!index(argv[5], ':')) {
        fprintf(stderr, "Invalid address\n");
    }
    app = argv[5];
    address = index(argv[5], ':');
    *address = 0;
    address++;

    const char *progname = "osc_lfo";

    client = jack_client_open (progname, JackNullOption, NULL);
    if(!client)
        errx(1, "Failed to create JACK client");

    jack_set_process_callback (client, process, 0);
    osc_out = jack_port_register (client, "out", JACK_DEFAULT_OSC_TYPE, JackPortIsOutput, 0);
    if(!osc_out)
        errx(1, "Failed to register JACK port");


    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGHUP,  sighandler);
    signal(SIGINT,  sighandler);

    if(jack_activate(client))
        errx(1, "Failed to activate JACK client");

    char buf[256];
    strncpy(buf, app, 255);
    strncat(buf, ":osc", 255);
    jack_connect(client, jack_port_name(osc_out), buf);

    while(do_run)
        sleep(1);

    jack_client_close(client);
    return 0;
}

float osc_state;

#define PI 3.141592653

int process(jack_nframes_t nframes, void *arg)
{
    (void) arg;
    const double Fs = jack_get_sample_rate(client);
    void *out = jack_port_get_buffer(osc_out, nframes);

    jack_osc_clear_buffer(out);
    jack_nframes_t pos = 0;
    int val = offset+amp/2*sin(2*PI*osc_state);
    if(val > 127)
        val = 127;
    if(val < 0)
        val = 0;
    size_t msg_size    =
        rtosc_message(buffer, sizeof(buffer), address, "c", val);
    //printf("sending %d\n", rtosc_argument(buffer, 0).i);
    if(msg_size <= jack_osc_max_event_size(out))
        jack_osc_event_write(out, pos, (unsigned char*)buffer, msg_size);

    const double dt = freq/Fs;
    for(unsigned i=0; i<nframes; ++i,osc_state+=dt) ;
    osc_state  = fmod(osc_state,1.0);

    return 0;
}

void jack_shutdown (void *arg)
{
    (void) arg;
    exit(1);
}
