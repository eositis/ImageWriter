/*
 * Serial port stub for Windows
 * Full implementation would use CreateFile, GetCommState, SetCommState
 * For now, list and open print helpful messages
 */

#include "serial.h"
#include <stdio.h>
#include <stdlib.h>

int serial_get_port_list(const char *paths[], int max_paths)
{
	(void)paths;
	(void)max_paths;
	return 0;
}

int serial_list_ports(void)
{
	printf("Serial port listing on Windows requires enumeration of COM ports.\n");
	printf("Typically: COM3, COM4, etc. for USB-serial adapters.\n");
	printf("Use: -s COM3 (or whatever port your adapter uses)\n");
	printf("\nFull Windows serial support not yet implemented.\n");
	printf("Check Device Manager for your COM port number.\n");
	return 0;
}

serial_port_t *serial_open(const char *path, int baud)
{
	(void)baud;
	fprintf(stderr, "Serial port support on Windows is not yet implemented.\n");
	fprintf(stderr, "You specified: %s\n", path ? path : "(null)");
	fprintf(stderr, "Please use file input mode: ./imagewriter -o bmp printer_dump.bin\n");
	return NULL;
}

int serial_read(serial_port_t *port, unsigned char *buf, int len)
{
	(void)port;
	(void)buf;
	(void)len;
	return -1;
}

int serial_is_open(serial_port_t *port)
{
	return port != NULL ? 0 : 0;
}

void serial_close(serial_port_t *port)
{
	(void)port;
}
