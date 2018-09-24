// Texture IDs
#if PIXFMT == V4L2_PIX_FMT_SBGGR8 || PIXFMT == V4L2_PIX_FMT_SGBRG8 || \
    PIXFMT == V4L2_PIX_FMT_SGRBG8 || PIXFMT == V4L2_PIX_FMT_SRGGB8 || \
    PIXFMT == V4L2_PIX_FMT_SBGGR10 || PIXFMT == V4L2_PIX_FMT_SGBRG10 || \
    PIXFMT == V4L2_PIX_FMT_SGRBG10 || PIXFMT == V4L2_PIX_FMT_SRGGB10 || \
    PIXFMT == V4L2_PIX_FMT_SBGGR12 || PIXFMT == V4L2_PIX_FMT_SGBRG12 || \
    PIXFMT == V4L2_PIX_FMT_SGRBG12 || PIXFMT == V4L2_PIX_FMT_SRGGB12 || \
    PIXFMT == V4L2_PIX_FMT_GREY || PIXFMT == V4L2_PIX_FMT_Y16 || \
    PIXFMT == V4L2_PIX_FMT_Y16_BE || PIXFMT == V4L2_PIX_FMT_Z16 || \
    PIXFMT == V4L2_PIX_FMT_Y10 || PIXFMT == V4L2_PIX_FMT_Y12
uniform highp usampler2D tex;
#else
uniform sampler2D tex;
#endif
uniform sampler2D ytex;
uniform sampler2D uvtex;
uniform sampler2D utex;
uniform sampler2D vtex;

in vec2 vs_TexCoord;

out vec4 fs_FragColor;

// YUV (aka Y'CbCr) to R'G'B' matrices

const mat3 yuv2rgb = mat3(
#if YCBCRENC == V4L2_YCBCR_ENC_SMPTE240M
	// Old obsolete HDTV standard. Replaced by REC 709.
	// SMPTE 240M has its own luma coefficients
	1.0,     1.0,     1.0,
	0.0,    -0.2253,  1.8270,
	1.5756, -0.4768,  0.0
#elif YCBCRENC == V4L2_YCBCR_ENC_BT2020
	// BT.2020 luma coefficients
	1.0,     1.0,     1.0,
	0.0,    -0.1646,  1.8814,
	1.4719, -0.5703,  0.0
#elif YCBCRENC == V4L2_YCBCR_ENC_601 || YCBCRENC == V4L2_YCBCR_ENC_XV601
	// These colorspaces all use the BT.601 luma coefficients
	1.0,    1.0,    1.0,
	0.0,   -0.344,  1.773,
	1.403, -0.714,  0.0
#else
	// The HDTV colorspaces all use REC 709 luma coefficients
	1.0,     1.0,     1.0,
	0.0,    -0.1870,  1.8556,
	1.5701, -0.4664,  0.0
#endif
);

// Various colorspace conversion matrices that transfer the source chromaticities
// to the sRGB/Rec.709 chromaticities

