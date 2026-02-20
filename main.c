#include "imagewriter.h"
#include "serial.h"
#if defined(BUILD_NUMBER)
#include "build_number.h"
#else
#define BUILD_NUMBER 0
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if !defined(WIN32)
#include <sys/wait.h>
#endif

/* Config file: ~/.imagewriterrc */
#define CONFIG_MAX 256

struct app_config {
	long dpi;
	int paper_size;
	long banner_size;
	int multipage;
	char output[32];
	char serial_port[256];
	int serial_baud;
	int debug;
	char printer_name[128];
	char last_file[256];  /* for "run again" when no args */
	int last_mode;        /* 1=file, 2=serial */
};

static void config_defaults(struct app_config *c)
{
	c->dpi = 144;
	c->paper_size = 0;
	c->banner_size = 0;
	c->multipage = 0;
	c->output[0] = '\0';
	c->serial_port[0] = '\0';
	c->serial_baud = SERIAL_BAUD_DEFAULT;
	c->debug = 0;
	c->printer_name[0] = '\0';
	c->last_file[0] = '\0';
	c->last_mode = 0;
}

static const char *config_path(void)
{
	static char path[512];
	const char *home = getenv("HOME");
	if (!home) home = ".";
	snprintf(path, sizeof(path), "%s/.imagewriterrc", home);
	return path;
}

static void config_load(struct app_config *c)
{
	FILE *f;
	char line[512];
	char key[64], val[448];

	config_defaults(c);
	f = fopen(config_path(), "r");
	if (!f) return;

	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '#' || *p == '\n' || *p == '\0') continue;
		if (sscanf(p, "%63[^=]=%447[^\n]", key, val) != 2) continue;
		p = val + strlen(val);
		while (p > val && (p[-1] == ' ' || p[-1] == '\t')) *--p = '\0';
		p = val;
		while (*p == ' ' || *p == '\t') p++;
		if (strcmp(key, "dpi") == 0) c->dpi = atol(val);
		else if (strcmp(key, "paper") == 0) c->paper_size = atoi(val);
		else if (strcmp(key, "banner") == 0) c->banner_size = atol(val);
		else if (strcmp(key, "multipage") == 0) c->multipage = atoi(val);
		else if (strcmp(key, "output") == 0) strncpy(c->output, val, sizeof(c->output) - 1);
		else if (strcmp(key, "serial_port") == 0) strncpy(c->serial_port, val, sizeof(c->serial_port) - 1);
		else if (strcmp(key, "serial_baud") == 0) c->serial_baud = atoi(val);
		else if (strcmp(key, "debug") == 0) c->debug = atoi(val);
		else if (strcmp(key, "printer_name") == 0) strncpy(c->printer_name, val, sizeof(c->printer_name) - 1);
		else if (strcmp(key, "last_file") == 0) strncpy(c->last_file, val, sizeof(c->last_file) - 1);
		else if (strcmp(key, "last_mode") == 0) c->last_mode = atoi(val);
	}
	fclose(f);
}

static void config_save(const struct app_config *c)
{
	FILE *f = fopen(config_path(), "w");
	if (!f) return;
	fprintf(f, "# ImageWriter config\n");
	fprintf(f, "dpi=%ld\n", c->dpi);
	fprintf(f, "paper=%d\n", c->paper_size);
	fprintf(f, "banner=%ld\n", c->banner_size);
	fprintf(f, "multipage=%d\n", c->multipage);
	if (c->output[0]) fprintf(f, "output=%s\n", c->output);
	if (c->serial_port[0]) fprintf(f, "serial_port=%s\n", c->serial_port);
	fprintf(f, "serial_baud=%d\n", c->serial_baud);
	fprintf(f, "debug=%d\n", c->debug);
	if (c->printer_name[0]) fprintf(f, "printer_name=%s\n", c->printer_name);
	if (c->last_file[0]) fprintf(f, "last_file=%s\n", c->last_file);
	fprintf(f, "last_mode=%d\n", c->last_mode);
	fclose(f);
}

