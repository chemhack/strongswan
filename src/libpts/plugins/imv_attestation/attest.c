/*
 * Copyright (C) 2011 Andreas Steffen
 * HSR Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#define _GNU_SOURCE
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include <library.h>
#include <debug.h>

#include <imcv.h>
#include <libpts.h>
#include <pts/pts_meas_algo.h>

#include "attest_db.h"
#include "attest_usage.h"

/**
 * global debug output variables
 */
static int debug_level = 0;
static bool stderr_quiet = TRUE;

/**
 * attest dbg function
 */
static void attest_dbg(debug_t group, level_t level, char *fmt, ...)
{
	int priority = LOG_INFO;
	char buffer[8192];
	char *current = buffer, *next;
	va_list args;

	if (level <= debug_level)
	{
		if (!stderr_quiet)
		{
			va_start(args, fmt);
			vfprintf(stderr, fmt, args);
			fprintf(stderr, "\n");
			va_end(args);
		}

		/* write in memory buffer first */
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		/* do a syslog with every line */
		while (current)
		{
			next = strchr(current, '\n');
			if (next)
			{
				*(next++) = '\0';
			}
			syslog(priority, "%s\n", current);
			current = next;
		}
	}
}

/**
 * global attestation database object
 */
attest_db_t *attest;

/**
 * atexit handler to close db on shutdown
 */
static void cleanup(void)
{
	attest->destroy(attest);
	libpts_deinit();
	libimcv_deinit();
	closelog();
}

static void do_args(int argc, char *argv[])
{
	enum {
		OP_UNDEF,
		OP_USAGE,
		OP_KEYS,
		OP_COMPONENTS,
		OP_FILES,
		OP_HASHES,
		OP_MEASUREMENTS,
		OP_PRODUCTS,
		OP_ADD,
		OP_DEL,
	} op = OP_UNDEF;

	/* reinit getopt state */
	optind = 0;

	while (TRUE)
	{
		int c;

		struct option long_opts[] = {
			{ "help", no_argument, NULL, 'h' },
			{ "components", no_argument, NULL, 'c' },
			{ "files", no_argument, NULL, 'f' },
			{ "keys", no_argument, NULL, 'k' },
			{ "products", no_argument, NULL, 'p' },
			{ "hashes", no_argument, NULL, 'H' },
			{ "measurements", no_argument, NULL, 'M' },
			{ "add", no_argument, NULL, 'a' },
			{ "delete", no_argument, NULL, 'd' },
			{ "del", no_argument, NULL, 'd' },
			{ "products", no_argument, NULL, 'p' },
			{ "hashes", no_argument, NULL, 'H' },
			{ "add", no_argument, NULL, 'a' },
			{ "delete", no_argument, NULL, 'd' },
			{ "del", no_argument, NULL, 'd' },
			{ "directory", required_argument, NULL, 'D' },
			{ "dir", required_argument, NULL, 'D' },
			{ "file", required_argument, NULL, 'F' },
			{ "key", required_argument, NULL, 'K' },
			{ "owner", required_argument, NULL, 'O' },
			{ "product", required_argument, NULL, 'P' },
			{ "sha1", no_argument, NULL, '1' },
			{ "sha256", no_argument, NULL, '2' },
			{ "sha384", no_argument, NULL, '3' },
			{ "did", required_argument, NULL, '4' },
			{ "fid", required_argument, NULL, '5' },
			{ "pid", required_argument, NULL, '6' },
			{ "cid", required_argument, NULL, '7' },
			{ "kid", required_argument, NULL, '8' },
			{ 0,0,0,0 }
		};

		c = getopt_long(argc, argv, "", long_opts, NULL);
		switch (c)
		{
			case EOF:
				break;
			case 'h':
				op = OP_USAGE;
				break;
			case 'c':
				op = OP_COMPONENTS;
				continue;
			case 'f':
				op = OP_FILES;
				continue;
			case 'k':
				op = OP_KEYS;
				continue;
			case 'p':
				op = OP_PRODUCTS;
				continue;
			case 'H':
				op = OP_HASHES;
				continue;
			case 'M':
				op = OP_MEASUREMENTS;
				continue;
			case 'a':
				op = OP_ADD;
				continue;
			case 'd':
				op = OP_DEL;
				continue;
			case 'C':
				if (!attest->set_component(attest, optarg, op == OP_ADD))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case 'D':
				if (!attest->set_directory(attest, optarg, op == OP_ADD))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case 'H':
				op = OP_HASHES;
				continue;
			case 'a':
				op = OP_ADD;
				continue;
			case 'd':
				op = OP_DEL;
				continue;
			case 'D':
				if (!attest->set_directory(attest, optarg, op == OP_ADD))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case 'F':
				if (!attest->set_file(attest, optarg, op == OP_ADD))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case 'K':
				if (!attest->set_key(attest, optarg, op == OP_ADD))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case 'O':
				attest->set_owner(attest, optarg);
				continue;
			case 'P':
				if (!attest->set_product(attest, optarg, op == OP_ADD))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case '1':
				attest->set_algo(attest, PTS_MEAS_ALGO_SHA1);
				continue;
			case '2':
				attest->set_algo(attest, PTS_MEAS_ALGO_SHA256);
				continue;
			case '3':
				attest->set_algo(attest, PTS_MEAS_ALGO_SHA384);
				continue;
			case '4':
				if (!attest->set_did(attest, atoi(optarg)))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case '5':
				if (!attest->set_fid(attest, atoi(optarg)))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case '6':
				if (!attest->set_pid(attest, atoi(optarg)))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case '7':
				if (!attest->set_cid(attest, atoi(optarg)))
				{
					exit(EXIT_FAILURE);
				}
				continue;
			case '8':
				if (!attest->set_kid(attest, atoi(optarg)))
				{
					exit(EXIT_FAILURE);
				}
				continue;
		}
		break;
	}

	switch (op)
	{
		case OP_USAGE:
			usage();
			break;
		case OP_PRODUCTS:
			attest->list_products(attest);
			break;
		case OP_KEYS:
			attest->list_keys(attest);
			break;
		case OP_COMPONENTS:
			attest->list_components(attest);
			break;
		case OP_FILES:
			attest->list_files(attest);
			break;
		case OP_HASHES:
			attest->list_hashes(attest);
			break;
		case OP_MEASUREMENTS:
			attest->list_measurements(attest);
			break;
		case OP_ADD:
			attest->add(attest);
			break;
		case OP_DEL:
			attest->delete(attest);
			break;
		case OP_HASHES:
			attest->list_hashes(attest);
			break;
		default:
			usage();
			exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	char *uri;

	/* enable attest debugging hook */
	dbg = attest_dbg;
	openlog("attest", 0, LOG_DEBUG);

	atexit(library_deinit);

	/* initialize library */
	if (!library_init(NULL))
	{
		exit(SS_RC_LIBSTRONGSWAN_INTEGRITY);
	}
	if (!lib->plugins->load(lib->plugins, NULL,
			lib->settings->get_str(lib->settings, "attest.load", PLUGINS)))
	{
		exit(SS_RC_INITIALIZATION_FAILED);
	}

	uri = lib->settings->get_str(lib->settings, "attest.database", NULL);
	if (!uri)
	{
		fprintf(stderr, "database URI attest.database not set.\n");
		exit(SS_RC_INITIALIZATION_FAILED);
	}
	attest = attest_db_create(uri);
	if (!attest)
	{
		exit(SS_RC_INITIALIZATION_FAILED);
	}
	atexit(cleanup);
	libimcv_init();
	libpts_init();

	do_args(argc, argv);

	exit(EXIT_SUCCESS);
}
