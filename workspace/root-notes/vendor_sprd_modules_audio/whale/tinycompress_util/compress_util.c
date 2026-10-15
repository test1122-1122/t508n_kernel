/*copyright (C) 2010 The Android Open Source Project
* Copyright (C) 2012-2015, The Linux Foundation. All rights reserved.
*
* Not a Contribution, Apache license notifications and license are retained
* for attribution purposes only.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#define LOG_TAG "audio_hwcompress_util"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <limits.h>

#include <linux/types.h>
#include <linux/ioctl.h>
#define __force
#define __user
#include <sound/asound.h>
#include "sound/compress_params.h"
#include "sound/compress_offload.h"
#include "compress_util.h"

/**
macro define
**/
#define COMPR_ERR_MAX 128
/* Default maximum time we will wait in a poll() - 20 seconds */
#define DEFAULT_MAX_POLL_WAIT_MS    20000


#define SNDRV_COMPRESS_SAMPLERATE  100
#define SNDRV_COMPRESS_BITRATE     101
#define SNDRV_COMPRESS_CHANNEL     102



/**
extern function
**/
extern int is_compress_ready(struct compress *compress);

/*Attention : the struct compress with tinycompress need keep same*/
struct compress {
    int fd;
    unsigned int flags;
    char error[COMPR_ERR_MAX];
    struct compr_config *config;
    int running;
    int max_poll_wait_ms;
    int nonblocking;
    unsigned int gapless_metadata;
    unsigned int next_track;
};

static struct compress bad_compress = {
	.fd = -1,
};


static int oops(struct compress *compress, int e, const char *fmt, ...)
{
    va_list ap;
    int sz;

    va_start(ap, fmt);
    vsnprintf(compress->error, COMPR_ERR_MAX, fmt, ap);
    va_end(ap);
    sz = strlen(compress->error);

    snprintf(compress->error + sz, COMPR_ERR_MAX - sz,
        ": %s", strerror(e));
    errno = e;

    return -1;
}

static inline void
fill_compress_params(struct compr_config *config, struct snd_compr_params *params)
{
	params->buffer.fragment_size = config->fragment_size;
	params->buffer.fragments = config->fragments;
	memcpy(&params->codec, config->codec, sizeof(params->codec));
}


struct compress *compress_open_sprd(unsigned int card, unsigned int device,
		unsigned int flags, struct compr_config *config)
{
	struct compress *compress;
	struct snd_compr_caps caps;
	char fn[256];

	if (!config) {
		oops(&bad_compress, EINVAL, "passed bad config");
		return &bad_compress;
	}

	compress = calloc(1, sizeof(struct compress));
	if (!compress) {
		oops(&bad_compress, errno, "cannot allocate compress object");
		return &bad_compress;
	}

	compress->next_track = 0;
	compress->gapless_metadata = 0;
	compress->config = calloc(1, sizeof(*config));
	if (!compress->config)
		goto input_fail;

	snprintf(fn, sizeof(fn), "/dev/snd/comprC%uD%u", card, device);

	compress->max_poll_wait_ms = DEFAULT_MAX_POLL_WAIT_MS;

	compress->flags = flags;
	if (!((flags & COMPRESS_OUT) || (flags & COMPRESS_IN))) {
		oops(&bad_compress, EINVAL, "can't deduce device direction from given flags");
		goto config_fail;
	}

	if (flags & COMPRESS_OUT) {
		compress->fd = open(fn, O_RDONLY);
	} else {
		compress->fd = open(fn, O_WRONLY);
	}
	if (compress->fd < 0) {
		oops(&bad_compress, errno, "cannot open device '%s'", fn);
		goto config_fail;
	}

	if (ioctl(compress->fd, SNDRV_COMPRESS_GET_CAPS, &caps)) {
		oops(compress, errno, "cannot get device caps");
		goto codec_fail;
	}

	/* If caller passed "don't care" fill in default values */
	if ((config->fragment_size == 0) || (config->fragments == 0)) {
		config->fragment_size = caps.min_fragment_size;
		config->fragments = caps.max_fragments;
	}

#if 0
	/* FIXME need to turn this On when DSP supports
	 * and treat in no support case
	 */
	if (_is_codec_supported(compress, config, &caps) == false) {
		oops(compress, errno, "codec not supported\n");
		goto codec_fail;
	}
#endif

	memcpy(compress->config, config, sizeof(*compress->config));

	return compress;

codec_fail:
	close(compress->fd);
	compress->fd = -1;
config_fail:
	free(compress->config);
input_fail:
	free(compress);
	return &bad_compress;
}


