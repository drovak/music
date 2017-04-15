#!/bin/bash
# Automates creating a music file with SimH (cool, right?!)

infile=$1
outfile=${infile%.*}.txt
pdp8exe=~/src/simh_mod/BIN/pdp8
time2wav=../wav_creator_buf/time2wav

$pdp8exe pdp8.ini $1 > $outfile
sed -i '' -e '1,11d' $outfile
sed -i '' -e '$ d' $outfile
tr -d '\r' < $outfile > temp.txt
tr -dc '0-9\n' < temp.txt > $outfile
rm temp.txt
$time2wav $outfile
