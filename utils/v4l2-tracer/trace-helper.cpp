/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2022 Collabora Ltd.
 */

#include "trace.h"
#include <math.h>

struct trace_context ctx_trace = {};

bool is_video_or_media_device(const char *path)
{
	std::string dev_path_video = "/dev/video";
	std::string dev_path_media = "/dev/media";
	bool is_video = strncmp(path, dev_path_video.c_str(), dev_path_video.length()) == 0;
	bool is_media = strncmp(path, dev_path_media.c_str(), dev_path_media.length()) == 0;
	return (is_video || is_media);
}

void add_device(int fd, std::string path)
{
	debug_line_info("\n\tfd: %d, path: %s", fd, path.c_str());
	std::pair<int, std::string> new_pair = std::make_pair(fd, path);
	ctx_trace.devices.insert(new_pair);
}

std::string get_device(int fd)
{
	std::string path;
	auto it = ctx_trace.devices.find(fd);
	if (it != ctx_trace.devices.end())
		path = it->second;
	return path;
}

void print_devices(void)
{
	if (!is_debug())
		return;
	if (ctx_trace.devices.size())
		fprintf(stderr, "Devices:\n");
	for (auto &device_pair : ctx_trace.devices)
		fprintf(stderr, "fd: %d, path: %s\n", device_pair.first, device_pair.second.c_str());
}

void print_decode_order(void)
{
	if (!is_debug())
		return;
	fprintf(stderr, "Decode order: ");
	for (auto &num : ctx_trace.decode_order)
		fprintf(stderr, "%ld, ",  num);
	fprintf(stderr, ".\n");
}

void set_decode_order(long decode_order)
{
	debug_line_info("\n\t%ld", decode_order);

	auto it = find(ctx_trace.decode_order.begin(), ctx_trace.decode_order.end(), decode_order);
	if (it == ctx_trace.decode_order.end())
		ctx_trace.decode_order.push_front(decode_order);

	print_decode_order();
}

long get_decode_order(void)
{
	long decode_order = 0;
	if (!ctx_trace.decode_order.empty())
		decode_order = ctx_trace.decode_order.front();
	return decode_order;
}

void add_buffer_trace(int fd, __u32 type, __u32 index, __u32 offset = 0)
{
	struct buffer_trace buf = {};
	buf.fd = fd;
	buf.type = type;
	buf.index = index;
	buf.offset = offset;
	buf.display_order = -1;
	ctx_trace.buffers.push_front(buf);
}

void remove_buffer_trace(__u32 type, __u32 index)
{
	for (auto it = ctx_trace.buffers.begin(); it != ctx_trace.buffers.end(); ++it) {
		if ((it->type == type) && (it->index == index)) {
			ctx_trace.buffers.erase(it);
			break;
		}
	}
}

bool buffer_in_trace_context(int fd, __u32 offset)
{
	bool buffer_in_trace_context = false;
	for (auto &b : ctx_trace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			buffer_in_trace_context = true;
			break;
		}
	}
	return buffer_in_trace_context;
}

int get_buffer_fd_trace(__u32 type, __u32 index)
{
	int fd = 0;
	for (auto &b : ctx_trace.buffers) {
		if ((b.type == type) && (b.index == index)) {
			fd = b.fd;
			break;
		}
	}
	return fd;
}

__u32 get_buffer_type_trace(int fd, __u32 offset)
{
	__u32 type = 0;
	for (auto &b : ctx_trace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			type = b.type;
			break;
		}
	}
	return type;
}

int get_buffer_index_trace(int fd, __u32 offset)
{
	int index = -1;
	for (auto &b : ctx_trace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			index = b.index;
			break;
		}
	}
	return index;
}

__u32 get_buffer_offset_trace(__u32 type, __u32 index)
{
	__u32 offset = 0;
	for (auto &b : ctx_trace.buffers) {
		if ((b.type == type) && (b.index == index)) {
			offset = b.offset;
			break;
		}
	}
	return offset;
}

