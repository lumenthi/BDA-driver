#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include "controller.h"

#define RED "\033[38;5;196m"
#define GREEN "\033[38;5;85m"
#define BLUEGREEN "\033[38;5;123m"
#define ORANGE "\033[38;5;214m"
#define YELLOW "\033[38;5;227m"
#define BLUE "\033[38;5;159m"

#define DEFAULT "\033[0m"

volatile static int stop = 0;

typedef struct 	s_joystick {
	int 	x;
	int 	y;
}		t_joystick;

t_joystick joystick_left;
t_joystick joystick_right;

static char *button_name[0x12] = {
	"A", "B", "X", "Y",
	"LB", "RB", "LT", "RT",
	"LESS", "PLUS", "HOME",
	"JOYSTICK LEFT", "JOYSTICK RIGHT",
	"DPAD-UP", "DPAD-RIGHT", "DPAD-DOWN", "DPAD-LEFT",
	"SCREENSHOT"
};

static void	sig_handler(int signal)
{
	printf("\nCaught signal, interrupting execution.\n");
	stop = 1;
}

static void	button_handler(t_controller controller)
{
	printf("%s%s%s %s\n",
		RED,
		button_name[controller.number],
		DEFAULT,
		controller.value == PRESSED ? "Pressed" : "Released"
	);
}

static void	axis_handler(t_controller controller)
{
	if (controller.number == X_JOYSTICK_LEFT)
		joystick_left.x = controller.value;
	else if (controller.number == Y_JOYSTICK_LEFT)
		joystick_left.y = controller.value;
	else if (controller.number == X_JOYSTICK_RIGHT)
		joystick_right.x = controller.value;
	else
		joystick_right.y = controller.value;

	printf("%sJoystick left: %s%d, %d%s | ",
		YELLOW,
		BLUE,
		joystick_left.x,
		joystick_left.y,
		DEFAULT
	);
	printf("%sJoystick right: %s%d, %d%s\n",
		YELLOW,
		BLUE,
		joystick_right.x,
		joystick_right.y,
		DEFAULT
	);
}

static void	init_handler(t_controller controller)
{
	if (controller.type == EVENT_INIT_BUTTONS) {
		printf("Initializing %s%s%s button\n",
			BLUEGREEN,
			button_name[controller.number],
			DEFAULT);
	}
	else if (controller.type == EVENT_INIT_JOYSTICKS) {
		if (controller.number == X_JOYSTICK_LEFT)
			printf("Initializing %s%s%s axis\n",
				GREEN,
				"JOYSTICK LEFT X",
				DEFAULT);
		else if (controller.number == Y_JOYSTICK_LEFT)
			printf("Initializing %s%s%s axis\n",
				GREEN,
				"JOYSTICK LEFT Y",
				DEFAULT);
		else if (controller.number == X_JOYSTICK_RIGHT)
			printf("Initializing %s%s%s axis\n",
				GREEN,
				"JOYSTICK RIGHT X",
				DEFAULT);
		else if (controller.number == Y_JOYSTICK_RIGHT)
			printf("Initializing %s%s%s axis\n",
				GREEN,
				"JOYSTICK RIGHT Y",
				DEFAULT);
	}
}

static void	handle_event(t_controller controller)
{
	if (controller.type == EVENT_BUTTON)
		button_handler(controller);
	else if (controller.type == EVENT_AXIS)
		axis_handler(controller);
	else
		init_handler(controller);
}

int 	main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	unsigned long int server_tid;

	signal(SIGINT, sig_handler);
	server_tid = new_controller_server(handle_event);
	printf("%s%s%s%s%s%s%s",
		"============================================\n",
		"Testing controller, press ", RED, "CTRL+C", DEFAULT, " to stop...\n"
		"============================================\n");

	while(!stop);
	stop_controller_server(server_tid);

	return 0;
}
