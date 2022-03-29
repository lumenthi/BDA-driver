#ifndef SERVER_H
# define SERVER_H

#include <pthread.h>
#include <linux/joystick.h>

/* Joystick API

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

#define TRIGGER_LEFT		0x04
#define TRIGGER_RIGHT		0x05
#define TRIGGER2_LEFT		0x06
#define TRIGGER2_RIGHT		0x07

#define LESS			0x08
#define PLUS			0x09
#define HOME			0x0A
#define SCREENSHOT		0x11

#define DPAD_UP			0x0D
#define DPAD_RIGHT		0x0E
#define DPAD_BOTTOM		0x0F
#define DPAD_LEFT		0x10

#define BUTTON_JOYSTICK_LEFT	0x0B
#define BUTTON_JOYSTICK_RIGHT	0x0C

#define X_JOYSTICK_LEFT		0x00
#define Y_JOYSTICK_LEFT		0x01
#define X_JOYSTICK_RIGHT	0x02
#define Y_JOYSTICK_RIGHT	0x03

typedef struct js_event		t_controller;

pthread_t 			new_controller_server(void *callback_function);
void				stop_controller_server(pthread_t thread_pid);

#endif
