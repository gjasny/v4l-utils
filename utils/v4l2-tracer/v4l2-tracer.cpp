/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#include "retrace.h"
#include <climits>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

int tracer(int argc, char *argv[], bool retrace = false);

pid_t tracee_pid = 0;

void v4l2_tracer_sig_handler(int signum)
{
	line_info("\n\tReceived signum: %d", signum);

	/* If there is a tracee, wait for it to handle the signal first before exiting. */
	if (tracee_pid) {
		kill(tracee_pid, signum);
		wait(nullptr);
	}
}

enum Options {
	V4l2TracerOptCompactPrint = 'c',
	V4l2TracerOptSetVideoDevice = 'd',
	V4l2TracerOptDebug = 'g',
	V4l2TracerOptHelp = 'h',
	V4l2TracerOptSetMediaDevice = 'm',
	V4l2TracerOptWriteDecodedToJson = 'r',
	V4l2TracerOptTraceUserspaceArg = 'u',
	V4l2TracerOptVerbose = 'v',
	V4l2TracerOptWriteDecodedToYUVFile = 'y',
};

const static struct option long_options[] = {
	{ "compact", no_argument, nullptr, V4l2TracerOptCompactPrint },
	{ "video_device", required_argument, nullptr, V4l2TracerOptSetVideoDevice },
	{ "debug", no_argument, nullptr, V4l2TracerOptDebug },
	{ "help", no_argument, nullptr, V4l2TracerOptHelp },
	{ "media_device", required_argument, nullptr, V4l2TracerOptSetMediaDevice },
	{ "raw", no_argument, nullptr, V4l2TracerOptWriteDecodedToJson },
	{ "userspace", no_argument, nullptr, V4l2TracerOptTraceUserspaceArg},
	{ "verbose", no_argument, nullptr, V4l2TracerOptVerbose },
	{ "yuv", no_argument, nullptr, V4l2TracerOptWriteDecodedToYUVFile },
	{ nullptr, 0, nullptr, 0 }
};

const char short_options[] = {
	V4l2TracerOptCompactPrint,
	V4l2TracerOptSetVideoDevice, ':',
	V4l2TracerOptDebug,
	V4l2TracerOptHelp,
	V4l2TracerOptSetMediaDevice, ':',
	V4l2TracerOptWriteDecodedToJson,
	V4l2TracerOptTraceUserspaceArg,
	V4l2TracerOptVerbose,
	V4l2TracerOptWriteDecodedToYUVFile
};

int get_options(int argc, char *argv[])
{
	int option = 0;

	do {
		/* If there are no commands after the valid options, return err. */
		if (optind == argc) {
			print_usage();
			return -1;
		}

		/* Avoid reading the tracee's options. */
		if ((strcmp(argv[optind], "trace") == 0) || (strcmp(argv[optind], "retrace") == 0))
			return 0;

		option = getopt_long(argc, argv, short_options, long_options, NULL);
		switch (option) {
		case V4l2TracerOptCompactPrint: {
			setenv("V4L2_TRACER_OPTION_COMPACT_PRINT", "true", 0);
			break;
		}
		case V4l2TracerOptSetVideoDevice: {
			std::string device_num = optarg;
			try {
				std::stoi(device_num, nullptr, 0);
			} catch (std::exception& e) {
				line_info("\n\tCan't convert <dev> \'%s\' to integer.", device_num.c_str());
				return -1;
			}
			if (device_num[0] >= '0' && device_num[0] <= '9' && device_num.length() <= 3) {
				std::string path_video = "/dev/video";
				path_video += optarg;
				setenv("V4L2_TRACER_OPTION_SET_VIDEO_DEVICE", path_video.c_str(), 0);
			} else {
				line_info("\n\tCan't use device number\'%s\'", device_num.c_str());
				return -1;
			}
			break;
		}
		case V4l2TracerOptDebug:
			setenv("V4L2_TRACER_OPTION_VERBOSE", "true", 0);
			setenv("V4L2_TRACER_OPTION_DEBUG", "true", 0);
			break;
		case V4l2TracerOptHelp:
			print_usage();
			return -1;
		case V4l2TracerOptSetMediaDevice: {
			std::string device_num = optarg;
			try {
				std::stoi(device_num, nullptr, 0);
			} catch (std::exception& e) {
				line_info("\n\tCan't convert <dev> \'%s\' to integer.", device_num.c_str());
				return -1;
			}
			if (device_num[0] >= '0' && device_num[0] <= '9' && device_num.length() <= 3) {
				std::string path_media = "/dev/media";
				path_media += optarg;
				setenv("V4L2_TRACER_OPTION_SET_MEDIA_DEVICE", path_media.c_str(), 0);
			} else {
				line_info("\n\tCan't use device number\'%s\'", device_num.c_str());
				return -1;
			}
			break;
		}
		case V4l2TracerOptWriteDecodedToJson:
			setenv("V4L2_TRACER_OPTION_WRITE_DECODED_TO_JSON_FILE", "true", 0);
			break;
		case V4l2TracerOptTraceUserspaceArg:
			setenv("V4L2_TRACER_OPTION_TRACE_USERSPACE_ARG", "true", 0);
			break;
		case V4l2TracerOptVerbose:
			setenv("V4L2_TRACER_OPTION_VERBOSE", "true", 0);
			break;
		case V4l2TracerOptWriteDecodedToYUVFile:
			setenv("V4L2_TRACER_OPTION_WRITE_DECODED_TO_YUV_FILE", "true", 0);
			break;
		default:
			break;
		}

		/* invalid option */
		if (optopt > 0) {
			print_usage();
			return -1;
		}

	} while (option != -1);

	return 0;
}

