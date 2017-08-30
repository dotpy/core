/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "base64.h"
#include "buffer.h"

static const char b64enc[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char b64dec[256] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 0-7 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 8-15 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 16-23 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 24-31 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 32-39 */
	0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f, /* 40-47 */
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, /* 48-55 */
	0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 56-63 */
	0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, /* 64-71 */
	0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, /* 72-79 */
	0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, /* 80-87 */
	0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff, /* 88-95 */
	0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, /* 96-103 */
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, /* 104-111 */
	0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, /* 112-119 */
	0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, /* 120-127 */

	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 128-255 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

void base64_encode(const void *src, size_t src_size, buffer_t *dest)
{
	const size_t res_size = MAX_BASE64_ENCODED_SIZE(src_size);
	unsigned char *start = buffer_append_space_unsafe(dest, res_size);
	unsigned char *ptr = start;
	const unsigned char *src_c = src;
	size_t src_pos;

	for (src_pos = 0; src_size - src_pos > 2; src_pos += 3, ptr += 4) {
		ptr[0] = b64enc[src_c[src_pos] >> 2];
		ptr[1] = b64enc[((src_c[src_pos] & 0x03) << 4) |
				(src_c[src_pos+1] >> 4)];
		ptr[2] = b64enc[((src_c[src_pos+1] & 0x0f) << 2) |
				((src_c[src_pos+2] & 0xc0) >> 6)];
		ptr[3] = b64enc[src_c[src_pos+2] & 0x3f];
	}

	i_assert(ptr <= start + res_size);

	switch (src_size - src_pos) {
	case 0:
		break;
	case 1:
		ptr[0] = b64enc[src_c[src_pos] >> 2];
		ptr[1] = b64enc[(src_c[src_pos] & 0x03) << 4];
		ptr[2] = '=';
		ptr[3] = '=';
		break;
	case 2:
		ptr[0] = b64enc[src_c[src_pos] >> 2];
		ptr[1] = b64enc[((src_c[src_pos] & 0x03) << 4) |
				(src_c[src_pos+1] >> 4)];
		ptr[2] = b64enc[((src_c[src_pos+1] & 0x0f) << 2)];
		ptr[3] = '=';
		break;
	default:
		i_unreached();
	}
}

#define IS_EMPTY(c) \
	((c) == '\n' || (c) == '\r' || (c) == ' ' || (c) == '\t')

int base64_decode(const void *src, size_t src_size,
		  size_t *src_pos_r, buffer_t *dest)
{
	const unsigned char *src_c = src;
	size_t src_pos;
	unsigned char input[4], output[3];
	int ret = 1;

	for (src_pos = 0; src_pos+3 < src_size; ) {
		input[0] = b64dec[src_c[src_pos]];
		if (input[0] == 0xff) {
			if (unlikely(!IS_EMPTY(src_c[src_pos]))) {
				ret = -1;
				break;
			}
			src_pos++;
			continue;
		}

		input[1] = b64dec[src_c[src_pos+1]];
		if (unlikely(input[1] == 0xff)) {
			ret = -1;
			break;
		}
		output[0] = (input[0] << 2) | (input[1] >> 4);

		input[2] = b64dec[src_c[src_pos+2]];
		if (input[2] == 0xff) {
			if (unlikely(src_c[src_pos+2] != '=' ||
				     src_c[src_pos+3] != '=')) {
				ret = -1;
				break;
			}
			buffer_append(dest, output, 1);
			ret = 0;
			src_pos += 4;
			break;
		}

		output[1] = (input[1] << 4) | (input[2] >> 2);
		input[3] = b64dec[src_c[src_pos+3]];
		if (input[3] == 0xff) {
			if (unlikely(src_c[src_pos+3] != '=')) {
				ret = -1;
				break;
			}
			buffer_append(dest, output, 2);
			ret = 0;
			src_pos += 4;
			break;
		}

		output[2] = ((input[2] << 6) & 0xc0) | input[3];
		buffer_append(dest, output, 3);
		src_pos += 4;
	}

	for (; src_pos < src_size; src_pos++) {
		if (!IS_EMPTY(src_c[src_pos]))
			break;
	}

	if (src_pos_r != NULL)
		*src_pos_r = src_pos;

	return ret;
}

buffer_t *t_base64_decode_str(const char *str)
{
	buffer_t *buf;
	size_t len = strlen(str);

	buf = buffer_create_dynamic(pool_datastack_create(),
				    MAX_BASE64_DECODED_SIZE(len));
	(void)base64_decode(str, len, NULL, buf);
	return buf;
}

bool base64_is_valid_char(char c)
{
	return b64dec[(uint8_t)c] != 0xff;
}
