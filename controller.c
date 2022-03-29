#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#include "controller.h"

static int fd = -1;

void	stop_controller_server(pthread_t thread_id)
{
	pthread_cancel(thread_id);
	if (fd != -1 && close(fd) < 0)
		fprintf(stderr, "%s", "Close failed.\n");
}

static void *start_controller_server(void *args)
{
	struct js_event event;

	void (*function_pointer)() = (void*)args;
	
	fd = open(DEVICE_PATH, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "%s", "Can't open the input device.\n");
		return NULL;
	}
	while (read(fd, &event, sizeof(struct js_event)))
		function_pointer(event);
}

pthread_t new_controller_server(void *callback_function)
{
	pthread_t thread_id;
	int retval;

	retval = pthread_create(&thread_id, NULL, start_controller_server, (void*)callback_function);
	return thread_id;
}
