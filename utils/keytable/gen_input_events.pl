#!/usr/bin/perl
use strict;

my @ev;
my @msc;
my @key;
my @key;
my @rel;
my @abs;

my %key_hash;

while (<>) {
	if (m/^\#define\s+(EV_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) {
		next if ($1 eq "EV_VERSION");
		my @e = ($1, $2);
		push(@ev, \@e);
	}

	if (m/^\#define\s+(MSC_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) {
		my @e = ($1, $2);
		push(@msc, \@e);
	}

	if (m/^\#define\s+(KEY_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) {
		next if ($1 eq "KEY_MAX");
		$key_hash{$1} = $2;
		my @e = ($1, $2);
		push(@key, \@e);
	}

	if (m/^\#define\s+(KEY_[^\s]+)\s+(KEY_[^\s]+)/) {
		next if ($1 eq "KEY_MIN_INTERESTING");
		my @e = ($1, $key_hash{$2});
		push(@key, \@e);
	}

	if (m/^\#define\s+(BTN_[^\s]+)\s+(0x[\d\w]+|[\d]+)/)  {
		my @e = ($1, $2);
		push(@key, \@e);
	}

	if (m/^\#define\s+(REL_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) {
		my @e = ($1, $2);
		push(@rel, \@e);
	}

	if (m/^\#define\s+(ABS_[^\s]+)\s+(0x[\d\w]+|[\d]+)/) {
		my @e = ($1, $2);
		push(@abs, \@e);
	}
}

print "struct parse_event {\n\tchar *name;\n\tunsigned int value;\n};\n";
print "struct parse_event events_type[] = {\n";
for my $e (@ev) {
	my $name = @$e[0];
	my $val = @$e[1];
	print "\t{\"$name\", $val},\n";
}
print "\t{ NULL, 0}\n};\n";

print "struct parse_event msc_events[] = {\n";
for my $e (@msc) {
	my $name = @$e[0];
	my $val = @$e[1];
	print "\t{\"$name\", $val},\n";
}
print "\t{ NULL, 0}\n};\n";

print "struct parse_event key_events[] = {\n";
for my $e (@key) {
	my $name = @$e[0];
	my $val = @$e[1];
	print "\t{\"$name\", $val},\n";
}
print "\t{ NULL, 0}\n};\n";

print "struct parse_event rel_events[] = {\n";
for my $e (@rel) {
	my $name = @$e[0];
	my $val = @$e[1];
	print "\t{\"$name\", $val},\n";
}
print "\t{ NULL, 0}\n};\n";

print "struct parse_event abs_events[] = {\n";
for my $e (@abs) {
	my $name = @$e[0];
	my $val = @$e[1];
	print "\t{\"$name\", $val},\n";
}
print "\t{ NULL, 0}\n};\n";

