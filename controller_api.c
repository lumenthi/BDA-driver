#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#define MAXPACKETSIZE 64 // 64 bytes -> lsusb.txt
#define DEVICE_PATH "/dev/usb/skel0" // TODO: device identification

static volatile int stop = 0;

void	sig_handler(int signal)
{
	stop = 1;
}

int 	main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	int fd;
	uint64_t nb;

	char buf[MAXPACKETSIZE];
	char oldbuf[MAXPACKETSIZE];

	signal(SIGINT, sig_handler);

	fd = open(DEVICE_PATH, O_RDONLY | O_NDELAY);
	if (fd == -1) {
		fprintf(stderr, "%s", "Can't open the device handle.\n");
		return 1;
	}
	while (!stop && (nb = read(fd, buf, MAXPACKETSIZE))) {
		if (*(uint64_t*)buf != *(uint64_t*)oldbuf) {
			printf("0x%lx\n", *(uint64_t*)buf);
			*(uint64_t*)oldbuf = *(uint64_t*)buf;
		}
	}
	if (close(fd) < 0)
		printf("Close failed.\n");
	printf("\nCaught signal, interrupting execution.\n");
	return 0;
}
