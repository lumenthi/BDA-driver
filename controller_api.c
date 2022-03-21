#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#define MAXPACKETSIZE 64 // 64 bytes -> lsusb.txt
#define DEVICE_PATH "/dev/usb/skel0" // TODO: device identification

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

static volatile int stop = 0;

void 	printBits(unsigned int num)
{
	for(int bit=0;bit<(sizeof(unsigned int) * 8); bit++) {
		printf("%i", num & 0x01);
		num = num >> 1;
	}
	printf("\n");
}

void	sig_handler(int signal)
{
	stop = 1;
}

void	handle_event(uint64_t data)
{
	// int pos = 0;
	printf("data: 0x%lx\n", data);
	// printf("bit %d is %s\n", pos, CHECK_BIT(data, pos) ? "set":"not set");
	printBits(data);
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
			handle_event(*(uint64_t*)buf);
			*(uint64_t*)oldbuf = *(uint64_t*)buf;
		}
	}
	if (close(fd) < 0)
		fprintf(stderr, "%s", "Close failed.\n");
	printf("\nCaught signal, interrupting execution.\n");
	return 0;
}
