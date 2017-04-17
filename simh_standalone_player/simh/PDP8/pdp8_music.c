/*
  PDP-8 Music Generation Add-On
  Kyle Owen - 16 April 2017
  To-Do:
    Add halt reason (buffer overrun) instead of using STOP_HALT
      (should the simulator halt on buffer overrun or silently try to continue?)
	Unify error messages and exit() calls
    Pick best buffer size (trade-off of delay vs. chance of overrun at high CPU load)
	Enable/disable music player at sim> prompt
	Make downsample library even more efficient (SSE2 intrinsics, perhaps?)
	Generalize at the simulator level to allow for a variety of interfaces to a variety
	  of computers
	Tidy up makefile with recent additions
*/


#include "pdp8_music.h"

PaStreamParameters outputParameters;
PaStream *stream;
PaError err;
paUserData data;

void PaErrorFunc(PaError err)
{
    Pa_Terminate();
    sim_printf("An error occured while using the portaudio stream\n" );
    sim_printf("Error number: %d\n", err );
    sim_printf("Error message: %s\n", Pa_GetErrorText( err ) );
    exit(err);
}

static void StreamFinished( void* userData )
{
   paUserData *data = (paUserData *) userData;
   //sim_printf( "Stream Completed: %s\n", data->message );
}

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

#ifdef DEBUG
	sim_printf("getting audio data, %d frames in 0 and %d frames in 1, current buffer is %d\n",
		output_data_size[0], output_data_size[1], current_audio_buf);
#endif

	if (output_data_size[current_audio_buf] == 0)
	{
		sim_printf("buffer overrun!\n");
		stream_error = -1;
		return paComplete;
	}

    for( i=0; i<framesPerBuffer; i++ )
    {
		if (output_data_size[current_audio_buf])
		{
			*out = output_data[current_audio_buf][data->playback_ptr++] * VOLUME;
			if (*out > 1.0f)
			{
				sim_printf("clipping detected!\n");
			}
			out++;
			output_data_size[current_audio_buf]--;
		}
		else
		{
#ifdef DEBUG
			sim_printf("swapping audio buffers\n");
#endif
			data->playback_ptr = 0;
			current_audio_buf++;
			current_audio_buf &= (NUMBER_OF_BUFFERS - 1);
			if (output_data_size[current_audio_buf] == 0)
			{
				sim_printf("buffer overrun!\n");
				stream_error = -1;
				return paComplete;
			}
		}
    }
    
	if (output_data_size[current_audio_buf] == 0)
	{
#ifdef DEBUG
		sim_printf("no data at current audio buffer, swapping buffers\n");
#endif
		data->playback_ptr = 0;
		current_audio_buf++;
		current_audio_buf &= (NUMBER_OF_BUFFERS - 1);
	}

    return paContinue; 
}


void do_init_music()
{
#ifdef DEBUG
	sim_printf("initializing music player\n");
#endif

	elapsed_time = 0;
	pulse_ptr = 0;
	pulses_left = 0;
	current_ds_buf = 0;
	current_audio_buf = 0;

	stream_started = 0;
	is_playing = 0;
	stream_error = 0;

	int error;

	if ((src_state = src_new(QUALITY, 1, &error)) == NULL)
	{	
		sim_printf("error: src_new() failed: %s\n", src_strerror(error));
		exit(-1);
	}

	src_data.end_of_input = 0; /* Set this later. */

	/* Start with zero to force load in while loop. */
	src_data.input_frames = 0;

	src_data.data_in = input_data;
	src_data.output_frames = OUTPUT_LENGTH;
	src_data.src_ratio = 0.0441;


	memset(output_data[0], 0, OUTPUT_LENGTH * sizeof(float));
	memset(output_data[1], 0, OUTPUT_LENGTH * sizeof(float));


    err = Pa_Initialize();
    if( err != paNoError ) PaErrorFunc(err);

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
      sim_printf("Error: No default output device.\n");
      PaErrorFunc(err);
    }
    outputParameters.channelCount = 1; 
    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

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
    if( err != paNoError ) PaErrorFunc(err);

    sprintf( data.message, "No Message" );
    err = Pa_SetStreamFinishedCallback( stream, &StreamFinished );
    if( err != paNoError ) PaErrorFunc(err);
}


