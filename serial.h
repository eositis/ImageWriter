/*
 * Serial port interface for ImageWriter emulator
 * Cross-platform API for USB-serial adapter discovery and configuration
 *
 * ImageWriter II defaults: 9600 baud, 8 data bits, no parity, 1 stop bit (8N1)
 * Hardware handshaking (DTR) is typical for Apple II connections
 */

#ifndef SERIAL_H
#define SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle for serial port */
typedef struct serial_port serial_port_t;

/* Baud rates supported by ImageWriter II: 300, 1200, 2400, 9600 (default), 19200 (LQ) */
#define SERIAL_BAUD_DEFAULT 9600

/*
 * List available serial ports. Prints to stdout, one port per line.
 * Returns number of ports found, or -1 on error.
 * On Unix: scans /dev/cu.*, /dev/tty.usb*, /dev/ttyUSB*, /dev/ttyACM*
 */
int serial_list_ports(void);

/*
 * Get list of available serial port paths for programmatic use.
 * Fills paths[0..n-1] with pointers to port path strings (valid until next call).
 * Returns number of ports found. max_paths limits the array size.
 */
int serial_get_port_list(const char *paths[], int max_paths);

/*
 * Open and configure serial port for ImageWriter emulation.
 * path: device path (e.g. "/dev/cu.usbserial-A50285BI" on macOS)
 * baud: baud rate (300, 1200, 2400, 9600, 19200)
 * Returns: serial handle, or NULL on failure
 */
serial_port_t *serial_open(const char *path, int baud);

/*
 * Read bytes from serial port. Blocks until data available or timeout.
 * Returns: number of bytes read, 0 on timeout, -1 on error
 */
int serial_read(serial_port_t *port, unsigned char *buf, int len);

/*
 * Check if port is still valid (e.g. not disconnected)
 */
int serial_is_open(serial_port_t *port);

/*
 * Close serial port and free handle
 */
void serial_close(serial_port_t *port);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_H */
