#ifndef _SUSPEND_USERUI_H_
#define _SUSPEND_USERUI_H_

#define SUSPEND_USERUI_INTERFACE_VERSION 4

enum {
	USERUI_MSG_BASE = 0x10,

	/* Userspace -> Kernel */
	USERUI_MSG_READY = 0x10,
	USERUI_MSG_ABORT = 0x11,
	USERUI_MSG_SET_STATE = 0x12,
	USERUI_MSG_GET_STATE = 0x13,
	USERUI_MSG_NOFREEZE_ME = 0x16,
	USERUI_MSG_SET_PROGRESS_GRANULARITY = 0x17,

	/* Kernel -> Userspace */
	USERUI_MSG_MESSAGE = 0x21,
	USERUI_MSG_PROGRESS = 0x22,
	USERUI_MSG_CLEANUP = 0x24,
	USERUI_MSG_REDRAW = 0x25,
	USERUI_MSG_KEYPRESS = 0x26,
	USERUI_MSG_NOFREEZE_ACK = 0x27,

	USERUI_MSG_MAX,
};

struct userui_msg_params {
	unsigned long a, b, c, d;
	char text[80];
};

#endif /* _SUSPEND_USERUI_H_ */
