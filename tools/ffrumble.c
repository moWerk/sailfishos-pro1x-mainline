/* Minimal force-feedback test for a memless rumble device (bring-up tool).
 * ffrumble /dev/input/eventN [strength 0-65535] [ms]
 * Uploads one FF_RUMBLE effect, plays it once, prints the ids and errors. */
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define BIT_SET(a, b) (!!((a)[(b) / (8 * sizeof(unsigned long))] & (1UL << ((b) % (8 * sizeof(unsigned long))))))

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/input/event6";
	int strength = argc > 2 ? atoi(argv[2]) : 0xffff;
	int ms = argc > 3 ? atoi(argv[3]) : 500;
	int fd = open(dev, O_RDWR);
	if (fd < 0) { perror(dev); return 1; }

	unsigned long ffbits[(FF_MAX + 1) / (8 * sizeof(unsigned long)) + 1];
	memset(ffbits, 0, sizeof ffbits);
	if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof ffbits), ffbits) < 0) { perror("EVIOCGBIT"); return 1; }
	int n = 0;
	if (ioctl(fd, EVIOCGEFFECTS, &n) < 0) perror("EVIOCGEFFECTS");
	printf("rumble=%d periodic=%d slots=%d\n", BIT_SET(ffbits, FF_RUMBLE), BIT_SET(ffbits, FF_PERIODIC), n);

	struct ff_effect e;
	memset(&e, 0, sizeof e);
	e.type = FF_RUMBLE;
	e.id = -1;
	e.u.rumble.strong_magnitude = strength;
	e.u.rumble.weak_magnitude = strength / 2;
	e.replay.length = ms;
	if (ioctl(fd, EVIOCSFF, &e) < 0) { perror("EVIOCSFF"); return 1; }
	printf("effect id %d\n", e.id);

	struct input_event play = { .type = EV_FF, .code = e.id, .value = 1 };
	if (write(fd, &play, sizeof play) != sizeof play) { perror("play"); return 1; }
	usleep((ms + 200) * 1000);
	play.value = 0;
	write(fd, &play, sizeof play);
	ioctl(fd, EVIOCRMFF, e.id);
	puts("done");
	return 0;
}
