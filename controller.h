#ifndef SERVER_H
# define SERVER_H

#include <pthread.h>
#include <linux/joystick.h>

/*

struct js_event {
	__u32 time;     		* event timestamp in milliseconds *
	__s16 value;    		* value *
	__u8 type;      		* event type *
	__u8 number;    		* axis/button number *
};

#define JS_EVENT_BUTTON     0x01    	* button pressed/released *
#define JS_EVENT_AXIS       0x02    	* joystick moved *
#define JS_EVENT_INIT       0x80    	* initial state of device *

*/

/* Controller input device path */
#define DEVICE_PATH 		"/dev/input/js0"

/* Value */
#define RELEASED		0x00
#define PRESSED			0x01

/* Type */
#define EVENT_BUTTON 		0x01
#define EVENT_AXIS		0x02
#define EVENT_INIT_BUTTONS	0x81
#define EVENT_INIT_JOYSTICKS	0x82

/* Number */
#define BUTTON_A 		0x00
#define BUTTON_B 		0x01
#define BUTTON_X 		0x02
#define BUTTON_Y 		0x03

typedef struct js_event		t_controller;

pthread_t 			new_controller_server(void *callback_function);
void				stop_controller_server(pthread_t thread_pid);

#endif
