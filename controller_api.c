#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include "controller.h"

volatile static int stop = 0;
void (*init_functions[2]) (uint8_t button);

static void	sig_handler(int signal)
{
	printf("\nCaught signal, interrupting execution.\n");
	stop = 1;
}

static void	handle_event(t_controller controller)
{
	printf("\n======EVENT======\n");

	printf("time: 0x%x\nvalue: 0x%x\ntype: 0x%x\nnumber: 0x%x\n",
		controller.time, controller.value, controller.type, controller.number);

	printf("=================\n");
}

int 	main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	unsigned long int server_tid;
	stop = 0;

	signal(SIGINT, sig_handler);

	server_tid = new_controller_server(handle_event);

	while(!stop);

	stop_controller_server(server_tid);

	return 0;
}
