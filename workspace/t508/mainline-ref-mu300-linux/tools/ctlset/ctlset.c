/* ctlset: write an integer ALSA control by name with raw ioctls, without the client-side clamp amixer applies.
 * The VBC profile selects are declared with max 0x0fffffff but carry the mode in bits 24-31, so amixer turns
 * every mode >= 16 into 0x0fffffff; the driver itself takes the full 32-bit value.
 *   ctlset CARD NAME VALUE        e.g. ctlset 0 'DSP VBC Profile Select' 0x1c5e0000                        */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sound/asound.h>

int main(int argc, char **argv)
{
	if (argc < 4) { fprintf(stderr, "usage: %s CARD NAME VALUE\n", argv[0]); return 2; }
	char dev[64]; snprintf(dev, sizeof dev, "/dev/snd/controlC%s", argv[1]);
	int fd = open(dev, O_RDWR);
	if (fd < 0) { perror(dev); return 1; }
	struct snd_ctl_elem_value v; memset(&v, 0, sizeof v);
	v.id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	strncpy((char *)v.id.name, argv[2], sizeof v.id.name - 1);
	v.value.integer.value[0] = (long)strtoul(argv[3], NULL, 0);
	if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_WRITE, &v) < 0) { perror("ELEM_WRITE"); return 1; }
	memset(&v.value, 0, sizeof v.value);
	if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_READ, &v) < 0) { perror("ELEM_READ"); return 1; }
	printf("%s = %#lx\n", argv[2], (unsigned long)v.value.integer.value[0]);
	return 0;
}