// globals shared with imagewriter.cpp
const char * g_imagewriter_fixed_font = "letgothl.ttf";
const char * g_imagewriter_prop_font = "letgothl.ttf";
const int iw_scc_write = 0;

/* Set by SIGINT handler to request graceful shutdown of serial listener */
static volatile int g_serial_stop = 0;

static void serial_sigint_handler(int sig)
{
	(void)sig;
	g_serial_stop = 1;
}

/* Convert str to unsigned short, return LONG_MIN on errors */
static long strtohu(const char *name, const char *val)
{
	char *endptr;
	errno = 0;
	long number = strtol(val, &endptr, 10);
	if (!*val || *endptr || errno || number < 0 || number > USHRT_MAX) {
		fprintf(stderr, "Invalid value '%s' for %s\n", val, name);
		return LONG_MIN;
	}
	return number;
}

/* Status callback for verbose/foreground mode */
static void status_callback(const char *msg)
{
	printf("  [%s]\n", msg);
	fflush(stdout);
}

/* Interactive mode configuration */
struct interactive_config {
	int input_mode;       /* 1=file, 2=serial */
	char files[8][256];   /* file paths */
	int num_files;
	char serial_port[256];
	int serial_baud;
	char output[32];      /* bmp, text, ps, colorps, printer */
	char printer_name[128];
	long dpi;
	int paper_size;
	long banner_size;
	int multipage;
	int debug_serial;
	int run_mode;         /* 1=foreground, 2=daemon */
	int verbose;
};

/* Read a line into buf, strip newline. Returns buf or NULL on EOF. */
static char *read_line(char *buf, size_t size)
{
	if (!fgets(buf, (int)size, stdin))
		return NULL;
	buf[strcspn(buf, "\n")] = '\0';
	return buf;
}

/* Prompt for yes/no. Default 1=yes. Returns 1=yes, 0=no. */
static int prompt_yn(const char *prompt, int default_yes)
{
	char buf[32];
	printf("%s [%s]: ", prompt, default_yes ? "Y/n" : "y/N");
	fflush(stdout);
	if (!read_line(buf, sizeof(buf)) || !buf[0])
		return default_yes;
	return (buf[0] == 'y' || buf[0] == 'Y') ? 1 : 0;
}

/* Prompt for menu selection. Returns 1-based index, 0 on cancel. */
static int prompt_menu(const char *prompt, const char *choices[], int nchoices)
{
	int i, sel;
	char buf[32];
	for (i = 0; i < nchoices; i++)
		printf("  %d) %s\n", i + 1, choices[i]);
	printf("%s [1-%d]: ", prompt, nchoices);
	fflush(stdout);
	if (!read_line(buf, sizeof(buf)) || !buf[0])
		return 1;
	sel = atoi(buf);
	if (sel < 1 || sel > nchoices)
		return 1;
	return sel;
}

/* List printers via lpstat. Returns count, fills names[]. */
static int list_printers(char names[][128], int max_names)
{
#if defined(WIN32)
	(void)names;
	(void)max_names;
	return 0;
#else
	FILE *fp;
	char line[256];
	int count = 0;
	fp = popen("lpstat -p 2>/dev/null", "r");
	if (!fp) return 0;
	while (fgets(line, sizeof(line), fp) && count < max_names) {
		char *p = line;
		/* Skip "printer " */
		if (strncmp(p, "printer ", 8) == 0)
			p += 8;
		/* Token is until " is" or space */
		char *end = strstr(p, " is");
		if (!end) end = strchr(p, ' ');
		if (end) {
			size_t len = (size_t)(end - p);
			if (len > 0 && len < 128) {
				memcpy(names[count], p, len);
				names[count][len] = '\0';
				count++;
			}
		}
	}
	pclose(fp);
	return count;
#endif
}

static int run_interactive(struct interactive_config *cfg);

