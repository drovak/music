#include <portaudio.h>
#include <samplerate.h>
#include "pdp8_defs.h"

/* libsamplerate related things */
#define QUALITY SRC_SINC_FASTEST	/* SRC_SINC_FASTEST is the fastest sinc downsample converter */
#define PULSE_LENGTH	7		/* PDP-8 MUSIC.PA documentation says 6 microseconds, but 7 ensures no glitches caused by rounding errors */
#define INPUT_LENGTH	8192		/* can't be more than about 8200 or else the input buffer won't be completely consumed by libsamplerate */
#define OUTPUT_LENGTH	512		/* this must be greater than INPUT_LENGTH * (OUTPUT_SAMPLE_RATE / 1 MHz), which is about 362 */
#define NUMBER_OF_BUFFERS	16	/* buffer size can be increased or decreased depending on CPU load, but must be a power of 2 */

SRC_DATA src_data;
SRC_STATE *src_state;

int init_music;
unsigned long elapsed_time;
int pulse_ptr;
int pulses_left;
float input_data[INPUT_LENGTH];
float output_data[NUMBER_OF_BUFFERS][OUTPUT_LENGTH];
volatile int output_data_size[NUMBER_OF_BUFFERS];
volatile int current_ds_buf;

/* libportaudio related things */
#define OUTPUT_SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (64)
#define VOLUME 0.8f	/* volume can be tweaked higher until clipping occurs, then back down */

volatile int current_audio_buf;
int stream_started;
volatile int is_playing;
volatile int stream_error;

typedef struct
{
	int playback_ptr;
	char message[20];
}
paUserData;

PaStreamParameters outputParameters;
PaStream *stream;
PaError err;
paUserData data;

/* function prototypes */
void PaErrorFunc(PaError err);	/* returns a friendly error message for libportaudio */
static void StreamFinished( void* userData );	/* gets called upon completion of audio stream, probably not necessary anymore */
static int patestCallback( const void *inputBuffer, void *outputBuffer,	/* callback for libportaudio to get new data */
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData );
void do_init_music();	/* initializes music player */
void do_music();	/* creates pulse upon CAF instruction in input data array */
int calc_inst_time(int IR, int PC);	/* returns the time (in tenths of microseconds) for a given instruction at a given location in memory */
void make_buffer();	/* the do-everything function to wait for empty buffer, downsample the input buffer, and store in output buffer, etc. */
void pause_stream();	/* pauses stream if it was playing */
void play_stream();	/* restarts stream if it was halted from a pause_stream() */
