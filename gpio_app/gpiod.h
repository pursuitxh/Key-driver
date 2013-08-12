/*
 * @file: gpiod.h
 * 
 * Created on: Nov 6, 2012
 */

#ifndef _GPIOD_H_
#define _GPIOD_H_

static const char version[] = "gpiod Tue Nov 6 16:06:35 CST 2012";

#define GPIOD_FALSE 		0
#define GPIOD_TRUE  		1

#define DEFAULT_SET_KEY		5
#define	DC_DETECT_KEY		6
#define POWER_KEY		8

typedef enum key_result {
	KEY_RESULT_SHORT = 0,
	KEY_RESULT_LONG,
	KEY_RESULT_FACTORY
}key_result_e;

typedef struct key_value {
	volatile int value;
	key_result_e result;
}key_value_t;

extern int gpio_use_syslog;

#define err(fmt, args...)	do { \
	if (gpio_use_syslog) { \
		syslog(LOG_ERR, "gpiod err: %13s:%4d (%-12s) " fmt "\n", \
			__FILE__, __LINE__, __FUNCTION__,  ##args); \
	} \
} while (0)

#define dbg(fmt, args...)	do { \
	if (gpio_use_syslog) { \
		syslog(LOG_DEBUG, fmt, ##args); \
	} \
} while (0)

#define BUG()	do { err("sorry, it's a bug"); abort(); } while (0)

#endif /* _GPIOD_H_ */