/* Set timestamped output prefix for this run (e.g. imagewriter_20260217_224816) */
static void set_output_timestamp(void)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char prefix[64];
	if (tm) {
		snprintf(prefix, sizeof(prefix), "imagewriter_%04d%02d%02d_%02d%02d%02d",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
		imagewriter_set_output_prefix(prefix);
	} else {
		imagewriter_set_output_prefix("");
	}
}

/* Run serial mode (shared by CLI and interactive) */
static int run_serial(const char *port_path, int baud, long dpi, int paper, long banner,
	const char *output, int multipage, int debug, const char *printer, int verbose)
{
	serial_port_t *port = serial_open(port_path, baud);
	if (!port) return EXIT_FAILURE;

	setenv("SDL_VIDEODRIVER", "dummy", 1);
	setenv("SDL_AUDIODRIVER", "dummy", 1);

	if (verbose)
		imagewriter_set_status_callback(status_callback);
	if (printer && printer[0])
		imagewriter_set_printer_name(printer);

	if (verbose)
		printf("  [Initializing virtual ImageWriter]\n");
	set_output_timestamp();
	imagewriter_init((int)dpi, paper, (int)banner, (char*)output, multipage);

#ifdef SIGINT
	signal(SIGINT, serial_sigint_handler);
#endif

	FILE *sessionFile = NULL;
	char sessionPath[256] = "";
	if (debug) {
		time_t now = time(NULL);
		struct tm *tm = localtime(&now);
		if (tm) {
			snprintf(sessionPath, sizeof(sessionPath),
				"imagewriter_session_%04d%02d%02d_%02d%02d%02d.bin",
				tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
				tm->tm_hour, tm->tm_min, tm->tm_sec);
			sessionFile = fopen(sessionPath, "wb");
		}
		if (sessionFile) {
			/* Write header: "IWDB" magic + 4-byte build number (LE) */
			unsigned char hdr[8] = { 'I','W','D','B', 0, 0, 0, 0 };
			unsigned n = (unsigned)BUILD_NUMBER;
			hdr[4] = (unsigned char)(n & 0xFF);
			hdr[5] = (unsigned char)((n >> 8) & 0xFF);
			hdr[6] = (unsigned char)((n >> 16) & 0xFF);
			hdr[7] = (unsigned char)((n >> 24) & 0xFF);
			if (fwrite(hdr, 1, 8, sessionFile) != 8)
				perror("Session header write");
			if (verbose)
				printf("  [Debug: dumping to %s (build %u)]\n", sessionPath, n);
		}
	}

	if (verbose)
		printf("  [Listening on %s - Ctrl+C to stop]\n", port_path);
	else
		printf("Listening on %s. Press Ctrl+C to stop and eject page.\n", port_path);

	unsigned char buf[256];
	int idle_count = 0;
	int receiving_shown = 0;
	while (!g_serial_stop && serial_is_open(port)) {
		int n = serial_read(port, buf, sizeof(buf));
		if (n > 0) {
			if (verbose && !receiving_shown) {
				printf("  [Receiving input]\n");
				receiving_shown = 1;
			}
			idle_count = 0;
			if (sessionFile && fwrite(buf, 1, (size_t)n, sessionFile) != (size_t)n)
				perror("Session file write");
			for (int i = 0; i < n; i++) {
				unsigned char b = buf[i];
				/* Apple II: map line-ending codes to CR */
				if (b == 0x8D || b == 0xFD || b == 0xA9) {
					imagewriter_loop(0x0D);
					continue;
				}
				/* Apple II IIc: 0xE0 often appears as digit 0 in LIST output */
				if (b == 0xE0) {
					imagewriter_loop(0x30);
					continue;
				}
				/* IIc LIST: 0xB2 0xB9 sequence observed for PRINT keyword */
				if (b == 0xB2 && i + 1 < n && buf[i + 1] == 0xB9) {
					for (const char *p = "PRINT "; *p; p++)
						imagewriter_loop((unsigned char)*p);
					i++;
					continue;
				}
				/* Applesoft tokens for LIST output */
				if (b == 0xBA) { for (const char *p = "PRINT "; *p; p++) imagewriter_loop((unsigned char)*p); continue; }
				if (b == 0xAB) { for (const char *p = "GOTO "; *p; p++) imagewriter_loop((unsigned char)*p); continue; }
				/* ImageWriter Technical Reference: 8th bit is always 1 for data, 0 for control codes.
				 * Strip high bit only when set to get 7-bit ASCII; pass control codes through. */
				imagewriter_loop((b & 0x80) ? (b & 0x7F) : b);
			}
		} else if (n < 0) {
			perror("Serial read error");
			break;
		} else {
			idle_count++;
			if (verbose && idle_count == 500)
				printf("  [Waiting for input]\n");
			receiving_shown = (idle_count < 500);
			usleep(1000);
		}
	}

	if (sessionFile) {
		fclose(sessionFile);
		if (verbose) printf("  [Session saved to %s]\n", sessionPath);
		else printf("Session dump saved to %s\n", sessionPath);
	}

	if (verbose) printf("  [Ejecting page]\n");
	imagewriter_feed();
	imagewriter_close();
	serial_close(port);
	if (verbose) printf("  [Stopped]\n");
	else printf("Serial listener stopped.\n");
	return EXIT_SUCCESS;
}

