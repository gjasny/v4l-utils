#!/usr/bin/perl

sub maxprefix {
	my $p = shift(@_);
	for (@_) {
		chop $p until /^\Q$p/;
	}
	$p =~ s/_[^_]*$/_/;
	$p = "CEC_OP_CEC_" if ($p =~ /CEC_OP_CEC_VERSION_/);
	return $p;
}

sub process_func
{
	my $feature = shift;
	my $func = shift;
	my $func_args = $func;
	$func =~ s/\(.*//;
	my $msg = $func;
	$msg =~ s/([a-z])/\U\1/g;
	$func =~ s/cec_msg//;
	my $opt = $func;
	$opt =~ s/_([a-z])/\U\1/g;
	$func_args =~ s/.*\((.*)\).*/\1/;
	my $has_reply = $func_args =~ /^int reply/;
	$func_args =~ s/^int reply,? ?//;
	my $arg_names;
	my $arg_ptrs;
	my $name, $type, $size;
	my $msg_dash_name, $msg_lc_name;
	my @enum, $val;
	my $usage;
	my $has_digital = $func_args =~ /cec_op_digital_service_id/;
	my $has_ui_command = $func_args =~ /cec_op_ui_command/;
	my $has_short_aud_descr = $func_args =~ /num_descriptors/;

	my @ops_args = split(/, */, $func_args);
	if ($has_digital) {
		$func_args =~ s/const struct cec_op_digital_service_id \*digital/__u8 service_id_method, __u8 dig_bcast_system, __u16 transport_id, __u16 service_id, __u16 orig_network_id, __u16 program_number, __u8 channel_number_fmt, __u16 major, __u16 minor/;
	}
	if ($has_ui_command) {
		$func_args =~ s/const struct cec_op_ui_command \*ui_cmd/__u8 ui_cmd, __u8 has_opt_arg, __u8 play_mode, __u8 ui_function_media, __u8 ui_function_select_av_input, __u8 ui_function_select_audio_input, __u8 ui_bcast_type, __u8 ui_snd_pres_ctl, __u8 channel_number_fmt, __u16 major, __u16 minor/;
	}
	if ($has_short_aud_descr) {
		$func_args =~ s/const __u32 \*descriptors/__u8 descriptor1, __u8 descriptor2, __u8 descriptor3, __u8 descriptor4/;
		$func_args =~ s/const __u8 \*audio_format_id, const __u8 \*audio_format_code/__u8 audio_format_id1, __u8 audio_format_code1, __u8 audio_format_id2, __u8 audio_format_code2, __u8 audio_format_id3, __u8 audio_format_code3, __u8 audio_format_id4, __u8 audio_format_code4/;
	}
	my @args = split(/, */, $func_args);
	my $has_struct = $func_args =~ /struct/;
	return if ($func_args =~ /__u\d+\s*\*/);

	my $cec_msg = $msg;
	while ($cec_msg ne "" && !exists($msgs{$cec_msg})) {
		$cec_msg =~ s/_[^_]*$//;
	}
	return if ($cec_msg eq "");

	my $msg_name = $cec_msg;
	$msg_name =~ s/CEC_MSG_//;
	$msg_dash_name = $msg;
	$msg_dash_name =~ s/CEC_MSG_//;
	$msg_dash_name =~ s/([A-Z])/\l\1/g;
	$msg_dash_name =~ s/_/-/g;
	$msg_lc_name = $msg;
	$msg_lc_name =~ s/([A-Z])/\l\1/g;

	if ($cec_msg eq $msg) {
		if ($cec_msg =~ /_CDC_/ && !$cdc_case) {
			$cdc_case = 1;
			$logswitch .= "\tcase CEC_MSG_CDC_MESSAGE:\n";
			$logswitch .= "\tswitch (msg->msg[4]) {\n";
		}
		if ($cdc_case) {
			$cdcmsgtable .= "\t{ $cec_msg, \"$msg_name\" },\n";
		} else {
			$msgtable .= "\t{ $cec_msg, \"$msg_name\" },\n";
		}
		if (@args == 0) {
			$logswitch .= "\tcase $cec_msg:\n";
			$logswitch .= "\t\tprintf(\"$cec_msg (0x%02x)\\n\", $cec_msg);\n";
			$logswitch .= "\t\tbreak;\n\n";
		} else {
			$logswitch .= "\tcase $cec_msg: {\n";
			foreach (@ops_args) {
				($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
				if ($type =~ /struct .*\*/) {
					$type =~ s/ \*//;
					$type =~ s/const //;
				}
				if ($name eq "rc_profile" || $name eq "dev_features") {
					$logswitch .= "\t\tconst __u8 *$name = NULL;\n";
				} elsif ($type eq "const char *") {
					$logswitch .= "\t\tchar $name\[16\];\n";
				} elsif ($type eq "const __u32 *") {
					$logswitch .= "\t\t__u32 $name\[4\];\n";
				} elsif ($type eq "const __u8 *") {
					$logswitch .= "\t\t__u8 $name\[4\];\n";
				} elsif ($type =~ /struct/) {
					$logswitch .= "\t\t$type $name = {};\n";
				} else {
					$logswitch .= "\t\t$type $name;\n";
				}
			}
			if ($cdc_case) {
				$logswitch .= "\t\t__u16 phys_addr;\n";
			}
			my $ops_lc_name = $msg_lc_name;
			$ops_lc_name =~ s/^cec_msg/cec_ops/;
			$logswitch .= "\n\t\t$ops_lc_name(msg";
			if ($cdc_case) {
				$logswitch .= ", &phys_addr";
			}
			foreach (@ops_args) {
				($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
				if ($type eq "const char *" ||
				    $type eq "const __u8 *" ||
				    $type eq "const __u32 *") {
					$logswitch .= ", $name";
				} else {
					$logswitch .= ", &$name";
				}
			}
			$logswitch .= ");\n";
			$logswitch .= "\t\tprintf(\"$cec_msg (0x%02x):\\n\", $cec_msg);\n";
			if ($cdc_case) {
				$logswitch .= "\t\tlog_arg(&arg_phys_addr, \"phys-addr\", phys_addr);\n";
			}
			foreach (@ops_args) {
				($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
				my $dash_name = $name;
				$dash_name =~ s/_/-/g;
				if ($name eq "rc_profile" || $name eq "dev_features") {
					$logswitch .= "\t\tlog_features(&arg_$name, \"$dash_name\", $name);\n";
				} elsif ($name eq "digital") {
					$logswitch .= "\t\tlog_digital(\"$dash_name\", &$name);\n";
				} elsif ($name eq "ui_cmd") {
					$logswitch .= "\t\tlog_ui_command(\"$dash_name\", &$name);\n";
				} elsif ($name eq "rec_src") {
					$logswitch .= "\t\tlog_rec_src(\"$dash_name\", &$name);\n";
				} elsif ($name eq "tuner_dev_info") {
					$logswitch .= "\t\tlog_tuner_dev_info(\"$dash_name\", &$name);\n";
				} elsif ($name eq "descriptors") {
					$logswitch .= "\t\tlog_descriptors(\"$dash_name\", num_descriptors, $name);\n";
				} elsif ($name eq "audio_format_id" || $name eq "audio_format_code") {
					$logswitch .= "\t\tlog_u8_array(\"$dash_name\", num_descriptors, $name);\n";
				} else {
					$logswitch .= "\t\tlog_arg(&arg_$name, \"$dash_name\", $name);\n";
				}
			}
			$logswitch .= "\t\tbreak;\n\t}\n";
		}
	}
	return if $has_struct;

	$options .= "\tOpt$opt,\n";
	$messages .= "\t\t$cec_msg,\n";
	$messages .= "\t\tOpt$opt,\n" unless $is_log;
	if (@args == 0) {
		$messages .= "\t\t0, { }, { },\n";
		$long_opts .= "\t{ \"$msg_dash_name\", no_argument, 0, Opt$opt }, \\\n";
		$usage .= "\t\"  --" . sprintf("%-30s", $msg_dash_name) . "Send $msg_name message (\" xstr($cec_msg) \")\\n\"\n";
		$usage_msg{$msg} = $usage;
		$switch .= "\tcase Opt$opt: {\n";
		$switch .= "\t\t$msg_lc_name(&msg";
		$switch .= ", reply" if $has_reply;
		$switch .= ");\n\t\tbreak;\n\t}\n\n";
	} else {
		$long_opts .= "\t{ \"$msg_dash_name\", required_argument, 0, Opt$opt }, \\\n";
		$usage .= "\t\"  --$msg_dash_name";
		my $prefix = "\t\"    " . sprintf("%-30s", " ");
		my $sep = "=";
		foreach (@args) {
			($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
			$name =~ s/_/-/g;
			$usage .= "$sep$name=<val>";
			$sep = ",";
		}
		$usage .= "\\n\"\n";
		foreach (@args) {
			($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
			@enum = @{$types{$name}};
			next if !scalar(@enum);
			$name =~ s/_/-/g;
			$usage .= $prefix . "'$name' can have these values:\\n\"\n";
			my $common_prefix = maxprefix(@enum);
			foreach (@enum) {
				my $e = $_;
				s/^$common_prefix//;
				s/([A-Z])/\l\1/g;
				s/_/-/g;
				$usage .= $prefix . "    $_ (\" xstr($e) \")\\n\"\n";
			}
		}
		$usage .= $prefix . "Send $msg_name message (\" xstr($cec_msg) \")\\n\"\n";
		$usage_msg{$msg} = $usage;
		$switch .= "\tcase Opt$opt: {\n";
		foreach (@args) {
			($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
			if ($type =~ /char/) {
				$switch .= "\t\tconst char *$name = \"\";\n";
			} else {
				$switch .= "\t\t$type $name = 0;\n";
			}
		}
		$switch .= "\n\t\twhile (*subs != '\\0') {\n";
		$switch .= "\t\t\tswitch (parse_subopt(&subs, opt->arg_names, &value)) {\n";
		my $cnt = 0;
		foreach (@args) {
			($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
			@enum = @{$types{$name}};
			$switch .= "\t\t\tcase $cnt:\n";
			if ($type =~ /char/) {
				$switch .= "\t\t\t\t$name = value;\n";
			} elsif (scalar(@enum) || $name eq "ui_cmd") {
				$switch .= "\t\t\t\t$name = parse_enum(value, opt->args\[$cnt\]);\n";
			} elsif ($name =~ /phys_addr/) {
				$switch .= "\t\t\t\t$name = parse_phys_addr(value);\n";
			} else {
				$switch .= "\t\t\t\t$name = strtol(value, 0L, 0);\n";
			}
			$switch .= "\t\t\t\tbreak;\n";
			$cnt++;
		}
		$switch .= "\t\t\tdefault:\n";
		$switch .= "\t\t\t\texit(1);\n";
		$switch .= "\t\t\t}\n\t\t}\n";
		$switch .= "\t\t$msg_lc_name(&msg";
		$switch .= ", reply" if $has_reply;
		foreach (@args) {
			($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
			$switch .= ", $name";
		}
		$switch .= ");\n\t\tbreak;\n\t}\n\n";

		foreach (@args) {
			($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
			if ($arg_names ne "") {
				$arg_names .= ", ";
				$arg_ptrs .= ", ";
			}
			if ($name eq "ui_cmd") {
				$arg_ptrs .= "&arg_rc_$name";
			} else {
				$arg_ptrs .= "&arg_$name";
			}
			$name =~ s/_/-/g;
			$arg_names .= '"' . $name . '"';
		}
		$size = $#args + 1;
		$messages .= "\t\t$size, { $arg_names },\n";
		$messages .= "\t\t{ $arg_ptrs },\n";
		foreach (@args) {
			($type, $name) = /(.*?) ?([a-zA-Z_]\w+)$/;
			@enum = @{$types{$name}};
			$size = scalar(@enum);

			if ($size && !defined($created_enum{$name})) {
				$created_enum{$name} = 1;
				$enums .= "static const struct cec_enum_values type_$name\[\] = {\n";
				my $common_prefix = maxprefix(@enum);
				foreach (@enum) {
					$val = $_;
					s/^$common_prefix//;
					s/([A-Z])/\l\1/g;
					s/_/-/g;
					$enums .= "\t{ \"$_\", $val },\n";
				}
				$enums .= "};\n\n";
			}
			if (!defined($created_arg{$name})) {
				$created_arg{$name} = 1;
				if ($type eq "__u8" && $size) {
					$arg_structs .= "static const struct arg arg_$name = {\n";
					$arg_structs .= "\tCEC_TYPE_ENUM, $size, type_$name\n};\n\n";
				} elsif ($type eq "__u8") {
					$arg_structs .= "#define arg_$name arg_u8\n";
				} elsif ($type eq "__u16") {
					$arg_structs .= "#define arg_$name arg_u16\n";
				} elsif ($type eq "__u32") {
					$arg_structs .= "#define arg_$name arg_u32\n";
				} elsif ($type eq "const char *") {
					$arg_structs .= "#define arg_$name arg_string\n";
				}
			}
		}
	}
	$messages .= "\t\t\"$msg_name\"\n";
	$messages .= "\t}, {\n";
	$feature_usage{$feature} .= $usage;
}

$is_log = shift;

while (<>) {
	last if /\/\* Messages \*\//;
}

$comment = 0;
$has_also = 0;
$operand_name = "";
$feature = "";

while (<>) {
	chomp;
	last if /_CEC_UAPI_FUNCS_H/;
	if (/^\/\*.*Feature \*\/$/) {
		($feature) = /^\/\* (.*) Feature/;
		$feature_usage{$feature} = "";
	}
	elsif (/^\/\*.*General Protocol Messages \*\/$/) {
		$feature = "Abort";
		$feature_usage{$feature} = "";
	}
	if ($operand_name ne "" && !/^#define/) {
		@{$types{$operand_name}} = @ops;
		undef @ops;
		$operand_name = "";
	}
	if (/\/\*.*Operand \((.*)\)/) {
		$operand_name = $1;
		next;
	}
	s/\/\*.*\*\///;
	if ($comment) {
		if ($has_also) {
			if (/CEC_MSG/) {
				($also_msg) = /(CEC_MSG\S+)/;
				push @{$feature_also{$feature}}, $also_msg;
			}
		} elsif (/^ \* Has also:$/) {
			$has_also = 1;
		}
		$has_also = 0 if (/\*\//);
		next unless /\*\//;
		$comment = 0;
		s/^.*\*\///;
	}
	if (/\/\*/) {
		$comment = 1;
		$has_also = 0;
		next;
	}
	next if /^\s*$/;
	if (/^\#define/) {
		($name, $val) = /define (\S+)\s+(\S+)/;
		if ($name =~ /^CEC_MSG/) {
			$msgs{$name} = 1;
		} elsif ($operand_name ne "" && $name =~ /^CEC_OP/) {
			push @ops, $name;
		}
		next;
	}
}

while (<>) {
	chomp;
	if (/^\/\*.*Feature \*\/$/) {
		($feature) = /^\/\* (.*) Feature/;
	}
	elsif (/^\/\*.*General Protocol Messages \*\/$/) {
		$feature = "Abort";
	}
	s/\/\*.*\*\///;
	if ($comment) {
		next unless /\*\//;
		$comment = 0;
		s/^.*\*\///;
	}
	if (/\/\*/) {
		$comment = 1;
		next;
	}
	next if /^\s*$/;
	next if /cec_msg_reply_feature_abort/;
	if (/^static inline void cec_msg.*\(.*\)/) {
		s/static\sinline\svoid\s//;
		s/struct cec_msg \*msg, //;
		s/struct cec_msg \*msg//;
		process_func($feature, $_);
		next;
	}
	if (/^static inline void cec_msg/) {
		$func = $_;
		next;
	}
	if ($func ne "") {
		$func .= $_;
		next unless /\)$/;
		$func =~ s/\s+/ /g;
		$func =~ s/static\sinline\svoid\s//;
		$func =~ s/struct cec_msg \*msg, //;
		$func =~ s/struct cec_msg \*msg//;
		process_func($feature, $func);
		$func = "";
	}
}

$options .= "\tOptHelpAll,\n";

if ($is_log == 2) {
	printf "struct msgtable {\n";
	printf "\t__u8 opcode;\n";
	printf "\tconst char *name;\n";
	printf "};\n\n";
	printf "static const struct msgtable msgtable[] = {\n";
	printf "%s", $msgtable;
	printf "\t{ CEC_MSG_VENDOR_COMMAND, \"VENDOR_COMMAND\" },\n";
	printf "\t{ CEC_MSG_VENDOR_COMMAND_WITH_ID, \"VENDOR_COMMAND_WITH_ID\" },\n";
	printf "\t{ CEC_MSG_VENDOR_REMOTE_BUTTON_DOWN, \"VENDOR_REMOTE_BUTTON_DOWN\" },\n";
	printf "\t{ CEC_MSG_CDC_MESSAGE, \"CDC_MESSAGE\" },\n";
	printf "};\n\n";
	printf "static const struct msgtable cdcmsgtable[] = {\n";
	printf "%s", $cdcmsgtable;
	printf "};\n";
	exit 0;
}

if ($is_log == 0) {
	foreach (sort keys %feature_usage) {
		$name = $_;
		s/ /_/g;
		s/([A-Z])/\l\1/g;
		$usage_var = $_ . "_usage";
		printf "static const char *$usage_var =\n";
		$usage = $feature_usage{$name};
		foreach (@{$feature_also{$name}}) {
			$usage .= $usage_msg{$_};
		}
		chop $usage;
		$usage =~ s/"  --vendor-remote-button-up/VENDOR_EXTRA\n\t"  --vendor-remote-button-up/;
		printf "%s;\n\n", $usage;
		s/_/-/g;
		$help_features .= sprintf("\t\"  --help-%-28s Show help for the $name feature\\n\" \\\n", $_);
		$opt = "OptHelp" . $name;
		$opt =~ s/ //g;
		$help .= "\tif (options[OptHelpAll] || options\[$opt\]) {\n";
		$help .= "\t\tprintf(\"$name Feature:\\n\");\n";
		$help .= "\t\tprintf(\"\%s\\n\", $usage_var);\n\t}\n";
		$options .= "\t$opt,\n";
		$long_opts .= "\t{ \"help-$_\", no_argument, 0, $opt }, \\\n";
	}

	print "enum {\n\tOptMessages = 255,\n";
	printf "%s\n\tOptLast = 512\n};\n\n", $options;

	printf "#define CEC_LONG_OPTS \\\n%s\n\n", $long_opts;
	printf "#define CEC_USAGE \\\n%s\n\n", $help_features;
}

printf "%s%s\n", $enums, $arg_structs;
printf "static const struct message messages[] = {\n\t{\n";
printf "%s\t}\n};\n\n", $messages;

if ($is_log == 0) {
	printf "static void usage_options(int ch)\n{\n";
	printf "%s}\n\n", $help;
	printf "static void parse_msg_args(struct cec_msg &msg, int reply, const message *opt, int ch)\n{\n";
	printf "\tchar *value, *subs = optarg;\n\n";
	printf "\tswitch (ch) {\n";
	$switch =~ s/(service_id_method, dig_bcast_system, transport_id, service_id, orig_network_id, program_number, channel_number_fmt, major, minor)/args2digital_service_id(\1)/g;
	$switch =~ s/(ui_cmd, has_opt_arg, play_mode, ui_function_media, ui_function_select_av_input, ui_function_select_audio_input, ui_bcast_type, ui_snd_pres_ctl, channel_number_fmt, major, minor)/args2ui_command(\1)/g;
	$switch =~ s/(descriptor1, descriptor2, descriptor3, descriptor4)/args2short_descrs(\1)/g;
	$switch =~ s/(audio_format_id1, audio_format_code1, audio_format_id2, audio_format_code2, audio_format_id3, audio_format_code3, audio_format_id4, audio_format_code4)/args2short_aud_fmt_ids(audio_format_id1, audio_format_id2, audio_format_id3, audio_format_id4), args2short_aud_fmt_codes(audio_format_code1, audio_format_code2, audio_format_code3, audio_format_code4)/g;
	printf "%s", $switch;
	printf "\t}\n};\n\n";
}

print <<'EOF';
void log_msg(const struct cec_msg *msg)
{
	if (msg->len == 1)
		printf("CEC_MSG_POLL\n");
	if ((msg->tx_status && !(msg->tx_status & CEC_TX_STATUS_OK)) ||
	    (msg->rx_status && !(msg->rx_status & (CEC_RX_STATUS_OK | CEC_RX_STATUS_FEATURE_ABORT))))
		printf("\t%s\n", status2s(*msg).c_str());

	if (msg->len == 1)
		return;

	switch (msg->msg[1]) {
EOF
printf "%s", $logswitch;
print <<'EOF';
	default:
		log_unknown_msg(msg);
		break;
	}
	break;

	default:
		log_unknown_msg(msg);
		break;
	}
}
EOF
