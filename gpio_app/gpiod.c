/*
 * @file: gpiod.c
 * 
 * Created on: Nov 6, 2012
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include "gpiod.h"

int g_gpio_fd;
FILE *g_fp;
static int gpio_running = GPIOD_FALSE;

int gpio_use_syslog = 0;

static void gpio_status_init()
{
	/* set GPIO4, GPIO1 output */
	system("echo 4 > /sys/class/gpio/export");
	system("echo low > /sys/class/gpio/gpio4/direction");
	
	system("echo 1 > /sys/class/gpio/export");
	system("echo low > /sys/class/gpio/gpio1/direction");
}

static void gpio_status_free()
{
	/* free GPIO4, GPIO1 */
	system("echo 4 > /sys/class/gpio/unexport");
	system("echo 1 > /sys/class/gpio/unexport");
}

static void set_wifi_led_off()
{
	dbg("Set wifi led off, indicate user can loosen the power key!\n");
	system("echo 0 > /sys/class/gpio/gpio4/value");
	system("echo 0 > /sys/class/gpio/gpio1/value");
}

static void set_5g_led()
{
	dbg("Enter set_5g_led function!!!\n");
	/* set GPIO4 to 1, GOIO1 to 1, make LED5 display ORANGE */
	system("echo 1 > /sys/class/gpio/gpio4/value");
	system("echo 1 > /sys/class/gpio/gpio1/value");
}

static void set_24g_led()
{
	dbg("Enter set_24g_led function!!!\n");
	/* set GPIO4 to 1, GOIO1 to 0, make LED5 display GREEN */
	system("echo 1 > /sys/class/gpio/gpio4/value");
	system("echo 0 > /sys/class/gpio/gpio1/value");
}

static void wifi_status_check()
{
	FILE *fp;
	char band[16];

	memset(band, '\0', sizeof(band));
	fp = popen("sysconf get wl_band", "r");
	fread(band, sizeof(char), sizeof(band), fp);

	if (band[1] == '5') {
		set_5g_led();
	} else {
		set_24g_led();
	}

	pclose(fp);
}

static void set_wifi_5g()
{	
	/* set wifi to 5GHz */
	system("echo '{\"method\":\"pref-set\",\"arguments\":{\"confs\":{\"wl_"
			"band\":\"5GHz\"}}}' | /web/rpc.cgi >/dev/null 2>&1");
	
	/* restart wifi */
	system("echo '{\"method\":\"revalidate\",\"arguments\":{\"wl\":{\"act"
			"ion\":\"restart\"}}}' | /web/rpc.cgi >/dev/null 2>&1");
}

static void set_wifi_24g()
{	
	/* set wifi to 2.4GHz */
	system("echo '{\"method\":\"pref-set\",\"arguments\":{\"confs\":{\"wl_"
			"band\":\"2.4GHz\"}}}' | /web/rpc.cgi >/dev/null 2>&1");
	
	/* restart wifi */
	system("echo '{\"method\":\"revalidate\",\"arguments\":{\"wl\":{\"act"
			"ion\":\"restart\"}}}' | /web/rpc.cgi >/dev/null 2>&1");
}

static void shutdown_system_power()
{
	dbg("Enter shutdown_system_ power function!!!\n");
	/*
	 * set GPIO7 to 0, shut down the system power
	 * writing "out" to direction defaults to initializing the value as low.
	 */
	system("echo 7 > /sys/class/gpio/export");
	system("echo high > /sys/class/gpio/gpio7/direction");
	system("echo 0 > /sys/class/gpio/gpio7/value");		
	system("echo 7 > /sys/class/gpio/unexport");

	set_wifi_led_off();
}

static void power_led_indicate()
{
	/* TODO: Not used now */
	dbg("Enter power_led_indicate function!!!\n");
}

static void set_factory_default()
{
	dbg("Enter set_factory_default function!!!\n");

	int i;
	system("rm -rf /mnt/rwdata/.firstrun");
	for (i = 0; i < 2; i++) {
		set_5g_led(); 		/* leds on */
		sleep(1);
		set_wifi_led_off();	/* leds off */
		sleep(1);
	}
	system ("reboot");
}