/* Apple II preprocessing: same logic as serial loop. Call when replaying session dumps. */
static void apple2_preprocess_feed(const unsigned char *buf, int n)
{
	for (int i = 0; i < n; i++) {
		unsigned char b = buf[i];
		if (b == 0x8D || b == 0xFD || b == 0xA9) {
			imagewriter_loop(0x0D);
			continue;
		}
		if (b == 0xE0) {
			imagewriter_loop(0x30);
			continue;
		}
		if (b == 0xB2 && i + 1 < n && buf[i + 1] == 0xB9) {
			for (const char *p = "PRINT "; *p; p++)
				imagewriter_loop((unsigned char)*p);
			i++;
			continue;
		}
		if (b == 0xBA) { for (const char *p = "PRINT "; *p; p++) imagewriter_loop((unsigned char)*p); continue; }
		if (b == 0xAB) { for (const char *p = "GOTO "; *p; p++) imagewriter_loop((unsigned char)*p); continue; }
		/* ImageWriter Technical Reference: 8th bit always 1 for data, 0 for control */
		imagewriter_loop((b & 0x80) ? (b & 0x7F) : b);
	}
}

/* Run file mode (shared by CLI and interactive) */
static int run_file_mode(char *files[], int num_files, long dpi, int paper, long banner,
	const char *output, int multipage, const char *printer, int verbose)
{
	setenv("SDL_VIDEODRIVER", "dummy", 1);
	setenv("SDL_AUDIODRIVER", "dummy", 1);

	if (verbose)
		imagewriter_set_status_callback(status_callback);
	if (printer && printer[0])
		imagewriter_set_printer_name(printer);

	if (verbose)
		printf("  [Initializing virtual ImageWriter]\n");
	set_output_timestamp();
	imagewriter_init((int)dpi, paper, (int)banner, (char*)output, multipage);

	for (int i = 0; i < num_files; i++) {
		if (verbose)
			printf("  [Processing file %d/%d: %s]\n", i + 1, num_files, files[i]);
		else
			printf("Parsing %s...\n", files[i]);

		FILE *file = fopen(files[i], "rb");
		if (!file) {
			perror("Failed to open file");
			imagewriter_close();
			return EXIT_FAILURE;
		}
		/* Skip ImageWriter session header if present ("IWDB" + 4-byte build) */
		int is_session = 0;
		{
			unsigned char magic[4];
			if (fread(magic, 1, 4, file) == 4 &&
			    magic[0] == 'I' && magic[1] == 'W' && magic[2] == 'D' && magic[3] == 'B') {
				is_session = 1;
				if (fseek(file, 4, SEEK_CUR) != 0)  /* skip build number */
					perror("Seek in session file");
			} else {
				rewind(file);
				/* Old session files (no header) have "session" in filename */
				if (strstr(files[i], "session") != NULL)
					is_session = 1;
			}
		}

		if (is_session) {
			/* Session dump: apply Apple II preprocessing (same as serial mode) */
			unsigned char buf[8192];
			size_t nr;
			while ((nr = fread(buf, 1, sizeof(buf), file)) > 0)
				apple2_preprocess_feed(buf, (int)nr);
		} else {
			int c;
			while ((c = fgetc(file)) != -1)
				imagewriter_loop(c);
		}

		if (!feof(file)) {
			perror("Error reading file");
			fclose(file);
			imagewriter_close();
			return EXIT_FAILURE;
		}

		imagewriter_feed();
		fclose(file);
	}

	if (verbose) printf("  [Complete]\n");
	else printf("Closing ImageWriter.\n");
	imagewriter_close();
	return EXIT_SUCCESS;
}