int compress_setparam(struct compress *compress)
{
	struct snd_compr_params params;

	fill_compress_params(compress->config, &params);

	if (ioctl(compress->fd, SNDRV_COMPRESS_SET_PARAMS, &params)) {
		return oops(&bad_compress, errno, "cannot set device");
	}
	return 0;
}


/*recomplete for compress_write*/
int offload_write(struct compress *compress, const void *buf, unsigned int size)
{
    if (NULL == compress || NULL == buf) {
        return -1;
    }

    struct snd_compr_avail avail;
    struct pollfd fds;
    int to_write = 0;   /* zero indicates we haven't written yet */
    int written, total = 0, ret;
    const char* cbuf = buf;
    const unsigned int frag_size = compress->config->fragment_size;

    if (!(compress->flags & COMPRESS_IN))
        return oops(compress, EINVAL, "Invalid flag set");
    if (!is_compress_ready(compress))
        return oops(compress, ENODEV, "device not ready");
    fds.fd = compress->fd;
    fds.events = POLLOUT;

    /*TODO: treat auto start here first */
    while (size) {
        if (ioctl(compress->fd, SNDRV_COMPRESS_AVAIL, &avail))
            return oops(compress, errno, "cannot get avail");

        /* We can write if we have at least one fragment available
         * or there is enough space for all remaining data
         */
        if (compress->nonblocking) {
            if(avail.avail == 0)
                return total;
        } else if ((avail.avail < frag_size) && (avail.avail < size)) {
            if (compress->nonblocking)
                return total;

            ret = poll(&fds, 1, compress->max_poll_wait_ms);
            if (fds.revents & POLLERR) {
                return oops(compress, EIO, "poll returned error!");
            }
            /* A pause will cause -EBADFD or zero.
             * This is not an error, just stop writing */
            if ((ret == 0) || (ret == -EBADFD))
                break;
            if (ret < 0)
                return oops(compress, errno, "poll error");
            if (fds.revents & POLLOUT) {
                continue;
            }
        }
        /* write avail bytes */
        if (size > avail.avail)
            to_write =  avail.avail;
        else
            to_write = size;
        written = write(compress->fd, cbuf, to_write);
        /* If play was paused the write returns -EBADFD */
        if (written == -EBADFD)
            break;
        if (written < 0)
            return oops(compress, errno, "write failed!");

        size -= written;
        cbuf += written;
        total += written;
    }
    return total;
}

int compress_set_metadata(struct compress *compress,
    struct compr_mdata *mdata)
{
    struct snd_compr_metadata metadata;

    if(mdata->bitrate) {
        metadata.key = SNDRV_COMPRESS_BITRATE;
        metadata.value[0] = mdata->bitrate;
        if (ioctl(compress->fd, SNDRV_COMPRESS_SET_METADATA, &metadata))
            return oops(compress, errno, "can't set metadata for stream\n");
    }
    if(mdata->samplerate) {
        metadata.key = SNDRV_COMPRESS_SAMPLERATE;
        metadata.value[0] = mdata->samplerate;
        if (ioctl(compress->fd, SNDRV_COMPRESS_SET_METADATA, &metadata))
            return oops(compress, errno, "can't set metadata for stream\n");
    }
    if(mdata->channel) {
        metadata.key = SNDRV_COMPRESS_CHANNEL;
        metadata.value[0] = mdata->channel;
        if (ioctl(compress->fd, SNDRV_COMPRESS_SET_METADATA, &metadata))
            return oops(compress, errno, "can't set metadata for stream\n");
    }
    return 0;
}
