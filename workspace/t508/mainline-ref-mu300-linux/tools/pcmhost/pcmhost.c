/* pcmhost: open an ALSA PCM and START it without writing or reading data - what a hostless front end
 * (a DSP scene such as FE_VOICE) needs, and what aplay/arecord cannot do. Raw ioctls, no alsa-lib.
 *   pcmhost DEVICE RATE CHANNELS SECONDS      e.g. pcmhost /dev/snd/pcmC1D5p 8000 1 60            */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sound/asound.h>

static void mask_set(struct snd_pcm_hw_params *p, int par, unsigned int v)
{
	struct snd_mask *m = &p->masks[par - SNDRV_PCM_HW_PARAM_FIRST_MASK];
	memset(m, 0, sizeof(*m));
	m->bits[v >> 5] |= 1u << (v & 31);
}
static void iv_set(struct snd_pcm_hw_params *p, int par, unsigned int lo, unsigned int hi)
{
	struct snd_interval *i = &p->intervals[par - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
	memset(i, 0, sizeof(*i));
	i->min = lo; i->max = hi; i->integer = 1;
}

int main(int argc, char **argv)
{
	if (argc < 5) { fprintf(stderr, "usage: %s DEVICE RATE CHANNELS SECONDS\n", argv[0]); return 2; }
	unsigned int rate = atoi(argv[2]), ch = atoi(argv[3]); int secs = atoi(argv[4]);
	int fd = open(argv[1], O_RDWR);
	if (fd < 0) { perror("open"); return 1; }
	struct snd_pcm_hw_params hp; memset(&hp, 0, sizeof(hp));
	for (int i = 0; i <= SNDRV_PCM_HW_PARAM_LAST_MASK - SNDRV_PCM_HW_PARAM_FIRST_MASK; i++)
		memset(&hp.masks[i], 0xff, sizeof(hp.masks[i]));
	for (int i = 0; i <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL; i++)
		{ hp.intervals[i].min = 0; hp.intervals[i].max = ~0u; }
	hp.rmask = ~0u;
	mask_set(&hp, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
	mask_set(&hp, SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FORMAT_S16_LE);
	mask_set(&hp, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
	iv_set(&hp, SNDRV_PCM_HW_PARAM_CHANNELS, ch, ch);
	iv_set(&hp, SNDRV_PCM_HW_PARAM_RATE, rate, rate);
	if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &hp) < 0) { perror("HW_PARAMS"); return 1; }
	unsigned long buf = hp.intervals[SNDRV_PCM_HW_PARAM_BUFFER_SIZE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].min;
	unsigned long per = hp.intervals[SNDRV_PCM_HW_PARAM_PERIOD_SIZE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL].min;
	struct snd_pcm_sw_params sp; memset(&sp, 0, sizeof(sp));
	sp.tstamp_mode = SNDRV_PCM_TSTAMP_NONE; sp.period_step = 1; sp.avail_min = 1;
	sp.start_threshold = 1; sp.silence_threshold = 0; sp.silence_size = 0;
	/* never stop on an xrun: a hostless scene has nobody filling the buffer */
	/* stop_threshold at or past the kernel's boundary is what lets an empty playback buffer START at all
	 * (snd_pcm_playback_data) and keeps it from stopping on an xrun afterwards */
	sp.stop_threshold = ~0UL;
	if (ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sp) < 0) perror("SW_PARAMS (continuing)");
	if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) { perror("PREPARE"); return 1; }
	if (ioctl(fd, SNDRV_PCM_IOCTL_START) < 0) { perror("START"); return 1; }
	printf("started %s rate=%u ch=%u buffer=%lu period=%lu\n", argv[1], rate, ch, buf, per); fflush(stdout);
	for (int t = 0; t < secs; t++) {
		struct snd_pcm_status st; memset(&st, 0, sizeof(st));
		if (ioctl(fd, SNDRV_PCM_IOCTL_STATUS, &st) == 0)
			{ printf("t=%ds state=%d hw_ptr=%lu\n", t, st.state, (unsigned long)st.hw_ptr); fflush(stdout); }
		sleep(1);
	}
	ioctl(fd, SNDRV_PCM_IOCTL_DROP); close(fd); return 0;
}
