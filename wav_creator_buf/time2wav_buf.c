/*
 * PDP-8 Music Demonstration
 * Converts time values of CAF instructions to a WAV file
 * Kyle Owen - 4 April 2017
 * Compile with gcc -o time2wav time2wav.c -lsamplerate
 * Usage: time2wav yankee.txt yankee.wav
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>
#include <portaudio.h>

#define PULSE_LENGTH 7

#define VOLUME 0.8f

#define MAX_INPUT_SIZE 4096
#define MAX_OUTPUT_SIZE 512

#define OUTPUT_SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (64)

int fill_buffer(char init);

FILE *infile;
FILE *outfile;

struct __attribute__((__packed__)) wav_header {
	char chunk_id[4];
	unsigned int chunk_size;
	char chunk_format[4];
	char subchunk_id[4];
	unsigned int subchunk_size;
	unsigned short audio_fmt;
	unsigned short num_channels;
	unsigned int sample_rate;
	unsigned int byte_rate;
	unsigned short block_align;
	unsigned short bits_per_samp;
	char data_subchunk_id[4];
	unsigned int data_subchunk_size;
} WAV_HEADER;

float input_data[MAX_INPUT_SIZE];
float output_data0[MAX_OUTPUT_SIZE];
float output_data1[MAX_OUTPUT_SIZE];

typedef struct
{
	unsigned long playback_ptr;
	unsigned long total_frames;
	float *output_data;
	float normalizer;
	char message[20];
}
paUserData;


static int patestCallback( const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData )
{
	
    paUserData *data = (paUserData*)userData;
    float *out = (float*)outputBuffer;
    unsigned long i;

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    
    for( i=0; i<framesPerBuffer; i++ )
    {
		if (data->playback_ptr < data->total_frames)
		{
			*out++ = data->output_data[data->playback_ptr++] / data->normalizer;
		}
		else
		{
			return paComplete;
		}
    }
    
    return paContinue; //or paComplete to end
}

/*
 * This routine is called by portaudio when playback is done.
 */
static void StreamFinished( void* userData )
{
   paUserData *data = (paUserData *) userData;
   printf( "Stream Completed: %s\n", data->message );
}

char writeWAV = 0;

int fill_buffer(char *init)
{
	unsigned long val;
	static unsigned long cur_time;
	static unsigned long next_pulse;
	static int pulses_left;
	static int input_ptr;
	static int empty_frames_left;
	if (*init)
	{
		fscanf(infile, "%ld", &val);	//discard first value
		val = 0;
		cur_time = 0;
		next_pulse = 0;
		pulses_left = PULSE_LENGTH;
		input_ptr = 0;
		empty_frames_left = 0;
		*init = 0;
	}

	memset(input_data, 0, MAX_INPUT_SIZE * sizeof(float));

	if (empty_frames_left--)
	{
		return MAX_INPUT_SIZE;
	}

	do 
	{
		for (pulses_left; input_ptr < MAX_INPUT_SIZE && pulses_left > 0; pulses_left--)
			input_data[input_ptr++] = 1.0f;
		if (pulses_left)
		{
			next_pulse = 0;
			return MAX_INPUT_SIZE;
		}
		pulses_left = PULSE_LENGTH;
		if (feof(infile))
			return input_ptr;
		fscanf(infile, "%ld", &val);
		cur_time += val;
		empty_frames_left = ((val / 10) + (((val % 10) < 5) ? 0 : 1)) / MAX_INPUT_SIZE;
		next_pulse = ((cur_time / 10) + (((cur_time % 10) < 5) ? 0 : 1)) % MAX_INPUT_SIZE;
		input_ptr = next_pulse;
		if (empty_frames_left--)
			return MAX_INPUT_SIZE;
	} while (input_ptr < MAX_INPUT_SIZE);
}


int main(int argc, char* argv[])
{
	if (argc < 2 || argc > 3)
	{
		fprintf(stderr, "usage: %s [input] (output)\n", argv[0]);
		exit(0);
	}
	if (argc == 3)
		writeWAV = 1;

	fprintf(stderr, "opening %s for reading\n", argv[1]);
	if ((infile = fopen(argv[1], "r")) == NULL)
	{
		fprintf(stderr, "couldn't open %s for reading!\n", argv[1]);
		exit(-1);
	}

	if (writeWAV)
	{
		fprintf(stderr, "opening %s for writing\n", argv[2]);
		if ((outfile = fopen(argv[2], "w")) == NULL)
		{
			fprintf(stderr, "couldn't open %s for writing!\n", argv[2]);
			exit(-1);
		}

		memcpy(WAV_HEADER.chunk_id, "RIFF", 4);
		memcpy(WAV_HEADER.chunk_format, "WAVE", 4);
		memcpy(WAV_HEADER.subchunk_id, "fmt ", 4);
		memcpy(WAV_HEADER.data_subchunk_id, "data", 4);

		WAV_HEADER.subchunk_size = 16;
		WAV_HEADER.audio_fmt = 1;
		WAV_HEADER.num_channels = 1;
		WAV_HEADER.sample_rate = 44100;
		WAV_HEADER.byte_rate = WAV_HEADER.sample_rate * 2;
		WAV_HEADER.block_align = 2;
		WAV_HEADER.bits_per_samp = 16;
	}

	SRC_DATA src_data;
	SRC_STATE *src_state;
	int error;

	if ((src_state = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &error)) == NULL)
	{	
		fprintf(stderr, "error: src_new() failed: %s\n", src_strerror(error));
		exit(-1);
	}

	src_data.end_of_input = 0; /* Set this later. */

	/* Start with zero to force load in while loop. */
	src_data.input_frames = 0;

	src_data.data_in = input_data;
	src_data.data_out = output_data;
	src_data.output_frames = MAX_OUTPUT_SIZE;
	src_data.src_ratio = 0.0441;

	char init = 1;

	for (;;)
	{
		src_data.input_frames = fill_buffer(&init);
		if (src_data.input_frames < MAX_INPUT_SIZE)
			src_data.end_of_input = SF_TRUE;

		src_data.data_out = (src_data.data_out == output_data0) ? output_data1 : output_data0;

		if ((error = src_process(src_state, &src_data)))
		{
			fprintf(stderr, "error: src_process() failed: %s\n", src_strerror(error));
			exit(-1);
		}











    PaStreamParameters outputParameters;
    PaStream *stream;
    PaError err;
    paUserData data;

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default output device.\n");
      goto error;
    }
    outputParameters.channelCount = 1; 
    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

	data.output_data = output_data;
	data.normalizer = normalizer;
	data.total_frames = src_data.output_frames_gen;
	data.playback_ptr = 0;

    err = Pa_OpenStream(
              &stream,
              NULL, /* no input */
              &outputParameters,
              OUTPUT_SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              patestCallback,
              &data );
    if( err != paNoError ) goto error;

    sprintf( data.message, "No Message" );
    err = Pa_SetStreamFinishedCallback( stream, &StreamFinished );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

	while( ( err = Pa_IsStreamActive( stream ) ) == 1 ) {
		Pa_Sleep(1000);
	}
	if( err < 0 ) goto error;

/*
    err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;
*/
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;

    Pa_Terminate();
    printf("Test finished.\n");
	
	free(input_data);
	free(output_data);
	fclose(infile);
	fclose(outfile);
    
    return err;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}
