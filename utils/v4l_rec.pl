#!/usr/bin/perl
use strict;

# This is a very simple script to record a v4l program with ffmpeg or mencode
# Currenlty, works only with PAL-M or NTSC with ntsc-cable freqs
#
# mencode is easier due to usage of ALSA

my $station = shift or die "Usage: $0 <station> [standard] [device]";
my $dev;
my $std;

# Parameters with optional values

$std=shift or $std='PAL-M';
$dev=shift or $dev="/dev/video1";

##############################################
# Those stuff below are currently "hardcoded"

my $acard=0;
my $rec_ctrl="Aux,0";
my $file="out.mpg";
my $vbitrate=1500;
my $abitrate=224;

##############################################
# Those stuff below are NTSC / PAL-M specific

my $list="/usr/share/xawtv/ntsc-cable.list";
my $fps=30000/1001;
my $width=640;
my $height=480;
##############################################

my $on=0;
my $freq;

open IN,$list or die "$list not found";

while (<IN>) {
	if ($on) {
		if (m/freq\s*=\s*(\d+)(\d..)/) {
			$freq="$1.$2";
			$on=0;
		}
	};

	if (m/[\[]($station)[\]]/) {
		$on=1;
	}
}

close IN;

if ( !$freq ) {
	printf "Can't find station $station\n";
	exit;
}

printf "setting to channel $station, standard $std, freq=$freq on device $dev\n";
system "v4l2-ctl -d $dev -f $freq -s $std";

printf "Programming alsa to capture on $rec_ctrl at hw $acard\n";
system "amixer -c $acard sset $rec_ctrl 80% unmute cap";
system "amixer -c $acard sset Capture 15%";

printf "recording with ffmpeg on device $dev\n";

my $encode="/usr/bin/mencoder -tv driver=v4l2:device=$dev:norm=$std:width=$width:height=$height:input=0:alsa:adevice=hw.".$acard.":amode=1:forceaudio:fps=$fps tv:// -o $file -oac mp3lame -lameopts cbr:br=$abitrate -ovc lavc -lavcopts dia=-2:vcodec=mpeg4:vbitrate=$vbitrate -noodml";
#my $encode="ffmpeg -ad /dev/dsp".$acard." -vd $dev -tvstd $std -s ".$width."x".$height." -vcodec mpeg2video -f mpeg test.mpg";

print "$encode\n";
exec $encode;
