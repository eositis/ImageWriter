/*
 * Serial port implementation for macOS and Linux (POSIX)
 * Uses termios for configuration
 */

#include "serial.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/param.h>
#endif

struct serial_port {
	int fd;
	char path[256];
};

static speed_t baud_to_speed(int baud)
{
	switch (baud) {
	case 300:   return B300;
	case 1200:  return B1200;
	case 2400:  return B2400;
	case 4800:  return B4800;
	case 9600:  return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
	case 57600: return B57600;
	case 115200: return B115200;
	default:
		return B9600;
	}
}

static int is_serial_device(const char *name)
{
	/* macOS: cu.* preferred (no carrier detect wait), tty.* also works */
#ifdef __APPLE__
	if (strncmp(name, "cu.usb", 6) == 0) return 1;
	if (strncmp(name, "cu.SLAB", 7) == 0) return 1;  /* Prolific */
	if (strncmp(name, "cu.PL2303", 9) == 0) return 1;
	if (strncmp(name, "cu.ch34", 7) == 0) return 1;   /* CH340/CH341 */
	if (strncmp(name, "tty.usb", 7) == 0) return 1;
	if (strncmp(name, "tty.SLAB", 8) == 0) return 1;
	if (strncmp(name, "tty.PL2303", 10) == 0) return 1;
	/* Skip Bluetooth - typically not for direct serial */
	if (strstr(name, "Bluetooth") != NULL) return 0;
	if (strstr(name, "bluetooth") != NULL) return 0;
#else
	/* Linux: USB serial adapters */
	if (strncmp(name, "ttyUSB", 6) == 0) return 1;
	if (strncmp(name, "ttyACM", 6) == 0) return 1;
	/* Built-in serial */
	if (strncmp(name, "ttyS", 4) == 0) return 1;
#endif
	return 0;
}

#define MAX_PORT_LIST 32
static char s_port_list[MAX_PORT_LIST][64];
static int s_port_count;

int serial_get_port_list(const char *paths[], int max_paths)
{
	DIR *dir;
	struct dirent *ent;
	int count = 0;
	char path[512];

	s_port_count = 0;
	if (!paths || max_paths <= 0)
		return 0;

	dir = opendir("/dev");
	if (!dir)
		return -1;

	while ((ent = readdir(dir)) != NULL && count < max_paths) {
		if (!is_serial_device(ent->d_name))
			continue;

		snprintf(path, sizeof(path), "/dev/%s", ent->d_name);

		if (access(path, R_OK | W_OK) == 0) {
			strncpy(s_port_list[count], path, sizeof(s_port_list[0]) - 1);
			s_port_list[count][sizeof(s_port_list[0]) - 1] = '\0';
			paths[count] = s_port_list[count];
			count++;
		}
	}

	closedir(dir);
	s_port_count = count;
	return count;
}

int serial_list_ports(void)
{
	const char *paths[MAX_PORT_LIST];
	int count = serial_get_port_list(paths, MAX_PORT_LIST);

	printf("Available serial ports (ImageWriter-compatible):\n");
	printf("  Use -s /dev/<name> to connect\n\n");

	if (count < 0) {
		perror("Could not open /dev");
		return -1;
	}
	if (count == 0) {
		printf("  (none found - check USB connection and permissions)\n");
		printf("  On Linux you may need: sudo usermod -a -G dialout $USER\n");
	} else {
		for (int i = 0; i < count; i++)
			printf("  %s\n", paths[i]);
	}
	return count;
}

serial_port_t *serial_open(const char *path, int baud)
{
	struct serial_port *port;
	struct termios tty;
	speed_t speed = baud_to_speed(baud);

	if (!path || !*path) {
		fprintf(stderr, "serial_open: invalid path\n");
		return NULL;
	}

	port = (struct serial_port *)calloc(1, sizeof(*port));
	if (!port) {
		perror("serial_open: malloc");
		return NULL;
	}

	strncpy(port->path, path, sizeof(port->path) - 1);
	port->path[sizeof(port->path) - 1] = '\0';

	port->fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (port->fd < 0) {
		fprintf(stderr, "Could not open %s: %s\n", path, strerror(errno));
		free(port);
		return NULL;
	}

	if (tcgetattr(port->fd, &tty) != 0) {
		fprintf(stderr, "tcgetattr failed: %s\n", strerror(errno));
		close(port->fd);
		free(port);
		return NULL;
	}

	/* 8N1 - ImageWriter standard */
	cfmakeraw(&tty);
	tty.c_cflag &= ~(PARENB | PARODD);   /* No parity */
	tty.c_cflag &= ~CSTOPB;              /* 1 stop bit */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;                  /* 8 data bits */

	tty.c_cflag |= (CLOCAL | CREAD);     /* Ignore modem control, enable receiver */

	/* Set baud rate */
	cfsetispeed(&tty, speed);
	cfsetospeed(&tty, speed);

	/* Input: pass through raw bytes */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF);

	/* Output: raw */
	tty.c_oflag &= ~OPOST;

	/* Local: raw */
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* Timeout: return immediately with whatever is available (non-blocking read) */
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;

	if (tcsetattr(port->fd, TCSANOW, &tty) != 0) {
		fprintf(stderr, "tcsetattr failed: %s\n", strerror(errno));
		close(port->fd);
		free(port);
		return NULL;
	}

	/* Flush any pending data */
	tcflush(port->fd, TCIOFLUSH);

	printf("Serial port %s opened at %d baud (8N1)\n", path, baud);
	return port;
}

int serial_read(serial_port_t *port, unsigned char *buf, int len)
{
	ssize_t n;

	if (!port || port->fd < 0 || !buf || len <= 0)
		return -1;

	n = read(port->fd, buf, (size_t)len);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return 0;  /* Timeout / no data */
		return -1;
	}
	return (int)n;
}

int serial_is_open(serial_port_t *port)
{
	return port && port->fd >= 0;
}

void serial_close(serial_port_t *port)
{
	if (port) {
		if (port->fd >= 0) {
			close(port->fd);
			port->fd = -1;
		}
		free(port);
	}
}
