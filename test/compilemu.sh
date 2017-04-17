#!/bin/bash
# Automates creating a music file with SimH (cool, right?!)

infile=$1
outfile=${infile%.*}.txt
errfile=error.txt
pdp8exe=pdp8_music
time2wav=../wav_creator_buf/time2wav

if test $infile -nt $outfile ; then
	$pdp8exe pdp8.ini $1 > $outfile
	sed -i '' -e '1,6d' $outfile
	head $outfile > $errfile
	sed -i '' -e '1,5d' $outfile
	sed -i '' -e '$ d' $outfile
	tr -d '\r' < $outfile > temp.txt
	tr -dc '0-9\n' < temp.txt > $outfile
	rm temp.txt
	cat $errfile
fi
$time2wav $outfile
