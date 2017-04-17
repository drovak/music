/*
 * PDP-8 Music Demonstration
 * Converts time values of CAF instructions to a WAV file
 * Kyle Owen - 4 April 2017
 * Compile with gcc -o time2wav time2wav.c -lsamplerate -lportaudio
 * Usage: time2wav yankee.txt 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <samplerate.h>
#include <portaudio.h>

#define PULSE_LENGTH 7

//#define QUALITY SRC_LINEAR
//#define QUALITY SRC_ZERO_ORDER_HOLD
#define QUALITY SRC_SINC_FASTEST
//#define QUALITY SRC_SINC_MEDIUM_QUALITY

#define AUDIO_DELAY 0

#define VOLUME 0.8f

#define BUF_WINDOW_SIZE 8192
//#define MAX_INPUT_SIZE 300000000
#define MAX_INPUT_SIZE 8192
#define MAX_OUTPUT_SIZE 30000000

#define OUTPUT_SAMPLE_RATE (44100)
#define FRAMES_PER_BUFFER (64)

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

float *input_data;
float *output_data;

unsigned long long last_data = 0;
int done = 0;

typedef struct
{
	unsigned long long playback_ptr;
	unsigned long long total_frames;
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
		if (data->playback_ptr < last_data)
		{
			*out = data->output_data[data->playback_ptr++] * data->normalizer;
			if (*out > 1)
			{
				fprintf(stderr, "clipping detected! %f\n", *out);
			}
			out++;
		}
		else
		{
			if (!done)
				fprintf(stderr, "buffer overrun!\n");
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


	while (!feof(infile))	//probably want some bounds checking in the loop...
	{
		fscanf(infile, "%ld", &val);
		for (int i = 0; i < PULSE_LENGTH; i++)
			input_data[(cur_time / 10) + i + (((cur_time % 10) < 5) ? 0 : 1)] = 1.0f;
		cur_time += val;
	}
















































int fill_buffer(char *init, float *array)
{
	static unsigned long val;
	static int pulse_ticks_left;
	static int pulse_ptr;

	if (*init)
	{
		fscanf(infile, "%ld", &val);	//discard first value
		val = 0;
		pulse_ptr = 0;
		pulse_ticks_left = PULSE_LENGTH;
		*init = 0;
	}

	memset(input_data, 0, MAX_INPUT_SIZE * sizeof(float));

	int buf_empty_space = MAX_INPUT_SIZE;

	if (pulse_ticks_left)
	{
		buf_empty_space -= pulse_ticks_left;
		for (int i = pulse_ptr; pulse_ticks_left > 0 && i < MAX_INPUT_SIZE; i++)
		{
			array[i] = 1.0f;
			pulse_ticks_left--;
		}
	}

	do
	{
		if (feof(infile))
			break;
		if (val == 0)
			fscanf(infile, "%ld", &val);
		if (val > buf_empty_space)
		{
			val -= buf_empty_space;
			return MAX_INPUT_SIZE;
		}
		else
		{
			buf_empty_space -= val;
			pulse_ptr += val;
			pulse_ptr %= MAX_INPUT_SIZE;
			val = 0;
			pulse_ticks_left = PULSE_LENGTH;
			for (int i = pulse_ptr; pulse_ticks_left > 0 && i < MAX_INPUT_SIZE; i++)
			{
				array[i] = 1.0f;
				pulse_ticks_left--;
				buf_empty_space--;
			}
		}
	}
	while (buf_empty_space);

	return MAX_INPUT_SIZE - buf_empty_space;
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

	fprintf(stderr, "calling malloc() for input data\n");
	input_data = (float *) malloc(MAX_INPUT_SIZE * sizeof(float));
	if (input_data == NULL)
	{
		fprintf(stderr, "Unable to allocate input array!\n");
		exit(-1);
	}
	memset(input_data, 0, MAX_INPUT_SIZE * sizeof(float));
	
	/*
	unsigned long val;
	unsigned long cur_time = 0;

	fprintf(stderr, "reading input file\n");
	fscanf(infile, "%ld", &val);
	while (!feof(infile))	//probably want some bounds checking in the loop...
	{
		fscanf(infile, "%ld", &val);
		for (int i = 0; i < PULSE_LENGTH; i++)
			input_data[(cur_time / 10) + i + (((cur_time % 10) < 5) ? 0 : 1)] = 1.0f;
		cur_time += val;
	}
	fprintf(stderr, "current time: %lu\n", cur_time);*/

	fprintf(stderr, "calling malloc() for output data\n");
	output_data = (float *) malloc(MAX_OUTPUT_SIZE * sizeof(float));
	if (output_data == NULL)
	{
		fprintf(stderr, "Unable to allocate output array!\n");
		exit(-1);
	}
	fprintf(stderr, "allocated output data at %p\n", (void*) output_data);

	SRC_DATA src_data;
	SRC_STATE *src_state;
	int error;

	if ((src_state = src_new(QUALITY, 1, &error)) == NULL)
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
	data.normalizer = VOLUME;
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


	char init = 1;
	int stream_started = 0;

	//unsigned long total_input_frames = cur_time / 10 + PULSE_LENGTH + 1;

	unsigned int delay = 0;

	src_data.data_in = input_data;

	while ((src_data.input_frames = fill_buffer(&init, input_data)))
	{
		if ((error = src_process(src_state, &src_data)))
		{
			fprintf(stderr, "error: src_process() failed: %s\n", src_strerror(error));
			exit(-1);
		}
		if (src_data.input_frames_used != src_data.input_frames)
		{
			fprintf(stderr, "error: not all input frames consumed! %ld used, %ld sent\n", src_data.input_frames_used, src_data.input_frames);
			exit(-1);
		}
		src_data.data_out += src_data.output_frames_gen;
		last_data += src_data.output_frames_gen;
		delay++;
		if (!stream_started && delay > AUDIO_DELAY)
		{
			stream_started = 1;
			err = Pa_StartStream( stream );
			if( err != paNoError ) goto error;
		}
	}
	done = 1;
	if (!stream_started)
	{
		stream_started = 1;
		err = Pa_StartStream( stream );
		if( err != paNoError ) goto error;
	}