const mat3 colconv = mat3(
#if COLSP == V4L2_COLORSPACE_SMPTE170M || COLSP == V4L2_COLORSPACE_SMPTE240M
	// Current SDTV standard, although slowly being replaced by REC 709.
	// Uses the SMPTE 170M aka SMPTE-C aka SMPTE RP 145 conversion matrix.
	0.939536,  0.017743, -0.001591,
	0.050215,  0.965758, -0.004356,
	0.001789,  0.016243,  1.005951
#elif COLSP == V4L2_COLORSPACE_470_SYSTEM_M
	// Old obsolete NTSC standard. Replaced by REC 709.
	// Uses the NTSC 1953 conversion matrix and the Bradford method to
	// compensate for the different whitepoints.
	 1.4858417, -0.0251179, -0.0272254,
	-0.4033361,  0.9541568, -0.0440815,
	-0.0825056,  0.0709611,  1.0713068
#elif COLSP == V4L2_COLORSPACE_470_SYSTEM_BG
	// Old obsolete PAL/SECAM standard. Replaced by REC 709.
	// Uses the EBU Tech. 3213 conversion matrix.
	 1.0440, 0,  0,
	-0.0440, 1, -0.0119,
	 0,      0,  1.0119
#elif COLSP == V4L2_COLORSPACE_OPRGB
	 1.3982832, 0,  0,
	-0.3982831, 1, -0.0429383,
	 0,         0,  1.0429383
#elif COLSP == V4L2_COLORSPACE_DCI_P3
	// Uses the Bradford method to compensate for the different whitepoints.
	 1.1574000, -0.0415052, -0.0180562,
	-0.1548597,  1.0455684, -0.0785993,
	-0.0025403, -0.0040633,  1.0966555
#elif COLSP == V4L2_COLORSPACE_BT2020
	 1.6603627, -0.1245635, -0.0181566,
	-0.5875400,  1.1329114, -0.1006017,
	-0.0728227, -0.0083478,  1.1187583
#else
	// Identity matrix
	1.0
#endif
);