static int run_interactive(struct interactive_config *cfg)
{
#if defined(WIN32)
	/* Daemon mode not supported on Windows */
	cfg->run_mode = 1;
#endif

	if (cfg->run_mode == 2) {
		/* Daemon: fork and run in background */
		pid_t pid = fork();
		if (pid < 0) {
			perror("fork");
			return EXIT_FAILURE;
		}
		if (pid > 0) {
			printf("ImageWriter daemon started (PID %d)\n", pid);
			return EXIT_SUCCESS;
		}
		/* Child */
		setsid();
		if (chdir("/") < 0) { /* ignore */ }
		int fd = open("/dev/null", O_RDWR);
		if (fd >= 0) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > 2) close(fd);
		}
		cfg->verbose = 0;  /* No output in daemon */
	}

	if (cfg->input_mode == 2) {
		return run_serial(cfg->serial_port, cfg->serial_baud, cfg->dpi, cfg->paper_size,
			cfg->banner_size, cfg->output, cfg->multipage, cfg->debug_serial,
			cfg->printer_name, cfg->verbose);
	} else {
		char *file_ptrs[8];
		for (int i = 0; i < cfg->num_files; i++)
			file_ptrs[i] = cfg->files[i];
		return run_file_mode(file_ptrs, cfg->num_files, cfg->dpi, cfg->paper_size,
			cfg->banner_size, cfg->output, cfg->multipage, cfg->printer_name, cfg->verbose);
	}
}

static void usage(const char *progname);

