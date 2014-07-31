#include <stdio.h>
#include <strings.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <regex.h>
#include <unistd.h>
#include <rtosc/rtosc.h>
#include "jack_osc.h"

void validate_path(const char *path)
{
    char c = *path++;
    if(c != '/')
        errx(1,"Path must start with '/'");

    while(isalnum(c) || c == '/')
        c = *path++;

    if(c)
        errx(1, "Invalid path character '%c'\n", c);
}

static int float_p(const char *str)
{
    int result = 0;
    while(*str && *str != ' ')
        result |= *str++ == '.';
    return result;
}

/*
 * string - s".*" (approximately)
 * char   - c\d+
 * int    - i\d+
 * float  - f(-?)\d+(\.\d)?
 * true   - T
 * false  - F
 * nil    - N
 * inf    - I
 */

char getArgType(const char *arg)
{
    assert(arg);
    assert(*arg);
    switch(*arg)
    {
        case 's':
        case 'S':
        case 'c':
        case 'i':
        case 'f':
        case 'T':
        case 'F':
        case 'N':
        case 'I':
            return *arg;
    };
    errx(1, "Invalid Argument \"%s\"", arg);
}

rtosc_arg_t getArgValue(const char *arg, char type)
{
    rtosc_arg_t tmp = {0};
    switch(type)
    {
        case 's':
        case 'S':
            tmp.s = arg;
            break;
        case 'c':
        case 'i':
            tmp.i = atoi(arg+1);
            break;
        case 'f':
            tmp.f = atof(arg+1);
            break;
    };
    return tmp;
}

/**
 * Parser Modified from oscprompt
 */
char *stringToOsc(int argc, char **argv)
{
    assert(argc >= 1);
    validate_path(argv[0]);

    const char *path = *argv++;
    argc--;

    char *typestring  = malloc(argc+1);
    rtosc_arg_t *args = malloc(argc+1);

    //Assume all remaining arguments are valid arguments
    unsigned nargs = argc;
    for(int i=0, j=0; j<argc;++i)
    {
        typestring[i] = getArgType(argv[j]);
        if(typestring[i] == 's' || typestring[i] == 'S') {
            if(j+1 == argc)
                errx(1, "Invalid String Arg");
            args[i] = getArgValue(argv[j+1], typestring[i]);
            j+=2,nargs--;
        } else {
            args[i] = getArgValue(argv[j], typestring[i]);
            ++j; 
        }
    }
    typestring[nargs] = '\0';

    size_t buf_size = rtosc_amessage(NULL, 0, 
            path, typestring, args);
    char *buffer = malloc(buf_size);
    rtosc_amessage(buffer, buf_size, path, typestring, args);
    return buffer;
}

const char *help =
   "Invalid number of parameters\n"
   "example:\n"
   "jack_oscsend application /path/to/stuff s\"string\" c12 i90 f12.3 T F I N\n";

//Jack
const char    *message;
size_t         message_size;
jack_port_t   *osc_out;
jack_client_t *client;
int process(jack_nframes_t nframes, void *arg);

int done = 0;

int main(int argc, char **argv)
{
    char *app;
    char *address;

    if(argc < 2)
    {
        fprintf(stderr, "%s", help);
        return 1;
    }
    argv++;
    argc--;

    if(!index(argv[0], ':')) {
        fprintf(stderr, "Invalid address\n");
    }
    app = argv[0];
    address = index(argv[0], ':');
    *address = 0;
    address++;

    //Get message data
    message = stringToOsc(argc-1, argv+1);
    assert(message);
    message_size = rtosc_message_length(message,-1);
    assert(message_size);

    const char *progname = "jack_oscsend";

    client = jack_client_open(progname, JackNullOption, NULL);
    if(!client)
        errx(1, "Failed to create JACK client");

    jack_set_process_callback (client, process, 0);
    osc_out = jack_port_register(client, "out", JACK_DEFAULT_OSC_TYPE, JackNoStartServer, 0);
    if(!osc_out)
        errx(1, "Failed to register JACK port");

    if(jack_activate(client))
        errx(1, "Failed to activate JACK client");

    char buf[256];
    strncpy(buf, app, 255);
    strncat(buf, ":osc", 255);
    jack_connect(client, jack_port_name(osc_out), buf);

    while(!done)
        usleep(10);

    jack_client_close(client);
    return 0;
}

int process(jack_nframes_t nframes, void *arg)
{
    (void) arg;
    void *out = jack_port_get_buffer(osc_out, nframes);


    jack_osc_clear_buffer(out);
    jack_nframes_t pos = 0;
    if(!done && message_size <= jack_osc_max_event_size(out)) {
        jack_osc_event_write(out, pos, (unsigned char*)message, message_size);
        done = true;
    }

    return 0;
}