void main()
{
	const float texl_w = 1.0 / tex_w;
	const float texl_h = 1.0 / tex_h;
	float alpha = 0.0;
	vec2 xy = vs_TexCoord;
	float xcoord = floor(xy.x * tex_w);
	float ycoord = floor(xy.y * tex_h);
	bool xeven = mod(xcoord, 2.0) == 0.0;
	bool yeven = mod(ycoord, 2.0) == 0.0;
	vec3 yuv;
	vec3 rgb;

#if FIELD == V4L2_FIELD_SEQ_TB
	xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;
#elif FIELD == V4L2_FIELD_SEQ_BT
	xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;
#endif

#if IS_RGB

// Bayer pixel formats
#if PIXFMT == V4L2_PIX_FMT_SBGGR8 || PIXFMT == V4L2_PIX_FMT_SBGGR10 || PIXFMT == V4L2_PIX_FMT_SBGGR12
	uvec4 urgb;
	vec2 cell = vec2(xeven ? xy.x : xy.x - texl_w, yeven ? xy.y : xy.y - texl_h);
	urgb.r = texture(tex, vec2(cell.x + texl_w, cell.y + texl_h)).r;
	urgb.g = texture(tex, vec2((cell.y == xy.y) ? cell.x + texl_w : cell.x, xy.y)).r;
	urgb.b = texture(tex, cell).r;
#elif PIXFMT == V4L2_PIX_FMT_SGBRG8 || PIXFMT == V4L2_PIX_FMT_SGBRG10 || PIXFMT == V4L2_PIX_FMT_SGBRG12
	uvec4 urgb;
	vec2 cell = vec2(xeven ? xy.x : xy.x - texl_w, yeven ? xy.y : xy.y - texl_h);
	urgb.r = texture(tex, vec2(cell.x, cell.y + texl_h)).r;
	urgb.g = texture(tex, vec2((cell.y == xy.y) ? cell.x : cell.x + texl_w, xy.y)).r;
	urgb.b = texture(tex, vec2(cell.x + texl_w, cell.y)).r;
#elif PIXFMT == V4L2_PIX_FMT_SGRBG8 || PIXFMT == V4L2_PIX_FMT_SGRBG10 || PIXFMT == V4L2_PIX_FMT_SGRBG12
	uvec4 urgb;
	vec2 cell = vec2(xeven ? xy.x : xy.x - texl_w, yeven ? xy.y : xy.y - texl_h);
	urgb.r = texture(tex, vec2(cell.x + texl_w, cell.y)).r;
	urgb.g = texture(tex, vec2((cell.y == xy.y) ? cell.x : cell.x + texl_w, xy.y)).r;
	urgb.b = texture(tex, vec2(cell.x, cell.y + texl_h)).r;
#elif PIXFMT == V4L2_PIX_FMT_SRGGB8 || PIXFMT == V4L2_PIX_FMT_SRGGB10 || PIXFMT == V4L2_PIX_FMT_SRGGB12
	uvec4 urgb;
	vec2 cell = vec2(xeven ? xy.x : xy.x - texl_w, yeven ? xy.y : xy.y - texl_h);
	urgb.b = texture(tex, vec2(cell.x + texl_w, cell.y + texl_h)).r;
	urgb.g = texture(tex, vec2((cell.y == xy.y) ? cell.x + texl_w : cell.x, xy.y)).r;
	urgb.r = texture(tex, cell).r;
#elif PIXFMT == V4L2_PIX_FMT_RGB32 || PIXFMT == V4L2_PIX_FMT_XRGB32 || PIXFMT == V4L2_PIX_FMT_ARGB32 || \
	PIXFMT == V4L2_PIX_FMT_RGB444 || PIXFMT == V4L2_PIX_FMT_XRGB444 || PIXFMT == V4L2_PIX_FMT_ARGB444
	vec4 cell = texture(tex, xy);
#if V4L2_PIX_FMT_ARGB444 || PIXFMT == V4L2_PIX_FMT_ARGB32
	alpha = cell.r;
#endif
	rgb.rgb = cell.gba;
#elif PIXFMT == V4L2_PIX_FMT_BGR32 || PIXFMT == V4L2_PIX_FMT_XBGR32 || PIXFMT == V4L2_PIX_FMT_ABGR32
	vec4 cell = texture(tex, xy);
#if PIXFMT == V4L2_PIX_FMT_ABGR32
	alpha = cell.a;
#endif
	rgb.rgb = cell.bgr;
#elif PIXFMT == V4L2_PIX_FMT_GREY
	rgb.rgb = vec3(float(texture(tex, xy).r) / 255.0);
#elif PIXFMT == V4L2_PIX_FMT_Y10
	rgb.rgb = vec3(float(texture(tex, xy).r) / 1023.0);
#elif PIXFMT == V4L2_PIX_FMT_Y12
	rgb.rgb = vec3(float(texture(tex, xy).r) / 4095.0);
#elif PIXFMT == V4L2_PIX_FMT_Y16 || PIXFMT == V4L2_PIX_FMT_Z16
	rgb.rgb = vec3(float(texture(tex, xy).r) / 65535.0);
#elif PIXFMT == V4L2_PIX_FMT_Y16_BE
	uint low = texture(tex, xy).r >> 8;
	uint high = (texture(tex, xy).r & 0xFFu) << 8;
	rgb.rgb = vec3(float(high | low) / 65535.0);
#else
	vec4 color = texture(tex, xy);

// RGB pixel formats with an alpha component
#if PIXFMT == V4L2_PIX_FMT_ARGB555 || PIXFMT == V4L2_PIX_FMT_ARGB555X
	alpha = color.a;
#endif

#if PIXFMT == V4L2_PIX_FMT_BGR666
	vec3 frgb = floor(color.rgb * 255.0);
	frgb.r = floor(frgb.r / 64.0) + mod(frgb.g, 16.0) * 4.0;
	frgb.g = floor(frgb.g / 16.0) + mod(frgb.b, 4.0) * 16.0;
	frgb.b = floor(frgb.b / 4.0);
	rgb = frgb / 63.0;
#elif PIXFMT == V4L2_PIX_FMT_BGR24
	rgb = color.bgr;
#else
	rgb = color.rgb;
#endif

#endif

#if PIXFMT == V4L2_PIX_FMT_SBGGR8 || PIXFMT == V4L2_PIX_FMT_SGBRG8 || \
    PIXFMT == V4L2_PIX_FMT_SGRBG8 || PIXFMT == V4L2_PIX_FMT_SRGGB8
	rgb = vec3(urgb) / 255.0;
#elif PIXFMT == V4L2_PIX_FMT_SBGGR10 || PIXFMT == V4L2_PIX_FMT_SGBRG10 || \
      PIXFMT == V4L2_PIX_FMT_SGRBG10 || PIXFMT == V4L2_PIX_FMT_SRGGB10
	rgb = vec3(urgb) / 1023.0;
#elif PIXFMT == V4L2_PIX_FMT_SBGGR12 || PIXFMT == V4L2_PIX_FMT_SGBRG12 || \
      PIXFMT == V4L2_PIX_FMT_SGRBG12 || PIXFMT == V4L2_PIX_FMT_SRGGB12
	rgb = vec3(urgb) / 4095.0;
#endif

#if QUANT == V4L2_QUANTIZATION_LIM_RANGE
	rgb -= 16.0 / 255.0;
	rgb *= 255.0 / 219.0;
#endif

#else // IS_RGB

#if PIXFMT == V4L2_PIX_FMT_YUYV
	vec4 luma_chroma = texture(tex, xeven ? xy : vec2(xy.x - texl_w, xy.y));
	yuv.r = xeven ? luma_chroma.r : luma_chroma.b;
	yuv.gb = luma_chroma.ga;
#elif PIXFMT == V4L2_PIX_FMT_YVYU
	vec4 luma_chroma = texture(tex, xeven ? xy : vec2(xy.x - texl_w, xy.y));
	yuv.r = xeven ? luma_chroma.r : luma_chroma.b;
	yuv.gb = luma_chroma.ag;
#elif PIXFMT == V4L2_PIX_FMT_UYVY
	vec4 luma_chroma = texture(tex, xeven ? xy : vec2(xy.x - texl_w, xy.y));
	yuv.r = xeven ? luma_chroma.g : luma_chroma.a;
	yuv.gb = luma_chroma.rb;
#elif PIXFMT == V4L2_PIX_FMT_VYUY
	vec4 luma_chroma = texture(tex, xeven ? xy : vec2(xy.x - texl_w, xy.y));
	yuv.r = xeven ? luma_chroma.g : luma_chroma.a;
	yuv.gb = luma_chroma.br;
#elif PIXFMT == V4L2_PIX_FMT_NV16 || PIXFMT == V4L2_PIX_FMT_NV16M || \
      PIXFMT == V4L2_PIX_FMT_NV12 || PIXFMT == V4L2_PIX_FMT_NV12M
	yuv.r = texture(ytex, xy).r;
	if (xeven) {
		yuv.g = texture(uvtex, xy).r;
		yuv.b = texture(uvtex, vec2(xy.x + texl_w, xy.y)).r;
	} else {
		yuv.g = texture(uvtex, vec2(xy.x - texl_w, xy.y)).r;
		yuv.b = texture(uvtex, xy).r;
	}
#elif PIXFMT == V4L2_PIX_FMT_NV61 || PIXFMT == V4L2_PIX_FMT_NV61M || \
      PIXFMT == V4L2_PIX_FMT_NV21 || PIXFMT == V4L2_PIX_FMT_NV21M
	yuv.r = texture(ytex, xy).r;
	if (xeven) {
		yuv.g = texture(uvtex, vec2(xy.x + texl_w, xy.y)).r;
		yuv.b = texture(uvtex, xy).r;
	} else {
		yuv.g = texture(uvtex, xy).r;
		yuv.b = texture(uvtex, vec2(xy.x - texl_w, xy.y)).r;
	}
#elif PIXFMT == V4L2_PIX_FMT_NV24
	yuv.r = texture(ytex, xy).r;
	yuv.g = texture(uvtex, xy).r;
	yuv.b = texture(uvtex, xy).g;
#elif PIXFMT == V4L2_PIX_FMT_NV42
	yuv.r = texture(ytex, xy).r;
	yuv.g = texture(uvtex, xy).g;
	yuv.b = texture(uvtex, xy).r;
#elif PIXFMT == V4L2_PIX_FMT_YUV555
	vec4 color = texture(tex, xy);
	alpha = color.a;
	yuv = color.rgb;
#elif PIXFMT == V4L2_PIX_FMT_YUV444 || PIXFMT == V4L2_PIX_FMT_YUV32
	vec4 color = texture(tex, xy);
	alpha = color.r;
	yuv.r = color.g;
	yuv.g = color.b;
	yuv.b = color.a;
#elif PIXFMT == V4L2_PIX_FMT_YUV565
	yuv = texture(tex, xy).rgb;
#elif PIXFMT == V4L2_PIX_FMT_YUV422P || PIXFMT == V4L2_PIX_FMT_YUV420 || PIXFMT == V4L2_PIX_FMT_YVU420 || \
      PIXFMT == V4L2_PIX_FMT_YUV420M || PIXFMT == V4L2_PIX_FMT_YVU420M || \
      PIXFMT == V4L2_PIX_FMT_YUV422M || PIXFMT == V4L2_PIX_FMT_YVU422M || \
      PIXFMT == V4L2_PIX_FMT_YUV444M || PIXFMT == V4L2_PIX_FMT_YVU444M
	yuv = vec3(texture(ytex, xy).r, texture(utex, xy).r, texture(vtex, xy).r);
#endif

#if IS_HSV
	vec4 color = texture(tex, xy);

#if PIXFMT == V4L2_PIX_FMT_HSV32
	color = color.gbar;
#endif
	// From http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
	float hue = color.r;

#if HSVENC == V4L2_HSV_ENC_180
	hue = (hue * 256.0) / 180.0;
#endif
	vec3 c = vec3(hue, color.g, color.b);
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	rgb = c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
#else // IS_HSV
	yuv.gb -= 0.5;
#endif

#if QUANT != V4L2_QUANTIZATION_FULL_RANGE || YCBCRENC == V4L2_YCBCR_ENC_XV601 || YCBCRENC == V4L2_YCBCR_ENC_XV709
	/*
	 * xv709 and xv601 have full range quantization, but they still
	 * need to be normalized as if they were limited range. But the
	 * result are values outside the normal 0-1 range, which is the
	 * point of these extended gamut encodings.
	 */
	const vec3 scale = vec3(255.0 / 219.0, 255.0 / 224.0, 255.0 / 224.0);
	const vec3 offset = vec3(16.0 / 255.0, 0.0, 0.0);

	yuv -= offset;
	yuv *= scale;
#endif

#if YCBCRENC == V4L2_YCBCR_ENC_BT2020_CONST_LUM
	// BT.2020_CONST_LUM luma coefficients
	float y = yuv.r;
	float u = yuv.g;
	float v = yuv.b;
	float b = u <= 0.0 ? y + 1.9404 * u : y + 1.5816 * u;
	float r = v <= 0.0 ? y + 1.7184 * v : y + 0.9936 * v;
	float lin_r = (r < 0.081) ? r / 4.5 : pow((r + 0.099) / 1.099, 1.0 / 0.45);
	float lin_b = (b < 0.081) ? b / 4.5 : pow((b + 0.099) / 1.099, 1.0 / 0.45);
	float lin_y = (y < 0.081) ? y / 4.5 : pow((y + 0.099) / 1.099, 1.0 / 0.45);
	float lin_g = lin_y / 0.6780 - lin_r * 0.2627 / 0.6780 - lin_b * 0.0593 / 0.6780;
	float g = (lin_g < 0.018) ? lin_g * 4.5 : 1.099 * pow(lin_g, 0.45) - 0.099;
	rgb = vec3(r, g, b);
#elif !IS_HSV
	rgb = yuv2rgb * yuv;
#endif
#endif // !IS_RGB

// Convert non-linear R'G'B' to linear RGB, taking into account the
// colorspace.
#if XFERFUNC == V4L2_XFER_FUNC_SMPTE240M

// Old obsolete HDTV standard. Replaced by REC 709.
// This is the transfer function for SMPTE 240M
#define XFER(c) (((c) < 0.0913) ? (c) / 4.0 : pow(((c) + 0.1115) / 1.1115, 1.0 / 0.45))

	rgb = vec3(XFER(rgb.r), XFER(rgb.g), XFER(rgb.b));

#elif XFERFUNC == V4L2_XFER_FUNC_SRGB

// This is used for sRGB as specified by the IEC FDIS 61966-2-1 standard
#define XFER(c) (((c) < -0.04045) ? -pow((-(c) + 0.055) / 1.055, 2.4) : \
		(((c) <= 0.04045) ? (c) / 12.92 : pow(((c) + 0.055) / 1.055, 2.4)))

	rgb = vec3(XFER(rgb.r), XFER(rgb.g), XFER(rgb.b));

