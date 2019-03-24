#!/usr/bin/perl

# Copyright (c) 2006-2010 by Mauro Carvalho Chehab <mchehab@kernel.org>
#
# Licensed under the terms of the GNU GPL License version 2

use strict;
use File::Find;

my %depend = ();
my %depend2 = ();
my %rmlist = ();
my @nodep;
my @modlist;
my @allmodules;
my %reqmodules;
my %loaded = ();
my $i=0;

sub findprog($)
{
	foreach(split(/:/, $ENV{PATH}),qw(/sbin /usr/sbin /usr/local/sbin)) {
		return "$_/$_[0]" if(-x "$_/$_[0]");
	}
	die "Can't find needed utility '$_[0]'";
}

sub parse_dir {
	my $file = $File::Find::name;
	my $modinfo = findprog('modinfo');

	if (!($file =~ /[.]ko$/)) {
		return;
	}

	my $module = $file;
	$module =~ s|.*\/([^/]+)\.ko|\1|;

	open IN, "$modinfo $module.ko|" or die "can't run $modinfo $file";
	while (<IN>) {
		if (m/depends:\s*(.*)/) {
			my $deps = $1;
			$deps =~ s/\n//;
			$deps =~ s/[,]/ /g;
			$deps = " $deps ";
			$depend{$module} = $deps;
			push @allmodules, $module;
			$i++;
		}
	}
	close IN;
}

sub parse_loaded {
	open IN,  "/proc/modules";
	while (<IN>) {
		m/^([\w\d_-]+)/;
		$loaded{$1}=1;
	}
	close IN;
}

sub cleandep()
{
	my $dep;

	while ( my ($k, $v) = each(%depend) ) {
		my $arg=$v;
		my $arg2=" ";
		while (!($arg =~ m/^\s*$/)) {
			if ($arg =~ m/^ ([^ ]+) /) {
				my $val=$1;
				if (exists($depend{$val})) {
					$arg2="$arg2 $val ";
				} else {
					$reqmodules{$val}=1;
				}
			}
			$arg =~ s/^ [^ ]+//;
			$arg2 =~ s/\s\s+/ /;
		}
		$depend2 { $k } = $arg2;
	}

}

sub rmdep()
{
	my $dep;

	while ($dep=pop @nodep) {
		while ( my ($k, $v) = each(%depend2) ) {
			if ($v =~ m/\s($dep)\s/) {
				$v =~ s/\s${dep}\s/ /;
				$v =~ s/\s${dep}\s/ /;
				$v =~ s/\s${dep}\s/ /;
				$depend2 {$k} = $v;
			}
		}
	}
}

sub orderdep ()
{
	my $old;
	do {
		$old=$i;
		while ( my ($key, $value) = each(%depend2) ) {
			if ($value =~ m/^\s*$/) {
				push @nodep, $key;
				push @modlist, $key;
				$i=$i-1;
				delete $depend2 {$key};
			}
		}
		rmdep();
	} until ($old==$i);
	while ( my ($key, $value) = each(%depend2) ) {
		printf "ERROR: bad dependencies - $key ($value)\n";
	}
}

sub rmmod(@)
{
	my $rmmod = findprog('rmmod');
	my @not;
	foreach (reverse @_) {
		s/-/_/g;
		if (exists ($loaded{$_})) {
			print "$rmmod $_\n";
			unshift @not, $_ if (system "$rmmod $_");
		}
	}
	return @not;
}

sub prepare_cmd()
{
	my $ver=qx(uname -r);
	$ver =~ s/\s+$//;
	die "Couldn't get kernel version" if (!$ver);

	print "Seeking media drivers at /lib/modules/$ver/kernel/drivers/media/\n";
	find(\&parse_dir, "/lib/modules/$ver/kernel/drivers/media/");
	print "Seeking media drivers at /lib/modules/$ver/kernel/drivers/staging/media/\n";
	find(\&parse_dir, "/lib/modules/$ver/kernel/drivers/staging/media/");
	printf "found $i modules\n";

	cleandep();
	orderdep();
}

prepare_cmd;
parse_loaded;

my @notunloaded = rmmod(@modlist);
@notunloaded = rmmod(@notunloaded) if (@notunloaded);
if (@notunloaded) {
	print "Couldn't unload: ", join(' ', @notunloaded), "\n";
}
