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
#include <samplerate.h>

#define MAX_INPUT_SIZE 160000000
#define MAX_OUTPUT_SIZE 16000000

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

SRC_DATA src_data;

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		fprintf(stderr, "usage: %s [input] [output]\n", argv[0]);
		exit(0);
	}
	fprintf(stderr, "opening %s for reading\n", argv[1]);
	if ((infile = fopen(argv[1], "r")) == NULL)
	{
		fprintf(stderr, "couldn't open %s for reading!\n", argv[1]);
		exit(-1);
	}
	fprintf(stderr, "opening %s for writing\n", argv[2]);
	if ((outfile = fopen(argv[2], "w")) == NULL)
	{
		fprintf(stderr, "couldn't open %s for writing!\n", argv[2]);
		exit(-1);
	}

	int samples = 64000;

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

	fprintf(stderr, "calling malloc() for input data\n");
	input_data = (float *) malloc(MAX_INPUT_SIZE * sizeof(float));
	if (input_data == NULL)
	{
		fprintf(stderr, "Unable to allocate input array!\n");
		exit(-1);
	}

	fprintf(stderr, "calling malloc() for output data\n");
	output_data = (float *) malloc(MAX_OUTPUT_SIZE * sizeof(float));
	if (output_data == NULL)
	{
		fprintf(stderr, "Unable to allocate output array!\n");
		exit(-1);
	}

	fprintf(stderr, "calling memset() for input data\n");
	memset(input_data, 0, MAX_INPUT_SIZE * sizeof(float));

	unsigned long start;
	unsigned long val;

	fprintf(stderr, "reading input file\n");
	fscanf(infile, "%ld", &start);
	while (!feof(infile))	//probably want some bounds checking in the loop...
	{
		fscanf(infile, "%ld", &val);
		input_data[val-start] = 1;
		input_data[val-start+1] = 1;	//expand pulse in time; manual says 6 microseconds
		input_data[val-start+2] = 1; 
	}
		

	src_data.data_in = input_data;
	src_data.data_out = output_data;
	src_data.input_frames = val-start+3;
	src_data.output_frames = MAX_OUTPUT_SIZE;
	src_data.src_ratio = 0.084807692;	//convert sample rate from 520000 (average of 1.93 microseconds per instruction) to 44100

	fprintf(stderr, "performing sample rate conversion\n");
	int ret = src_simple(&src_data, SRC_SINC_MEDIUM_QUALITY, 1);
	fprintf(stderr, "sample rate conversion completed\n");

	if (ret)
	{
		fprintf(stderr, "src_simple returned %d!\n", ret);
		exit(-1);
	}

	fprintf(stderr, "number of output frames: %ld\n", src_data.output_frames_gen);

	float min = 0;
	float max = 0;

	fprintf(stderr, "normalizing output data\n");
	for (unsigned long i = 0; i < src_data.output_frames_gen; i++)
	{
		if (output_data[i] < min)
			min = output_data[i];
		if (output_data[i] > max)
			max = output_data[i];
	}
	fprintf(stderr, "min: %f\tmax: %f\n", min, max);

	float normalizer = (abs(min) > abs(max)) ? abs(min) : abs(max);

	WAV_HEADER.chunk_size = sizeof(WAV_HEADER) - 8 + (src_data.output_frames_gen * 2);
	WAV_HEADER.data_subchunk_size = src_data.output_frames_gen * 2;
	
	fprintf(stderr, "writing WAV header\n");
	fwrite(&WAV_HEADER, 1, sizeof(WAV_HEADER), outfile);
	
	fprintf(stderr, "writing output data\n");
	for (unsigned long i = 0; i < src_data.output_frames_gen; i++)
	{
		short data = (output_data[i] / normalizer) * 32767;		//rounding could be improved here; probably not too important
		fwrite(&data, 1, sizeof(data), outfile);
	}
	fprintf(stderr, "done\n");
	//maybe add silence before and after main data?

	free(input_data);
	free(output_data);
	fclose(infile);
	fclose(outfile);
	return 0;
}