#elif XFERFUNC == V4L2_XFER_FUNC_OPRGB

	// Avoid powers of negative numbers
	rgb = max(rgb, vec3(0.0));
	rgb = pow(rgb, vec3(2.19921875));

#elif XFERFUNC == V4L2_XFER_FUNC_DCI_P3

	// Avoid powers of negative numbers
	rgb = max(rgb, vec3(0.0));
	rgb = pow(rgb, vec3(2.6));

#elif XFERFUNC == V4L2_XFER_FUNC_SMPTE2084
	const vec3 m1 = vec3(1.0 / ((2610.0 / 4096.0) / 4.0));
	const vec3 m2 = vec3(1.0 / (128.0 * 2523.0 / 4096.0));
	const vec3 c1 = vec3(3424.0 / 4096.0);
	const vec3 c2 = vec3(32.0 * 2413.0 / 4096.0);
	const vec3 c3 = vec3(32.0 * 2392.0 / 4096.0);

	// Avoid powers of negative numbers
	rgb = max(rgb, vec3(0.0));
	rgb = pow(rgb, m2);
	// The factor 100 is because SMPTE-2084 maps to 0-10000 cd/m^2
	// whereas other transfer functions map to 0-100 cd/m^2.
	rgb = pow(max(rgb - c1, vec3(0.0)) / (c2 - rgb * c3), m1) * 100.0;

