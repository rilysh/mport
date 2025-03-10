/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2010-2018, 2021 Lucas Holt
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <err.h>
#include <locale.h>
#include <getopt.h>
#include <mport.h>
#include <mport_private.h>

#define MPORT_TOOLS_PATH "/usr/libexec/"

static void usage(void);

static void show_version(mportInstance *, int);

static void loadIndex(mportInstance *);

static mportIndexEntry **lookupIndex(mportInstance *, const char *);

static int install(mportInstance *, const char *);

static int cpeList(mportInstance *);

static int configGet(mportInstance *, const char *);

static int configSet(mportInstance *, const char *, const char *);

static int delete(const char *);

static int deleteAll(mportInstance *);

static int info(mportInstance *, const char *);

static int search(mportInstance *, char **);

static int stats(mportInstance *mport);

static int clean(mportInstance *);

static int verify(mportInstance *);

static int lock(mportInstance *, const char *);

static int unlock(mportInstance *, const char *);

static int which(mportInstance *, const char *, bool, bool);

int
main(int argc, char *argv[]) {
	char *flag = NULL, *buf = NULL;
	mportInstance *mport;
	int resultCode = MPORT_ERR_FATAL;
	int tempResultCode;
	int i;
	char **searchQuery;
	signed char ch;
	const char *chroot_path = NULL;
	const char *outputPath = NULL;
	int version = 0;
	int noIndex = 0;

	struct option longopts[] = {
		    {"no-index", no_argument, NULL, 'U'},
			{"chroot",  required_argument, NULL, 'c'},
			{"output",  required_argument, NULL, 'o'},
			{"version", no_argument,       NULL, 'v'},
			{NULL,      0,                 NULL, 0},
	};

	if (argc < 2)
		usage();

	if (setenv("POSIXLY_CORRECT", "1", 1) == -1)
		err(EXIT_FAILURE, "setenv() failed");


	setlocale(LC_ALL, "");

	while ((ch = getopt_long(argc, argv, "+c:o:Uv", longopts, NULL)) != -1) {
		switch (ch) {
			case 'U':
				noIndex++;
				break;
			case 'c':
				chroot_path = optarg;
				break;
			case 'o':
				outputPath = optarg;
                break;
			case 'v':
				version++;
				break;
			default:
				errx(EXIT_FAILURE, "Invalid argument provided");
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL, outputPath, noIndex != 0) != MPORT_OK) {
		errx(1, "%s", mport_err_string());
	}

	if (version == 1) {
		show_version(mport, version);
		mport_instance_free(mport);
		exit(EXIT_SUCCESS);
	}

	char *cmd = argv[0];

	if (!strcmp(cmd, "install")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}
		loadIndex(mport);
		for (i = 1; i < argc; i++) {
			tempResultCode = install(mport, argv[i]);
			if (tempResultCode != 0)
				resultCode = tempResultCode;
		}
	} else if (!strcmp(cmd, "delete")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}
		for (i = 1; i < argc; i++) {
			tempResultCode = delete(argv[i]);
			if (tempResultCode != 0)
				resultCode = tempResultCode;
		}
	} else if (!strcmp(cmd, "update")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}
		loadIndex(mport);
		for (i = 1; i < argc; i++) {
			tempResultCode = mport_update(mport, argv[i]);
			if (tempResultCode != 0)
				resultCode = tempResultCode;
		}
	} else if (!strcmp(cmd, "download")) {
		loadIndex(mport);
		char *path;

		int local_argc = argc;
		char *const *local_argv = argv;
		int dflag = 0;

		if (local_argc > 1) {
			int ch2;
			while ((ch2 = getopt(local_argc, local_argv, "d")) != -1) {
				switch (ch2) {
					case 'd':
						dflag = 1;
						break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		for (i = 1; i < argc; i++) {
			tempResultCode = mport_download(mport, argv[i], dflag == 1, &path);
			if (tempResultCode != 0) {
				resultCode = tempResultCode;
			} else if (path != NULL) {
				free(path);
			}
		}
	} else if (!strcmp(cmd, "upgrade")) {
		loadIndex(mport);
		resultCode = mport_upgrade(mport);
	} else if (!strcmp(cmd, "locks")) {
		asprintf(&buf, "%s%s", MPORT_TOOLS_PATH, "mport.list");
		flag = strdup("-l");
		resultCode = execl(buf, "mport.list", flag, (char *) 0);
		free(flag);
		free(buf);
	} else if (!strcmp(cmd, "import")) {
		loadIndex(mport);
		resultCode = mport_import(mport, argv[2]);
	} else if (!strcmp(cmd, "export")) {
		resultCode = mport_export(mport, argv[2]);
	} else if (!strcmp(cmd, "lock")) {
		if (argc > 1) {
			lock(mport, argv[1]);
		} else {
			usage();
		}
	} else if (!strcmp(cmd, "unlock")) {
		if (argc > 1) {
			unlock(mport, argv[1]);
		} else {
			usage();
		}
	} else if (!strcmp(cmd, "list")) {
		asprintf(&buf, "%s%s", MPORT_TOOLS_PATH, "mport.list");
		if (argc > 1) {
			if (!strcmp(argv[1], "updates") ||
			    !strcmp(argv[1], "up")) {
				flag = strdup("-u");
			} else if (!strcmp(argv[1], "prime")) {
				flag = strdup("-p");
			} else {
				mport_instance_free(mport);
				usage();
			}
		} else {
			flag = strdup("-v");
		}
		resultCode = execl(buf, "mport.list", flag, (char *) 0);
		free(flag);
		free(buf);
	} else if (!strcmp(cmd, "info")) {
		loadIndex(mport);
		resultCode = info(mport, argv[1]);
	} else if (!strcmp(cmd, "index")) {
		resultCode = mport_index_get(mport);
		if (resultCode != MPORT_OK) {
			fprintf(stderr, "Unable to fetch index: %s\n", mport_err_string());
		}
	} else if (!strcmp(cmd, "search")) {
		loadIndex(mport);
		searchQuery = calloc((size_t) argc - 1, sizeof(char *));
		for (i = 1; i < argc; i++) {
			searchQuery[i - 1] = strdup(argv[i]);
		}
		resultCode = search(mport, searchQuery);
		for (i = 1; i < argc; i++) {
			free(searchQuery[i - 1]);
		}
		free(searchQuery);
	} else if (!strcmp(cmd, "stats")) {
		loadIndex(mport);
		resultCode = stats(mport);
	} else if (!strcmp(cmd, "clean")) {
		loadIndex(mport);
		resultCode = clean(mport);
	} else if (!strcmp(cmd, "config")) {
		if (argc < 2) {
			mport_instance_free(mport);
			usage();
		}

		if (!strcmp(argv[1], "get")) {
			resultCode = configGet(mport, argv[2]);
		} else if (!strcmp(argv[1], "set")) {
			resultCode = configSet(mport, argv[2], argv[3]);
		}
	} else if (!strcmp(cmd, "mirror")) {
		if (argc > 2) {
			if (!strcmp(argv[1], "list")) {
				loadIndex(mport);
				printf("To set a mirror, use the following command:\n");
				printf("mport set config mirror_region <country>\n\n");
				resultCode = mport_index_print_mirror_list(mport);
			}
		}
	} else if (!strcmp(cmd, "cpe")) {
		resultCode = cpeList(mport);
	} else if (!strcmp(cmd, "deleteall")) {
		resultCode = deleteAll(mport);
	} else if (!strcmp(cmd, "autoremove")) {
		resultCode = mport_autoremove(mport);
	} else if (!strcmp(cmd, "verify")) {
		resultCode = verify(mport);
	} else if (!strcmp(cmd, "version")) {
		int local_argc = argc;
		char *const *local_argv = argv;
		if (local_argc > 1) {
			int ch2, tflag;
			tflag = 0;
			while ((ch2 = getopt(local_argc, local_argv, "t")) != -1) {
				switch (ch2) {
					case 't':
						tflag = 1;
						break;
				}
			}
			local_argc -= optind;
			local_argv += optind;

			if (tflag) {
				if (local_argv[0] == NULL) {
					fprintf(stderr, "Usage: mport version -t <v1> <v2>\n");
					return -2;
				}
				if (local_argv[1] == NULL) {
					fprintf(stderr, "Usage: mport version -t <v1> <v2>\n");
					return -2;
				}
				resultCode = mport_version_cmp(local_argv[0], local_argv[1]);
				printf("%c\n", resultCode == 0 ? '=' : resultCode == -1 ? '<' : '>');
			}
		}
	} else if (!strcmp(cmd, "which")) {
		int local_argc = argc;
		char *const *local_argv = argv;
		if (local_argc > 1) {
			int ch2, qflag, oflag;
			qflag = oflag = 0;
			while ((ch2 = getopt(local_argc, local_argv, "qo")) != -1) {
				switch (ch2) {
					case 'q':
						qflag = 1;
						break;
					case 'o':
						oflag = 1;
						break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
			which(mport, *local_argv, qflag, oflag);
		} else {
			usage();
		}
	} else {
		mport_instance_free(mport);
		usage();
	}

	mport_instance_free(mport);
	exit(resultCode);
}

void
usage(void) {
	show_version(NULL, 2);

	fprintf(stderr,
	        "usage: mport [-c chroot dir] [-U] [-o output] <command> args:\n"
	        "       mport autoremove\n"
	        "       mport clean\n"
	        "       mport config get [setting name]\n"
	        "       mport config set [setting name] [setting val]\n"
	        "       mport cpe\n"
	        "       mport delete [package name]\n"
	        "       mport deleteall\n"
	        "       mport download [-d] [package name]\n"
	        "       mport export [filename]\n"
	        "       mport import [filename]\n"
	        "       mport index\n"
	        "       mport info [package name]\n"
	        "       mport install [package name]\n"
	        "       mport list [updates|prime]\n"
	        "       mport lock [package name]\n"
	        "       mport locks\n"
		"       mport mirror list\n"
	        "       mport search [query ...]\n"
	        "       mport stats\n"
	        "       mport unlock [package name]\n"
	        "       mport update [package name]\n"
	        "       mport upgrade\n"
	        "       mport verify\n"
		"       mport version -t [v1] [v2]\n"
	        "       mport which [file path]\n"
	);
	exit(EXIT_FAILURE);
}

void
show_version(mportInstance *mport, int count) {
	char *version;
	if (count == 1)
		version = mport_version_short(mport);
	else
		version = mport_version(mport);
	fprintf(stderr, "%s", version);
	if (mport == NULL)
		fprintf(stderr, "(Host OS version, not configured)\n\n");
	free(version);
}

void
loadIndex(mportInstance *mport) {
	int result = mport_index_load(mport);
	if (result == MPORT_ERR_WARN)
		warnx("%s", mport_err_string());
	else if (result != MPORT_OK)
		errx(4, "Unable to load index %s", mport_err_string());
}

mportIndexEntry **
lookupIndex(mportInstance *mport, const char *packageName) {
	mportIndexEntry **indexEntries;

	if (mport_index_lookup_pkgname(mport, packageName, &indexEntries) != MPORT_OK) {
		fprintf(stderr, "Error looking up package name %s: %d %s\n",
		        packageName, mport_err_code(), mport_err_string());
		errx(mport_err_code(), "%s", mport_err_string());
	}

	return (indexEntries);
}

int
search(mportInstance *mport, char **query) {
	mportIndexEntry **indexEntry;

	if (query == NULL || *query == NULL) {
		fprintf(stderr, "Search terms required\n");
		return (1);
	}

	while (query != NULL && *query != NULL) {
		mport_index_search(mport, &indexEntry, "pkg glob %Q or comment glob %Q", *query, *query);
		if (indexEntry == NULL || *indexEntry == NULL) {
			query++;
			continue;
		}

		while (indexEntry != NULL && *indexEntry != NULL) {
			fprintf(stdout, "%s\t%s\t%s\n", (*indexEntry)->pkgname,
			        (*indexEntry)->version,
			        (*indexEntry)->comment);
			indexEntry++;
		}

		mport_index_entry_free_vec(indexEntry);
		query++;
	}

	return (0);
}

int
lock(mportInstance *mport, const char *packageName) {
	mportPackageMeta **packs;

	if (packageName == NULL) {
		warnx("%s", "Specify package name");
		return (1);
	}

	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	if (packs == NULL) {
		warnx("Package name not found, %s", packageName);
		return (1);
	} else {
		mport_lock_lock(mport, (*packs));
	}

	return (0);
}

int
unlock(mportInstance *mport, const char *packageName) {
	mportPackageMeta **packs;

	if (packageName == NULL) {
		warnx("%s", "Specify package name");
		return (1);
	}

	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	if (packs == NULL) {
		warnx("Package name not found, %s", packageName);
		return (1);
	} else {
		mport_lock_unlock(mport, (*packs));
	}

	return (0);
}

static int
stats(mportInstance *mport) {
	mportStats *s;
	if (mport_stats(mport, &s) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	printf("Local package database:\n");
	printf("\tInstalled packages: %d\n", s->pkg_installed);
	printf("\nRemote package database:\n");
	printf("\tPackages available: %d\n", s->pkg_available);

	return (0);
}

int
info(mportInstance *mport, const char *packageName) {
	if (packageName == NULL) {
		warnx("%s", "Specify package name");
		return (1);
	}

	char *out = mport_info(mport, packageName);
	if (out == NULL) {
		warnx("%s", mport_err_string());
		return (1);
	}

	printf("%s", out);
	free(out);

	return (0);
}

int
which(mportInstance *mport, const char *filePath, bool quiet, bool origin) {
	mportPackageMeta *pack = NULL;

	if (filePath == NULL) {
		warnx("%s", "Specify file path");
		return (1);
	}

	if (mport_asset_get_package_from_file_path(mport, filePath, &pack) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	if (pack != NULL && pack->origin != NULL) {
		if (quiet && origin) {
			printf("%s\n", pack->origin);
		} else if (quiet) {
			printf("%s-%s\n", pack->name, pack->version);
		} else if (origin) {
			printf("%s was installed by package %s\n", filePath, pack->origin);
		} else {
			printf("%s was installed by package %s-%s\n", filePath, pack->name, pack->version);
		}
	}

	return (0);
}

int
install(mportInstance *mport, const char *packageName) {
	mportIndexEntry **indexEntry;
	mportIndexEntry **i2;
	int resultCode;
	int item;
	int choice;

	indexEntry = lookupIndex(mport, packageName);
	if (*indexEntry == NULL) {
		int loc = -1;
		size_t len = strlen(packageName);
		for (int i = len -1; i >= 0; i--) {
			if (packageName[i] == '-') {
				loc = i;
				break;
			}
		}

		if (loc > 0) {
			char *d = strdup(packageName);
			char *v = &d[loc+1];
			d[loc] = '\0'; /* hack off the version number */
			indexEntry = lookupIndex(mport, d);
			if (strcmp(v, (*indexEntry)->version) != 0) {
				errx(4, "Package %s not found in the index.", packageName);	
			}
			free(d);
		}
	}

	if (indexEntry == NULL || *indexEntry == NULL)
		errx(4, "Package %s not found in the index.", packageName);

	if (indexEntry[1] != NULL) {
		printf("Multiple packages found. Please select one:\n");
		i2 = indexEntry;
		item = 0;
		while (*i2 != NULL) {
			printf("%d. %s-%s\n", item, (*i2)->pkgname, (*i2)->version);
			item++;
			i2++;
		}
		while (scanf("%d", &choice) < 1 || choice > item || choice < 0) {
			fprintf(stderr, "Please select an entry 0 - %d\n", item - 1);
		}
		item = 0;
		while (indexEntry != NULL) {
			if (item == choice)
				break;
			item++;
			indexEntry++;
		}
	}

	resultCode = mport_install_depends(mport, (*indexEntry)->pkgname, (*indexEntry)->version, MPORT_EXPLICIT);

	mport_index_entry_free_vec(indexEntry);

	return (resultCode);
}

int
delete(const char *packageName) {
	char *buf;
	int resultCode;

	asprintf(&buf, "%s%s %s %s", MPORT_TOOLS_PATH, "mport.delete", "-n", packageName);
	if (buf == NULL) {
		warnx("Out of memory.");
		return (1);
	}
	resultCode = system(buf);
	free(buf);

	return (resultCode);
}

int configGet(mportInstance *mport, const char *settingName) {
	char *val;

	val = mport_setting_get(mport, settingName);

	if (val != NULL) {
		printf("Setting %s value is %s\n", settingName, val);
	} else {
		printf("Setting %s is undefined.\n", settingName);
	}

	return 0;
}

int configSet(mportInstance *mport, const char *settingName, const char *val) {
	int result = mport_setting_set(mport, settingName, val);

	if (result != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	return 0;
}

int
cpeList(mportInstance *mport) {
	mportPackageMeta **packs;
	int cpe_total = 0;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	if (packs == NULL) {
		warnx("No packages installed.");
		return (1);
	}

	while (*packs != NULL) {
		if ((*packs)->cpe != NULL && strlen((*packs)->cpe) > 0) {
			printf("%s\n", (*packs)->cpe);
			cpe_total++;
		}
		packs++;
	}
	mport_pkgmeta_vec_free(packs);

	if (cpe_total == 0) {
		errx(EX_SOFTWARE, "No packages contained CPE information.");
	}

	return (0);
}

int
verify(mportInstance *mport) {
	mportPackageMeta **packs, **ref;
	int total = 0;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	if (packs == NULL) {
		warnx("No packages installed.");
		return (1);
	}
	ref = packs;

	while (*packs != NULL) {
		mport_verify_package(mport, *packs);
		packs++;
		total++;
	}

	mport_pkgmeta_vec_free(ref);
	printf("Packages verified: %d\n", total);

	return (0);
}

int
deleteAll(mportInstance *mport) {
	mportPackageMeta **packs;
	mportPackageMeta **depends;
	int total = 0;
	int errors = 0;
	int skip;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	if (packs == NULL) {
		fprintf(stderr, "No packages installed.\n");
		return (1);
	}

	while (1) {
		skip = 0;
		while (*packs != NULL) {
			if (mport_pkgmeta_get_updepends(mport, *packs, &depends) == MPORT_OK) {
				if (depends == NULL) {
					if (delete((*packs)->name) != 0) {
						fprintf(stderr, "Error deleting %s\n", (*packs)->name);
						errors++;
					}
					total++;
				} else {
					skip++;
					mport_pkgmeta_vec_free(depends);
				}
			}
			packs++;
		}
		if (skip == 0)
			break;
		mport_pkgmeta_vec_free(packs);
		if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
			warnx("%s", mport_err_string());
			return (1);
		}
	}

	mport_pkgmeta_vec_free(packs);

	printf("Packages deleted: %d\nErrors: %d\nTotal: %d\n", total - errors, errors, total);
	return (0);
}

int
clean(mportInstance *mport) {
	int ret;

	ret = mport_clean_database(mport);
	if (ret == MPORT_OK)
		ret = mport_clean_oldpackages(mport);
	return (ret);
}