void set_buffer_bytesused_trace(int fd, __u32 offset, __u32 bytesused)
{
	for (auto &b : ctx_trace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			b.bytesused = bytesused;
			break;
		}
	}
}

long get_buffer_bytesused_trace(int fd, __u32 offset)
{
	long bytesused = 0;
	for (auto &b : ctx_trace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			bytesused = b.bytesused;
			break;
		}
	}
	return bytesused;
}

void set_buffer_display_order(int fd, __u32 offset, long display_order)
{
	debug_line_info("\n\t%ld", display_order);
	for (auto &b : ctx_trace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			b.display_order = display_order;
			break;
		}
	}
}

void set_buffer_address_trace(int fd, __u32 offset, unsigned long address)
{
	for (auto &b : ctx_trace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			b.address = address;
			break;
		}
	}
}

unsigned long get_buffer_address_trace(int fd, __u32 offset)
{
	unsigned long address = 0;
	for (auto &b : ctx_trace.buffers) {
		if ((b.fd == fd) && (b.offset == offset)) {
			address = b.address;
			break;
		}
	}
	return address;
}

bool buffer_is_mapped(unsigned long buffer_address)
{
	bool ret = false;
	for (auto &b : ctx_trace.buffers) {
		if (b.address == buffer_address) {
			ret = true;
			break;
		}
	}
	return ret;
}

void print_buffers_trace(void)
{
	if (!is_debug())
		return;
	for (auto &b : ctx_trace.buffers) {
		fprintf(stderr, "fd: %d, %s, index: %d, display_order: %ld, bytesused: %d, ",
		        b.fd, val2s(b.type, v4l2_buf_type_val_def).c_str(), b.index, b.display_order, b.bytesused);
		fprintf(stderr, "address: %lu, offset: %u \n",  b.address, b.offset);
	}
}

unsigned get_expected_length_trace()
{
	/*
	 * TODO: this assumes that the stride is equal to the real width and that the
	 * padding follows the end of the chroma plane. It could be improved by
	 * following the model in v4l2-ctl-streaming.cpp read_write_padded_frame()
	 */
	unsigned expected_length = ctx_trace.width * ctx_trace.height;
	if (ctx_trace.pixelformat == V4L2_PIX_FMT_NV12 || ctx_trace.pixelformat == V4L2_PIX_FMT_YUV420) {
		expected_length *= 3;
		expected_length /= 2;
		expected_length += (expected_length % 2);
	}
	return expected_length;
}