int clean(std::string trace_filename)
{
	FILE *trace_file = fopen(trace_filename.c_str(), "r");
	if (trace_file == nullptr) {
		line_info("\n\tCan't open \'%s\'", trace_filename.c_str());
		return 1;
	}

	fprintf(stderr, "Cleaning: %s\n", trace_filename.c_str());

	std::string clean_filename = "clean_" + trace_filename;
	FILE *clean_file = fopen(clean_filename.c_str(), "w");
	if (clean_file == nullptr) {
		line_info("\n\tCan't open \'%s\'", clean_filename.c_str());
		return 1;
	}

	std::string line;
	char buf[SHRT_MAX];
	int count_total = 0;
	int count_lines_removed = 0;

	while (fgets(buf, SHRT_MAX, trace_file) != nullptr) {
		line = buf;
		count_total++;
		if (line.find("fd") != std::string::npos) {
			count_lines_removed++;
			continue;
		}
		if (line.find("address") != std::string::npos) {
			count_lines_removed++;
			continue;
		}
		if (line.find("fildes") != std::string::npos) {
			count_lines_removed++;
			continue;
		}
		if (line.find("\"start\"") != std::string::npos) {
			count_lines_removed++;
			continue;
		}
		if (line.find("\"name\"") != std::string::npos) {
			count_lines_removed++;
			continue;
		}

		fputs(buf, clean_file);
	}

	fclose(trace_file);
	fclose(clean_file);
	fprintf(stderr, "Removed %d lines of %d total lines: %s\n",
	        count_lines_removed, count_total, clean_filename.c_str());

	return 0;
}