static void handle_power_key_press(int mode)
{
	char band[16];

	memset(band, '\0', sizeof(band));
	
	if (mode == 0) {
		dbg("Power key short press...\n");
		g_fp = popen("sysconf get wl_band", "r");
		memset(band, '\0', sizeof(band));
		fread(band, sizeof(char), sizeof(band), g_fp);	
		
		if (band[1] == '5') {
			set_wifi_24g();
			set_24g_led();
		} else {
			set_wifi_5g();
			set_5g_led();
		}
	} else if (mode == 1) {
			dbg("Power key long press...\n");
			shutdown_system_power();
	} else if (mode == 2) {
			dbg("Set factory default...\n");
			set_factory_default();
	}
}

static void handle_default_key_press()
{
	dbg("default setting key is press...\n");

	system("rm -rf /mnt/rwdata/.firstrun");
	system ("reboot");
}

static void handle_dc_key_press()
{
	dbg("dc detect key is press...\n");
	power_led_indicate();
}

static void signal_handler(int signal)
{
	dbg("signal catched, code %d\n", signal);

	key_value_t key;

	if (SIGIO == signal) {
		read(g_gpio_fd, &key, sizeof(key));

		//dbg("Key%d is %d pressed!\n", key.value, key.result);
		if (key.value == POWER_KEY) {
			handle_power_key_press(key.result);
		} else if (key.value == DEFAULT_SET_KEY) {
			handle_default_key_press();
		} else if (key.value == DC_DETECT_KEY) {
			handle_dc_key_press();
		}
	} else if (SIGINT == signal || SIGTERM == signal) {
		if (gpio_running) {
			dbg("gpiod shut down...\n");
			gpio_running = GPIOD_FALSE;
			gpio_status_free();
			pclose(g_fp);
			close(g_gpio_fd);
		}
	}
}

static void set_signal(void)
{
	signal(SIGCHLD,SIG_IGN);

	struct sigaction act;

	bzero(&act, sizeof(act));
	act.sa_handler = signal_handler;
	sigemptyset(&act.sa_mask);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGIO, &act, NULL);
}

static void do_standalone_mode(char daemonize)
{
	int oflags;

	if (daemonize) {
		if (daemon(0,0) < 0)
			err("daemonizing failed...\n");
	}

	set_signal();

	gpio_status_init();
	wifi_status_check();

	g_gpio_fd = open("/dev/KEY_LB", 0);
	if (g_gpio_fd < 0) {
		err("open gpio device error!\n");
		exit(1);
	}

	fcntl(g_gpio_fd, F_SETOWN, getpid());
	oflags = fcntl(g_gpio_fd, F_GETFL);
	fcntl(g_gpio_fd, F_SETFL, oflags | FASYNC);

	dbg("gpiod start (%s)\n", version);
	gpio_running = GPIOD_TRUE;

	while (gpio_running) {
		sleep(1000);
	}

	return;
}

static const char help_message[] = "\
Usage: gpiod [options]				\n\
	-D, --daemon				\n\
		Run as a daemon process.	\n\
						\n\
	-d, --debug				\n\
		Print debugging information.	\n\
						\n\
	-v, --version				\n\
		Show version.			\n\
						\n\
	-h, --help 				\n\
		Print this help.		\n";

static void show_help(void)
{
	printf("%s", help_message);
}

static const struct option longopts[] = {
	{"daemon",	no_argument,	NULL, 'D'},
	{"debug",	no_argument,	NULL, 'd'},
	{"version",	no_argument,	NULL, 'v'},
	{"help",	no_argument,	NULL, 'h'},
	{NULL,		0,		NULL,  0}
};

int main(int argc, char *argv[])
{
	char daemonize = GPIOD_FALSE;

	enum {
		cmd_standalone_mode = 1,
		cmd_help,
		cmd_version
	} cmd = cmd_standalone_mode;

	if (geteuid() != 0)
		printf("please running it with the root authority!\n");

	for (;;) {
		int c;
		int index = 0;

		c = getopt_long(argc, argv, "vhdD", longopts, &index);

		if (c == -1)
			break;

		switch (c) {
		case 'd':
			gpio_use_syslog = 1;
			continue;
		case 'v':
			cmd = cmd_version;
			break;
		case 'h':
			cmd = cmd_help;
			break;
		case 'D':
			daemonize = GPIOD_TRUE;
			break;
		case '?':
			show_help();
			exit(EXIT_FAILURE);
		default:
			err("getopt error\n");
			break;
		}
	}

	switch (cmd) {
	case cmd_standalone_mode:
		do_standalone_mode(daemonize);
		break;
	case cmd_version:
		printf("%s\n", version);
		break;
	case cmd_help:
		show_help();
		break;
	default:
		dbg("unknown cmd\n");
		show_help();
		break;
	}

	return 0;
}
