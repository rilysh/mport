/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2010, 2011, 2013, 2014 Lucas Holt
 * Copyright (c) 2008 Chris Reinhardt
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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <mport.h>

static void usage(void);
static char * str_remove(const char *, const char);

int 
main(int argc, char *argv[]) 
{
	int ch;
	mportInstance *mport;
	mportPackageMeta **packs;
	mportIndexEntry **indexEntries;
	mportIndexEntry **iestart;
	bool quiet = false;
	bool verbose = false;
	bool origin = false;
	bool update = false;
	bool locks = false;
    bool prime = false;
	char *comment;
	char *os_release;
	char name_version[30];
	const char *chroot_path = NULL;
	
	if (argc > 3)
		usage();
    
	while ((ch = getopt(argc, argv, "c:lopqvu")) != -1) {
		switch (ch) {
			case 'c':
				chroot_path = optarg;
				break;
			case 'l':
				locks = true;
				break;
			case 'o':
				origin = true;
				break;
            case 'p':
                prime = true;
                break;
			case 'q':
				quiet = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'u':
				update = true;
				break; 
			case '?':
			default:
				usage();
				break; 
		}
	}

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}
	
	mport = mport_instance_new();
	if (mport_instance_init(mport, NULL, NULL, false) != MPORT_OK) {
		warnx("%s", mport_err_string());
		exit(EXIT_FAILURE);
	}

	os_release = mport_get_osrelease(mport);

	if (update && mport_index_load(mport) != MPORT_OK) {
                warnx("Unable to load updates index, %s", mport_err_string());
		exit(8);
	}

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		mport_instance_free(mport);
		exit(EXIT_FAILURE);
	}
	
	if (packs == NULL) {
		if (!quiet)
			warnx("No packages installed matching.");
		mport_instance_free(mport);
		exit(3);
	}
	
	while (*packs != NULL) {
		if (update) {
			if (mport_index_lookup_pkgname(mport, (*packs)->name, &indexEntries) != MPORT_OK) {
				(void) fprintf(stderr, "Error Looking up package name %s: %d %s\n", (*packs)->name,  mport_err_code(), mport_err_string());
				exit(mport_err_code());
			}

			if (indexEntries == NULL || *indexEntries == NULL) {
				(void) printf("%-15s %8s is no longer available.\n", (*packs)->name, (*packs)->version);
				packs++;
				continue;
			}
	
			iestart = indexEntries;		
			while (*indexEntries != NULL) {
				if (((*indexEntries)->version != NULL && mport_version_cmp((*packs)->version, (*indexEntries)->version) < 0) 
					|| ((*packs)->version != NULL && mport_version_cmp((*packs)->os_release, os_release) < 0)) {

                        if (verbose) {
                            (void) printf("%-15s %8s (%s)  <  %-s\n", (*packs)->name, (*packs)->version,
                                          (*packs)->os_release, (*indexEntries)->version);
                        } else {
                            (void) printf("%-15s %8s  <  %-8s\n", (*packs)->name, (*packs)->version,
                                          (*indexEntries)->version);
                        }
				}
				indexEntries++;
			}
				
			mport_index_entry_free_vec(iestart);
			indexEntries = NULL;
		} else if (verbose) {
			comment = str_remove((*packs)->comment, '\\');
			snprintf(name_version, 30, "%s-%s", (*packs)->name, (*packs)->version);
			
			(void) printf("%-30s\t%6s\t%s\n", name_version, (*packs)->os_release, comment);
			free(comment);
		}
        else if (prime && (*packs)->automatic == 0)
            (void) printf("%s\n", (*packs)->name);
		else if (quiet && !origin)
			(void) printf("%s\n", (*packs)->name);
		else if (quiet && origin)
			(void) printf("%s\n", (*packs)->origin);
		else if (origin)
			(void) printf("Information for %s-%s:\n\nOrigin:\n%s\n\n",
						  (*packs)->name, (*packs)->version, (*packs)->origin);
		else if (locks) {
			if ((*packs)->locked == 1)
				(void) printf("%s-%s\n", (*packs)->name, (*packs)->version);

		} else
			(void) printf("%s-%s\n", (*packs)->name, (*packs)->version);
		packs++;
	}
	
	mport_instance_free(mport); 
	
	return (0);
}


static char * 
str_remove( const char *str, const char ch )
{
	size_t i;
	size_t x;
	size_t len;
	char *output;
	
	if (str == NULL)
		return NULL;
	
	len = strlen(str);
	
	output = calloc(len + 1, sizeof(char));
	
	for (i = 0, x = 0; i <= len; i++) {
		if (str[i] != ch) {
			output[x] = str[i];
			x++;
		}
    }
    output[len] = '\0';
	
    return (output);
} 


static void 
usage(void) 
{

	fprintf(stderr, "Usage: mport.list [-q | -v | -u | -c <chroot path>]\n");

	exit(2);
}