/*
	while (total_input_frames > 0)
	{
		//fprintf(stderr, "filling buffer\n");
		//src_data.input_frames = fill_buffer(&init);
		//fprintf(stderr, "got %ld frames\n", src_data.input_frames);
		//if (done)
		//	src_data.end_of_input = 1;
		
		//if (src_data.input_frames < MAX_INPUT_SIZE)
		//{
		//	done = 1;
		//	src_data.end_of_input = 1;
		//}
		src_data.input_frames = (total_input_frames > BUF_WINDOW_SIZE) ? BUF_WINDOW_SIZE : total_input_frames;
		total_input_frames -= src_data.input_frames;

		if ((error = src_process(src_state, &src_data)))
		{
			fprintf(stderr, "error: src_process() failed: %s\n", src_strerror(error));
			exit(-1);
		}
		if (src_data.input_frames_used != src_data.input_frames)
		{
			fprintf(stderr, "error: not all input frames consumed! %ld used, %ld sent\n", src_data.input_frames_used, src_data.input_frames);
			exit(-1);
		}
		//fprintf(stderr, "got %ld frames, moving from %p ", src_data.output_frames_gen, src_data.data_out);

		src_data.data_in += src_data.input_frames;
		src_data.data_out += src_data.output_frames_gen;
		last_data += src_data.output_frames_gen;
		
		//fprintf(stderr, "to %p, %lld frames total\n", src_data.data_out, last_data);
		//fprintf(stderr, "%p, %lld frames total\n", src_data.data_out, last_data);
		delay++;
		if (!stream_started && delay > AUDIO_DELAY)
		{
			stream_started = 1;
			err = Pa_StartStream( stream );
			if( err != paNoError ) goto error;
		}
	}
	done = 1;
	if (!stream_started)
	{
		stream_started = 1;
		err = Pa_StartStream( stream );
		if( err != paNoError ) goto error;
	}*/
/*
	err = Pa_StartStream( stream );
	if( err != paNoError ) goto error;
*/	

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
