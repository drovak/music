# music
Computer music programs, mostly involving the PDP-8

wav_creator directory contains time2wav, a program which generates music from a PDP-8. It requires a simple patch to SimH to generate the total elapsed cycle count every time a CAF instruction is executed. The program takes the values (treated as elapsed time) and generates a WAV file with pulses at each value. The WAV file is low-passed as it would be if an AM radio were playing the music. This is very much a beta program and will likely crash given certain input files. The only dependency is libsamplerate used to sample down the series of pulses. A sample can also be found on YouTube: https://www.youtube.com/watch?v=_urDcyluX9c

wav_creator_nt is another time2wav program which is different in a couple of respects: it uses delta values between CAF instructions, and these values are now in units of tenths of microseconds. The elapsed instruction count method proved to be inaccurate, so SimH was patched with a few lines of code that accurately counts microseconds based on what instruction is executed. Results have improved tremendously. A new sample has also been uploaded to YouTube: https://www.youtube.com/watch?v=ZGeycH7qUTU

wav_creator_smallbuf reuses the input and output buffers to play music, but is otherwise similar to wav_creator_nt.

compilemu.sh is a script to call up an instance of the PDP-8 simulator, attach a music file to be compiled, saves the output data, and calls time2wav to play it. 

All of this is rendered somewhat obsolete, as simh_standalone_player contains the appropriate files to be added to the SimH PDP-8 simulator to allow it to play music as a standalone application. It is fairly CPU-intensive and may overrun the audio buffer if your computer is heavily loaded. To compile, replace the makefile with the one provided, replace pdp8_cpu.c with the new one, and add in the pdp8_music.h and .c files. Running `make pdp8` should compile it, assuming you've already made the appropriate changes and recompiled libsamplerate; src_sinc.c is supplied which contains modifications to speed up the downsampling process given the confined data the PDP-8 sends out.

There is still some benefit to time2wav and the other modified SimH instance, as they can directly generate the data files (and WAV files) for instant playback, though this support could ultimately be added to SimH with some effort.