static int do_interactive_mode(void)
{
	struct app_config saved_cfg;
	config_load(&saved_cfg);

	struct interactive_config cfg = {0};
	char buf[256];
	const char *outputs[] = {"bmp", "text", "ps", "colorps", "printer"};
	const char *papers[] = {"US Letter", "A4"};
	const int bauds[] = {300, 1200, 2400, 9600, 19200};
	const char *run_modes[] = {"Foreground (current session, with status)", "Background daemon"};

	printf("\n=== ImageWriter Interactive Mode ===\n\n");

	/* Input mode */
	printf("Input source:\n");
	printf("  1) File(s)\n");
	printf("  2) Serial port\n");
	printf("Choose [1-2]: ");
	fflush(stdout);
	if (!read_line(buf, sizeof(buf))) return EXIT_FAILURE;
	cfg.input_mode = (buf[0] == '2') ? 2 : 1;

	if (cfg.input_mode == 1) {
		printf("Enter input file path (or multiple paths, one per line; empty line to finish):\n");
		cfg.num_files = 0;
		while (cfg.num_files < 8) {
			printf("  File %d: ", cfg.num_files + 1);
			fflush(stdout);
			if (!read_line(buf, sizeof(buf)) || !buf[0]) break;
			strncpy(cfg.files[cfg.num_files], buf, sizeof(cfg.files[0]) - 1);
			cfg.files[cfg.num_files][sizeof(cfg.files[0]) - 1] = '\0';
			cfg.num_files++;
		}
		if (cfg.num_files == 0) {
			fprintf(stderr, "No files specified.\n");
			return EXIT_FAILURE;
		}
	} else {
		const char *paths[32];
		int n = serial_get_port_list(paths, 32);
		if (n <= 0) {
			printf("No serial ports found. Enter path manually (e.g. /dev/cu.usbserial-xxx): ");
			fflush(stdout);
			if (!read_line(buf, sizeof(buf)) || !buf[0]) return EXIT_FAILURE;
			strncpy(cfg.serial_port, buf, sizeof(cfg.serial_port) - 1);
		} else {
			printf("Select serial port:\n");
			for (int i = 0; i < n; i++)
				printf("  %d) %s\n", i + 1, paths[i]);
			printf("  %d) Enter path manually\n", n + 1);
			printf("Choose [1-%d]: ", n + 1);
			fflush(stdout);
			if (!read_line(buf, sizeof(buf))) return EXIT_FAILURE;
			int sel = atoi(buf);
			if (sel >= 1 && sel <= n)
				strncpy(cfg.serial_port, paths[sel - 1], sizeof(cfg.serial_port) - 1);
			else {
				printf("Path: ");
				fflush(stdout);
				if (!read_line(buf, sizeof(buf))) return EXIT_FAILURE;
				strncpy(cfg.serial_port, buf, sizeof(cfg.serial_port) - 1);
			}
		}
		cfg.serial_port[sizeof(cfg.serial_port) - 1] = '\0';

		printf("Baud rate: 1) 300  2) 1200  3) 2400  4) 9600  5) 19200 [4]: ");
		fflush(stdout);
		if (!read_line(buf, sizeof(buf)) || !buf[0]) buf[0] = '4';
		int br = (buf[0] >= '1' && buf[0] <= '5') ? (buf[0] - '0') : 4;
		cfg.serial_baud = bauds[br - 1];

		cfg.debug_serial = prompt_yn("Debug: dump raw serial to session file?", 0);
	}

	/* Output type */
	int out_sel = prompt_menu("Output format", outputs, 5);
	strncpy(cfg.output, outputs[out_sel - 1], sizeof(cfg.output) - 1);
	cfg.output[sizeof(cfg.output) - 1] = '\0';

	if (strcmp(cfg.output, "printer") == 0) {
		char pnames[16][128];
		int np = list_printers(pnames, 16);
		if (np > 0) {
			printf("Available printers:\n");
			for (int i = 0; i < np; i++)
				printf("  %d) %s\n", i + 1, pnames[i]);
			printf("  %d) Default printer\n", np + 1);
			printf("Choose [1-%d]: ", np + 1);
			fflush(stdout);
			if (read_line(buf, sizeof(buf)) && buf[0]) {
				int sel = atoi(buf);
				if (sel >= 1 && sel <= np)
					strncpy(cfg.printer_name, pnames[sel - 1], sizeof(cfg.printer_name) - 1);
			}
		} else {
			printf("No printers found (lpstat). Using default.\n");
		}
		cfg.printer_name[sizeof(cfg.printer_name) - 1] = '\0';
	}

	/* DPI */
	printf("DPI [144]: ");
	fflush(stdout);
	if (read_line(buf, sizeof(buf)) && buf[0]) {
		long v = strtohu("DPI", buf);
		cfg.dpi = (v >= 0) ? v : 144;
	} else {
		cfg.dpi = 144;
	}

	/* Paper size */
	int psel = prompt_menu("Paper size", papers, 2);
	cfg.paper_size = psel - 1;

	printf("Banner size (0=normal) [0]: ");
	fflush(stdout);
	if (read_line(buf, sizeof(buf)) && buf[0]) {
		long v = strtohu("Banner", buf);
		cfg.banner_size = (v >= 0) ? v : 0;
	} else {
		cfg.banner_size = 0;
	}

	cfg.multipage = prompt_yn("Multipage output (combine pages)?", 0);

	/* Run mode */
	cfg.run_mode = prompt_menu("Run mode", run_modes, 2);
	cfg.verbose = (cfg.run_mode == 1);

	if (cfg.verbose)
		printf("\n--- Starting (foreground with status) ---\n\n");

	/* Save config for next run */
	saved_cfg.dpi = cfg.dpi;
	saved_cfg.paper_size = cfg.paper_size;
	saved_cfg.banner_size = cfg.banner_size;
	saved_cfg.multipage = cfg.multipage;
	snprintf(saved_cfg.output, sizeof(saved_cfg.output), "%s", cfg.output);
	snprintf(saved_cfg.serial_port, sizeof(saved_cfg.serial_port), "%s", cfg.serial_port);
	saved_cfg.serial_baud = cfg.serial_baud;
	saved_cfg.debug = cfg.debug_serial;
	snprintf(saved_cfg.printer_name, sizeof(saved_cfg.printer_name), "%s", cfg.printer_name);
	saved_cfg.last_mode = cfg.input_mode;
	if (cfg.input_mode == 1 && cfg.num_files > 0)
		snprintf(saved_cfg.last_file, sizeof(saved_cfg.last_file), "%s", cfg.files[0]);
	config_save(&saved_cfg);

	return run_interactive(&cfg);
}

