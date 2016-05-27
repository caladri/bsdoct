/*
 * Copyright (c) 2015-2016 Juli Mallett. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "target.h"

static void usage(void);

int
main(int argc, char *argv[])
{
	struct target_selector all, selected;
	unsigned n;
	int ch;

	TARGET_SELECTOR_CLEAR(&selected);

	all = target_identify();
	if (TARGET_SELECTOR_EMPTY(&all))
		errx(1, "no targets identified.");

	while ((ch = getopt(argc, argv, "as:")) != -1) {
		switch (ch) {
		case 'a':
			selected = all;
			break;
		case 's':
			n = atoi(optarg);
			if (!TARGET_SELECTED(&all, n))
				errx(1, "target%u not present.", n);
			TARGET_SELECT(&selected, n);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (TARGET_SELECTED_COUNT(&all) == 1 &&
	    TARGET_SELECTOR_EMPTY(&selected))
		selected = all;

	if (TARGET_SELECTOR_EMPTY(&selected)) {
		if (argc == 0) {
			printf("targets present:");
			for (n = 0; n < TARGET_SELECTOR_COUNT; n++) {
				if (TARGET_SELECTED(&all, n))
					printf(" %u", n);
			}
			printf("\n");
			return (0);
		}
		errx(1, "no targets specified.");
	}

	if (argc == 0 || strcmp(argv[0], "show") == 0) {
		if (argc > 1)
			usage();
		target_show(&selected);
		return (0);
	}

	if (strcmp(argv[0], "boot") == 0) {
		if (argc != 1)
			errx(1, "loading bootloader not yet supported.");
		target_boot(&selected);
		return (0);
	}

	if (strcmp(argv[0], "console") == 0) {
		if (argc != 1)
			usage();
		if (TARGET_SELECTED_COUNT(&selected) != 1)
			errx(1, "must select exactly one target for console.");
		//target_console(&selected);
		return (0);
	}

	if (strcmp(argv[0], "reset") == 0) {
		if (argc != 1)
			usage();
		target_reset(&selected);
		return (0);
	}

	fprintf(stderr, "unknown command: %s\n", argv[0]);
	usage();
}

static void
usage(void)
{
	fprintf(stderr,
"usage: bsdoct\n"
"       bsdoct -a command\n"
"       bsdoct -s target-number [-s target-number ...] command\n"
"\n"
"       if only one target is available, it will be selected by default\n"
"\n"
"       no command: show selected targets\n"
"       no command and no selectors: enumerate available targets\n"
"\n"
"       commands:\n"
"           boot [bootloader-path]\n"
"           console\n"
"           reset\n"
"           show\n");
}