void s_ext_ctrls_setup(struct v4l2_ext_controls *ext_controls)
{
	if (ext_controls->which != V4L2_CTRL_WHICH_REQUEST_VAL)
		return;

	debug_line_info();
	/*
	 * Since userspace sends H264 frames out of order, get information
	 * about the correct display order of each frame so that v4l2-tracer
	 * can write the decoded frames to a file.
	 */
	for (__u32 i = 0; i < ext_controls->count; i++) {
		struct v4l2_ext_control ctrl = ext_controls->controls[i];

		switch (ctrl.id) {
		case V4L2_CID_STATELESS_H264_SPS: {
			ctx_trace.fmt.h264.max_pic_order_cnt_lsb = pow(2, ctrl.p_h264_sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
			break;
		}
		case V4L2_CID_STATELESS_H264_DECODE_PARAMS: {
			long pic_order_cnt_msb;
			int max = ctx_trace.fmt.h264.max_pic_order_cnt_lsb;
			long prev_pic_order_cnt_msb = get_decode_order();
			int prev_pic_order_cnt_lsb = ctx_trace.fmt.h264.pic_order_cnt_lsb;
			int pic_order_cnt_lsb = ctrl.p_h264_decode_params->pic_order_cnt_lsb;

			if (is_debug()) {
				line_info();
				fprintf(stderr, "\tprev_pic_order_cnt_lsb: %d\n", prev_pic_order_cnt_lsb);
				fprintf(stderr, "\tprev_pic_order_cnt_msb: %ld\n", prev_pic_order_cnt_msb);
				fprintf(stderr, "\tpic_order_cnt_lsb: %d\n", pic_order_cnt_lsb);
			}

			/*
			 * TODO: improve the displaying of decoded frames following H264 specification
			 * 8.2.1.1. For now, dump all the previously decoded frames when an IDR_PIC is
			 * received to avoid losing frames although this will still sometimes result
			 * in frames out of order.
			 */
			if ((ctrl.p_h264_decode_params->flags & V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC) != 0U)
				trace_mem_decoded();

			/*
			 * When pic_order_cnt_lsb wraps around to zero, adjust the total count using
			 * max to keep the correct display order.
			 */
			if ((pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
				((prev_pic_order_cnt_lsb - pic_order_cnt_lsb) >= (max / 2))) {
				pic_order_cnt_msb = prev_pic_order_cnt_msb + max;
			} else if ((pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
				((pic_order_cnt_lsb - prev_pic_order_cnt_lsb) > (max / 2))) {
				pic_order_cnt_msb = prev_pic_order_cnt_msb - max;
			} else {
				pic_order_cnt_msb = prev_pic_order_cnt_msb + (pic_order_cnt_lsb - prev_pic_order_cnt_lsb);
			}

			debug_line_info("\n\tpic_order_cnt_msb: %ld", pic_order_cnt_msb);
			ctx_trace.fmt.h264.pic_order_cnt_lsb = pic_order_cnt_lsb;
			set_decode_order(pic_order_cnt_msb);
			break;
		}
		default:
			break;
		}
	}
}

void qbuf_setup(struct v4l2_buffer *buf)
{
	debug_line_info("\n\t%s, index: %d", val2s(buf->type, v4l2_buf_type_val_def).c_str(), buf->index);

	int buf_fd = get_buffer_fd_trace(buf->type, buf->index);
	__u32 buf_offset = get_buffer_offset_trace(buf->type, buf->index);

	__u32 bytesused = 0;
	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		bytesused = buf->m.planes[0].bytesused;
	else if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		bytesused = buf->bytesused;
	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		set_buffer_bytesused_trace(buf_fd, buf_offset, bytesused);

	/* The output buffer should have compressed data just before it is queued, so trace it. */
	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		trace_mem_encoded(buf_fd, buf_offset);
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		/*
		 * If the capture buffer is queued for reuse, trace it before it is reused.
		 * Capture buffers can't be traced using dqbuf because the buffer is mmapped
		 * after the call to dqbuf.
		 */
		trace_mem_decoded();

		/* H264 sets display order in controls, otherwise display just in the order queued. */
		if (ctx_trace.compression_format != V4L2_PIX_FMT_H264_SLICE)
			set_decode_order(get_decode_order() + 1);

		set_buffer_display_order(buf_fd, buf_offset, get_decode_order());
		print_decode_order();
		print_buffers_trace();
	}
}

void dqbuf_setup(struct v4l2_buffer *buf)
{
	debug_line_info("\n\t%s, index: %d", val2s(buf->type, v4l2_buf_type_val_def).c_str(), buf->index);

	int buf_fd = get_buffer_fd_trace(buf->type, buf->index);
	__u32 buf_offset = get_buffer_offset_trace(buf->type, buf->index);

	__u32 bytesused = 0;
	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		bytesused = buf->m.planes[0].bytesused;
	else if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		bytesused = buf->bytesused;

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		set_buffer_bytesused_trace(buf_fd, buf_offset, bytesused);
}

void streamoff_cleanup(v4l2_buf_type buf_type)
{
	debug_line_info();
	if (is_verbose() || (getenv("V4L2_TRACER_OPTION_WRITE_DECODED_TO_YUV_FILE") != nullptr)) {
		fprintf(stderr, "VIDIOC_STREAMOFF: %s\n", val2s(buf_type, v4l2_buf_type_val_def).c_str());
		fprintf(stderr, "%s, %s %s, width: %d, height: %d\n",
		        val2s(ctx_trace.compression_format, v4l2_pix_fmt_val_def).c_str(),
		        val2s(ctx_trace.pixelformat, v4l2_pix_fmt_val_def).c_str(),
		        fcc2s(ctx_trace.pixelformat).c_str(), ctx_trace.width, ctx_trace.height);
	}

	/*
	 * Before turning off the stream, trace any remaining capture buffers that were missed
	 * because they were not queued for reuse.
	 */
	if (buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
	    buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		trace_mem_decoded();
}

void g_fmt_setup_trace(struct v4l2_format *format)
{
	if (format->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		ctx_trace.width = format->fmt.pix.width;
		ctx_trace.height = format->fmt.pix.height;
		ctx_trace.pixelformat = format->fmt.pix.pixelformat;
	}
	if (format->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx_trace.width = format->fmt.pix_mp.width;
		ctx_trace.height = format->fmt.pix_mp.height;
		ctx_trace.pixelformat = format->fmt.pix_mp.pixelformat;
	}
	if (format->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		ctx_trace.compression_format = format->fmt.pix.pixelformat;
	if (format->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		ctx_trace.compression_format = format->fmt.pix_mp.pixelformat;
}

void s_fmt_setup(struct v4l2_format *format)
{
	if (format->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		ctx_trace.compression_format = format->fmt.pix.pixelformat;
	if (format->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		ctx_trace.compression_format = format->fmt.pix_mp.pixelformat;
}

void expbuf_setup(struct v4l2_exportbuffer *export_buffer)
{
	__u32 type = export_buffer->type;
	__u32 index = export_buffer->index;
	int fd_found_in_trace_context = get_buffer_fd_trace(type, index);

	/* If the buffer was already added to the trace context don't add it again. */
	if (fd_found_in_trace_context == export_buffer->fd)
		return;

	/*
	 * If a buffer was previously added to the trace context using the video device
	 * file descriptor, replace the video fd with the more specific buffer fd from EXPBUF.
	 */
	if (fd_found_in_trace_context != 0)
		remove_buffer_trace(type, index);

	add_buffer_trace(export_buffer->fd, type, index);
}

void querybuf_setup(int fd, struct v4l2_buffer *buf)
{
	/* If the buffer was already added to the trace context don't add it again. */
	if (get_buffer_fd_trace(buf->type, buf->index) != 0)
		return;

	if (buf->memory == V4L2_MEMORY_MMAP) {
		__u32 offset = 0;
		if ((buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) ||
		    (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT))
			offset = buf->m.offset;
		if ((buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) ||
		    (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE))
			offset = buf->m.planes->m.mem_offset;
		add_buffer_trace(fd, buf->type, buf->index, offset);
	}
}

void query_ext_ctrl_setup(int fd, struct v4l2_query_ext_ctrl *ptr)
{
	if (ptr->flags & (V4L2_CTRL_FLAG_HAS_PAYLOAD|V4L2_CTRL_FLAG_DYNAMIC_ARRAY)) {
		if (ptr->id == V4L2_CID_STATELESS_HEVC_ENTRY_POINT_OFFSETS)
			ctx_trace.elems = ptr->elems;
	}
}

void write_json_object_to_json_file(json_object *jobj)
{
	std::string json_str;
	if (getenv("V4L2_TRACER_OPTION_COMPACT_PRINT") != nullptr)
		json_str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PLAIN);
	else
		json_str = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);

	if (ctx_trace.trace_file == nullptr) {
		std::string filename;
		if (getenv("TRACE_ID") != nullptr)
			filename = getenv("TRACE_ID");
		ctx_trace.trace_filename = filename;
		ctx_trace.trace_filename += ".json";
		ctx_trace.trace_file = fopen(ctx_trace.trace_filename.c_str(), "a");
	}

	fwrite(json_str.c_str(), sizeof(char), json_str.length(), ctx_trace.trace_file);
	fputs(",\n", ctx_trace.trace_file);
	fflush(ctx_trace.trace_file);
}

void close_json_file(void)
{
	if (ctx_trace.trace_file != nullptr) {
		fclose(ctx_trace.trace_file);
		ctx_trace.trace_file = 0;
	}
}