static void usage(const char * progname)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  File mode:    %s [-d dpi] [-p paper] [-b banner] [-o output] [-m] file [file2 ...]\n", progname);
	fprintf(stderr, "  Serial mode:  %s [-d dpi] [-p paper] [-b banner] [-o output] [-B baud] [-D] -s <port>\n", progname);
	fprintf(stderr, "  Interactive:  %s -i\n", progname);
	fprintf(stderr, "  List ports:   %s -l\n", progname);
	fprintf(stderr, "\n");
	fprintf(stderr, "  -i           Interactive mode (menus and prompts)\n");
	fprintf(stderr, "  -s <port>    Serial port\n");
	fprintf(stderr, "  -l           List serial ports\n");
	fprintf(stderr, "  -B <baud>    Baud rate (300,1200,2400,9600,19200)\n");
	fprintf(stderr, "  -o <type>    Output: bmp, text, ps, colorps, printer\n");
	fprintf(stderr, "  -D           Debug: dump raw serial to session file\n");
	fprintf(stderr, "  -d, -p, -b, -m  DPI, paper, banner, multipage\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Config: %s (loaded when no options given; saved after each run)\n", config_path());
}

int main(int argc, char * argv[])
{
	struct app_config cfg;
	static char output_buf[32];
	static char serial_port_buf[256];
	long dpi;
	long paperSize;
	long bannerSize;
	int multipageOutput;
	char * output = output_buf;
	char * serialPort = NULL;
	int serialBaud;
	int listPortsOnly = 0;
	int debugSerial = 0;
	int interactive = 0;

	config_load(&cfg);
	dpi = cfg.dpi;
	paperSize = cfg.paper_size;
	bannerSize = cfg.banner_size;
	multipageOutput = cfg.multipage;
	serialBaud = cfg.serial_baud;
	debugSerial = cfg.debug;
	snprintf(output_buf, sizeof(output_buf), "%s", cfg.output[0] ? cfg.output : "bmp");
	if (cfg.serial_port[0]) {
		snprintf(serial_port_buf, sizeof(serial_port_buf), "%s", cfg.serial_port);
		serialPort = serial_port_buf;
	}

	int opt;
	while ((opt = getopt(argc, argv, "d:p:b:mo:s:B:lDi")) != -1) {
		switch (opt) {
		case 'd':
			dpi = strtohu("DPI", optarg);
			if (dpi < 0) return EXIT_FAILURE;
			break;
		case 'p':
			paperSize = strtohu("Paper Size", optarg);
			if (paperSize < 0) return EXIT_FAILURE;
			if (paperSize > 1) {
				fprintf(stderr, "Paper size must be 0 (US Letter) or 1 (A4)\n");
				return EXIT_FAILURE;
			}
			break;
		case 'b':
			bannerSize = strtohu("Banner Size", optarg);
			if (bannerSize < 0) return EXIT_FAILURE;
			break;
		case 'm':
			multipageOutput = 1;
			break;
		case 'o':
			snprintf(output_buf, sizeof(output_buf), "%s", optarg);
			output = output_buf;
			break;
		case 's':
			serialPort = optarg;
			break;
		case 'B':
			serialBaud = (int)strtohu("Baud rate", optarg);
			if (serialBaud < 0) return EXIT_FAILURE;
			if (serialBaud != 300 && serialBaud != 1200 && serialBaud != 2400 &&
			    serialBaud != 9600 && serialBaud != 19200) {
				fprintf(stderr, "Baud rate must be 300, 1200, 2400, 9600, or 19200\n");
				return EXIT_FAILURE;
			}
			break;
		case 'l':
			listPortsOnly = 1;
			break;
		case 'D':
			debugSerial = 1;
			break;
		case 'i':
			interactive = 1;
			break;
		case '?':
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (interactive)
		return do_interactive_mode();

	if (listPortsOnly) {
		serial_list_ports();
		return EXIT_SUCCESS;
	}

	if (serialPort) {
		if (optind < argc) {
			fprintf(stderr, "Serial mode: do not specify input files.\n");
			usage(argv[0]);
			return EXIT_FAILURE;
		}
		if (!output[0]) output = "bmp";
		printf("Initializing virtual ImageWriter [serial mode, dpi=%ld, output='%s', baud=%d]\n",
			dpi, output, serialBaud);
		cfg.last_mode = 2;
		cfg.dpi = dpi;
		cfg.paper_size = (int)paperSize;
		cfg.banner_size = bannerSize;
		cfg.multipage = multipageOutput;
		snprintf(cfg.output, sizeof(cfg.output), "%s", output);
		snprintf(cfg.serial_port, sizeof(cfg.serial_port), "%s", serialPort);
		cfg.serial_baud = serialBaud;
		cfg.debug = debugSerial;
		config_save(&cfg);
		return run_serial(serialPort, serialBaud, dpi, (int)paperSize, bannerSize,
			output, multipageOutput, debugSerial, cfg.printer_name[0] ? cfg.printer_name : NULL, 0);
	}

	if (optind >= argc) {
		if (cfg.last_file[0] && cfg.last_mode == 1) {
			char *files[1] = { cfg.last_file };
			printf("Using saved config (last file: %s)\n", cfg.last_file);
			if (!output[0]) output = "bmp";
			cfg.last_mode = 1;
			cfg.dpi = dpi;
			cfg.paper_size = (int)paperSize;
			cfg.banner_size = bannerSize;
			cfg.multipage = multipageOutput;
			snprintf(cfg.output, sizeof(cfg.output), "%s", output);
			config_save(&cfg);
			return run_file_mode(files, 1, dpi, (int)paperSize, bannerSize,
				output, multipageOutput, cfg.printer_name[0] ? cfg.printer_name : NULL, 0);
		}
		fprintf(stderr, "No input files specified. Use -i for interactive mode, -l to list ports.\n");
		fprintf(stderr, "Or run with a file first to save config for 'run again'.\n");
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if (!output[0]) output = "bmp";
	printf("Initializing virtual ImageWriter [dpi=%ld, paperSize=%ld, bannerSize=%ld, output='%s', multipageOutput=%d]\n",
		dpi, paperSize, bannerSize, output, multipageOutput);
	cfg.last_mode = 1;
	cfg.dpi = dpi;
	cfg.paper_size = (int)paperSize;
	cfg.banner_size = bannerSize;
	cfg.multipage = multipageOutput;
	snprintf(cfg.output, sizeof(cfg.output), "%s", output);
	snprintf(cfg.last_file, sizeof(cfg.last_file), "%s", argv[optind]);
	config_save(&cfg);
	return run_file_mode(&argv[optind], argc - optind, dpi, (int)paperSize, bannerSize,
		output, multipageOutput, cfg.printer_name[0] ? cfg.printer_name : NULL, 0);
}