void do_music()
{
#ifdef DEBUG
	sim_printf("putting pulse at %d\n", pulse_ptr);
#endif
	pulses_left = PULSE_LENGTH;
	for (int i = pulse_ptr; i < INPUT_LENGTH && pulses_left; i++)
	{
		input_data[i] = 1.0f;
		pulses_left--;
	}
}

int calc_inst_time(int IR, int PC)
{
	switch (IR >> 9) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			if ((IR & 0600) == 0200 || (IR & 0600) == 0) {
				return 26;
			} else if ((IR & 0600) == 0400) {
				if ((IR & 0170) == 010)
					return 40;
				else
					return 38;
			} else if ((IR & 0600) == 0600) {
				if ((IR & 0170) == 010)
					return (PC & 07600) ? 38 : 40;
				else
					return 38;
			}
		case 5:
			if ((IR & 0600) == 0200 || (IR & 0600) == 0) {
				return 12;
			} else if ((IR & 0600) == 0400) {
				if ((IR & 0170) == 010)
					return 26;
				else
					return 24;
			} else if ((IR & 0600) == 0600) {
				if ((IR & 0170) == 010)
					return (PC & 07600) ? 24 : 26;
				else
					return 24;
			}
		case 6:
		case 7:
		default:
			return 12;
	}
}


void make_buffer()
{
	elapsed_time -= (10 * INPUT_LENGTH);
	if (elapsed_time >= (10 * INPUT_LENGTH))
	{
		sim_printf("error: elapsed time overflowed!\n");
		elapsed_time = 0;
	}
	src_data.input_frames = INPUT_LENGTH;
	src_data.data_in = input_data;

	int waiting = 0;
	while (output_data_size[current_ds_buf] != 0)
	{
		if (!waiting)
		{
			waiting = 1;
#ifdef DEBUG
			sim_printf("waiting for buffer %d to empty\n", current_ds_buf);
#endif
		}
	}
#ifdef DEBUG
	sim_printf("done waiting; buffer %d must be empty\n", current_ds_buf);
#endif

	src_data.data_out = output_data[current_ds_buf];
	int err;
	if ((err = src_process(src_state, &src_data)))
	{
		sim_printf("error: src_process() failed: %s\n", src_strerror(err));
		exit(-1);
	}
	if (src_data.input_frames_used != src_data.input_frames)
	{
		sim_printf("error: not all input frames consumed! %ld used, %ld sent\n", src_data.input_frames_used, src_data.input_frames);
		exit(-1);
	}
	//src_data.output_frames_gen = (current_ds_buf ? 342 : 361);
	output_data_size[current_ds_buf] = src_data.output_frames_gen;
#ifdef DEBUG
	sim_printf("generated %ld frames in buffer %d\n", src_data.output_frames_gen, current_ds_buf);
#endif
	current_ds_buf++;
	current_ds_buf &= (NUMBER_OF_BUFFERS - 1);
	memset(input_data, 0, INPUT_LENGTH * sizeof(float));
	for (int i = 0; i < pulses_left; i++)
		input_data[i] = 1.0f;
	pulses_left = 0;
	
	if (!stream_started && current_ds_buf == 0)
	{
#ifdef DEBUG
		sim_printf("starting stream with %d samples in 0 and %d in 1\n", output_data_size[0], output_data_size[1]);
#endif
		stream_started = 1;
		is_playing = 1;
		err = Pa_StartStream( stream );
		if( err != paNoError ) PaErrorFunc(err);
	}
}

void pause_stream()
{
	if (stream_started && is_playing)
	{
		is_playing = 0;
		err = Pa_StopStream( stream );
		if( err != paNoError ) PaErrorFunc(err);
	}
}

void play_stream()
{
	if (stream_started && !is_playing)
	{
		is_playing = 1;
		err = Pa_StartStream( stream );
		if( err != paNoError ) PaErrorFunc(err);
	}
}
