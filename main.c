#include "imagewriter.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char * g_imagewriter_fixed_font = "";
char * g_imagewriter_prop_font = "";
int iw_scc_write = 0;

int main(int argc, char * argv[])
{
	// settings
	unsigned short dpi = 144;
	unsigned short paperSize = 0;
	unsigned short bannerSize = 0;
	int multipageOutput = 1;
	char * output = "";

	// parse options
	int opt;
	while ((opt = getopt(argc, argv, "d:p:b:m:o:")) != -1) {
		switch (opt) {
		// dpi / paperSize / bannerSize / multiPageOutput
		case 'd':
			dpi = atoi(optarg);
			if (dpi < 1) {
				fprintf(stderr, "Invalid DPI %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;

		case 'p':
			paperSize = atoi(optarg);
			break;

		case 'b':
			bannerSize = atoi(optarg);
			break;

		case 'm':
			multipageOutput = atoi(optarg);
			break;

		// Output format
		case 'o':
			output = optarg;
			break;

		case '?':
			break;

		default:
			fprintf(stderr, "Unknown option\n");
			return EXIT_FAILURE;
		}
	}

	imagewriter_init(dpi, paperSize, bannerSize, output, multipageOutput);

	for (int index = optind; index < argc; index++) {
		FILE * file = fopen(argv[index], "rb");

		if (file == NULL) {
			perror("Failed to open file");
			return EXIT_FAILURE;
		}

		int c;
		while ((c = fgetc(file)) != -1)
			imagewriter_loop(c);

		imagewriter_feed();
		fclose(file);
	}

	imagewriter_close();

	return EXIT_SUCCESS;
}