int tracer(int argc, char *argv[], bool retrace)
{
	char *exec[argc];
	int exec_index = 0;

	char retrace_command[] = "__retrace";

	if (retrace) {
		std::string trace_file = argv[optind];
		if (trace_file.find(".json") == std::string::npos) {
			line_info("\n\tTrace file \'%s\' must have .json file extension",
			          trace_file.c_str());
			print_usage();
			return -1;
		}
	}

	/* Get the application to be traced. */
	if (retrace) {
		exec[exec_index++] = argv[0]; /* tracee is v4l2-tracer, local or installed */
		exec[exec_index++] = retrace_command;
		exec[exec_index++] = argv[optind]; /* json file to be retraced */
	} else {
		while (optind < argc)
			exec[exec_index++] = argv[optind++];
	}
	exec[exec_index] = nullptr;

	/* Create a unique trace filename and open a file. */
	std::string trace_id;
	if (retrace) {
		std::string json_file_name = argv[optind];
		trace_id = json_file_name.substr(0, json_file_name.find(".json"));
		trace_id += "_retrace";
	} else {
		const int timestamp_start_pos = 1;
		trace_id = std::to_string(100000 + time(nullptr) % 100000);
		trace_id = trace_id.substr(timestamp_start_pos) + "_trace";
	}
	setenv("TRACE_ID", trace_id.c_str(), 0);
	std::string trace_filename = trace_id + ".json";
	FILE *trace_file = fopen(trace_filename.c_str(), "w");
	if (trace_file == nullptr) {
		fprintf(stderr, "Could not open trace file: %s\n", trace_filename.c_str());
		perror("");
		return errno;
	}

	/* Open the json array.*/
	fputs("[\n", trace_file);

	/* Add v4l-utils package and git info to the top of the trace file. */
	std::string json_str;
	json_object *v4l2_tracer_info_obj = json_object_new_object();
	json_object_object_add(v4l2_tracer_info_obj, "package_version",
	                       json_object_new_string(PACKAGE_VERSION));
	std::string git_commit_cnt = STRING(GIT_COMMIT_CNT);
	git_commit_cnt = git_commit_cnt.erase(0, 1); /* remove the hyphen in front of git_commit_cnt */
	json_object_object_add(v4l2_tracer_info_obj, "git_commit_cnt",
	                       json_object_new_string(git_commit_cnt.c_str()));
	json_object_object_add(v4l2_tracer_info_obj, "git_sha",
	                       json_object_new_string(STRING(GIT_SHA)));
	json_object_object_add(v4l2_tracer_info_obj, "git_commit_date",
	                       json_object_new_string(STRING(GIT_COMMIT_DATE)));
	json_str = json_object_to_json_string(v4l2_tracer_info_obj);
	fwrite(json_str.c_str(), sizeof(char), json_str.length(), trace_file);
	fputs(",\n", trace_file);
	json_object_put(v4l2_tracer_info_obj);

	/* Add v4l2-tracer command line to the top of the trace file. */
	json_object *tracee_obj = json_object_new_object();
	std::string tracee;
	for (int i = 0; i < argc; i++) {
		tracee += argv[i];
		tracee += " ";
	}
	json_object_object_add(tracee_obj, "Trace", json_object_new_string(tracee.c_str()));
	const time_t current_time = time(nullptr);
	json_object_object_add(tracee_obj, "Timestamp", json_object_new_string(ctime(&current_time)));

	json_str = json_object_to_json_string(tracee_obj);
	fwrite(json_str.c_str(), sizeof(char), json_str.length(), trace_file);
	fputs(",\n", trace_file);
	json_object_put(tracee_obj);
	fclose(trace_file);

	/*
	 * Preload the libv4l2tracer library. The tracer is looked up using following order:
	 * 1. Check if LD_PRELOAD is already set, in which case just honor it
	 * 2. Check V4L2_TRACER_PATH env is set (meson devenv / uninstalled)
	 * 3. Check in the prefix/libdir path for an installed tracer.
	 */
	std::string libv4l2tracer_path;
	if (getenv("LD_PRELOAD"))
		libv4l2tracer_path = std::string(getenv("LD_PRELOAD"));
	else if (getenv("V4L2_TRACER"))
		libv4l2tracer_path = std::string(getenv("V4L2_TRACER"));
	else
		libv4l2tracer_path = std::string(LIBTRACER_PATH) + "/libv4l2tracer.so";

	struct stat sb;
	if (stat(libv4l2tracer_path.c_str(), &sb) == -1) {
		if (stat(libv4l2tracer_path.c_str(), &sb) == -1) {
			fprintf(stderr, "Exiting: can't find libv4l2tracer library in %s\n", libv4l2tracer_path.c_str());
			fprintf(stderr, "If you are using a different location, try setting the env 'V4L2_TRACER'\n");
			exit(EXIT_FAILURE);
		}
	}

	if (is_verbose())
		fprintf(stderr, "Loading libv4l2tracer: %s\n", libv4l2tracer_path.c_str());
	setenv("LD_PRELOAD", libv4l2tracer_path.c_str(), 0);

	tracee_pid = fork();
	if (tracee_pid == 0) {

		if (is_debug()) {
			line_info();
			fprintf(stderr, "\ttracee: ");
			for (int i = 0; i < exec_index; i++)
				fprintf(stderr,"%s ", exec[i]);
			fprintf(stderr, "\n");
		}

		execvpe(exec[0], (char* const*) exec, environ);
		line_info("\n\tCould not execute application \'%s\'", exec[0]);
		perror(" ");
		return errno;
	}

	int exec_result = 0;
	wait(&exec_result);

	if (WIFEXITED(exec_result))
		WEXITSTATUS(exec_result);

	fprintf(stderr, "Tracee exited with status: %d\n", exec_result);

	/* Close the json-array and the trace file. */
	trace_file = fopen(trace_filename.c_str(), "r+");
	fseek(trace_file, -2L, SEEK_END);
	fputs("\n]\n", trace_file);
	fclose(trace_file);

	if (retrace)
		fprintf(stderr, "Retrace complete: ");
	else
		fprintf(stderr, "Trace complete: ");
	fprintf(stderr, "%s", trace_filename.c_str());
	fprintf(stderr, "\n");

	unsetenv("LD_PRELOAD");
	return exec_result;
}

int main(int argc, char *argv[])
{
	int ret = -1;

	if (argc <= 1) {
		print_usage();
		return ret;
	}

	ret = get_options(argc, argv);

	if (ret < 0) {
		debug_line_info();
		return ret;
	}

	if (optind == argc) {
		debug_line_info();
		print_usage();
		return ret;
	}

	std::string command = argv[optind++];

	if (optind == argc) {
		debug_line_info();
		print_usage();
		return ret;
	}

	struct sigaction act = {};
	act.sa_handler = v4l2_tracer_sig_handler;
	sigaction(SIGINT, &act, nullptr);
	sigaction(SIGTERM, &act, nullptr);

	if (command == "trace") {
		ret = tracer(argc, argv);
	} else if (command == "retrace") {
		ret = tracer(argc, argv, true);
	} else if (command == "__retrace") {
		/*
		 * This command is meant to be used only internally to allow
		 * v4l2-tracer to recursively trace itself during a retrace.
		 */
		ret = retrace(argv[optind]);
	} else if (command == "clean") {
		ret = clean (argv[optind]);
	} else {
		if (is_debug()) {
			line_info("Invalid command");
			for (int i = 0; i < argc; i++)
				fprintf(stderr,"%s ", argv[i]);
			fprintf(stderr, "\n");
		}
		print_usage();
	}

	return ret;
}
