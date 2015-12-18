while (<>) {
	chomp;
	$line .= $_;
	# Some older openGL implementations don't like the \ at the end of an #if
	# So concatenate those lines here instead.
	if ($line =~ /\\$/) {
		chop $line;
		next;
	}
	$line =~ s/\\/\\\\/g;
	printf "\"$line\\n\"\n";
	$line = "";
}