#elif XFERFUNC != V4L2_XFER_FUNC_NONE

// All others use the transfer function specified by REC 709
#define XFER(c) (((c) <= -0.081) ? -pow(((c) - 0.099) / -1.099, 1.0 / 0.45) : \
		 (((c) < 0.081) ? (c) / 4.5 : pow(((c) + 0.099) / 1.099, 1.0 / 0.45)))

	rgb = vec3(XFER(rgb.r), XFER(rgb.g), XFER(rgb.b));

#endif

// Convert the given colorspace to the REC 709/sRGB colorspace. All colors are
// specified as linear RGB.
#if COLSP == V4L2_COLORSPACE_SMPTE170M || COLSP == V4L2_COLORSPACE_SMPTE240M || \
    COLSP == V4L2_COLORSPACE_470_SYSTEM_M || COLSP == V4L2_COLORSPACE_470_SYSTEM_BG || \
    COLSP == V4L2_COLORSPACE_OPRGB || COLSP == V4L2_COLORSPACE_DCI_P3 || \
    COLSP == V4L2_COLORSPACE_BT2020
	rgb = colconv * rgb;
#endif

// Convert linear RGB to non-linear R'G'B', assuming an sRGB display colorspace.

#define XFER_SRGB(c) (((c) < -0.0031308) ? -1.055 * pow(-(c), 1.0 / 2.4) + 0.055 : \
	(((c) <= 0.0031308) ? (c) * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055))

	rgb = vec3(XFER_SRGB(rgb.r), XFER_SRGB(rgb.g), XFER_SRGB(rgb.b));

	fs_FragColor = vec4(rgb, alpha);
}
