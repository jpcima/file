/*
 * file - find type of a file or files - main program.
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include "magic.h"
#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>	/* for MAXPATHLEN */
#include <sys/stat.h>
#include <fcntl.h>	/* for open() */
#ifdef RESTORE_TIME
# if (__COHERENT__ >= 0x420)
#  include <sys/utime.h>
# else
#  ifdef USE_UTIMES
#   include <sys/time.h>
#  else
#   include <utime.h>
#  endif
# endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>	/* for read() */
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>	/* for long options (is this portable?)*/
#endif

#include <netinet/in.h>		/* for byte swapping */

#include "patchlevel.h"

#ifndef	lint
FILE_RCSID("@(#)$Id: file.c,v 1.70 2003/03/23 04:06:05 christos Exp $")
#endif	/* lint */


#ifdef S_IFLNK
# define USAGE  "Usage: %s [-bciknsvzL] [-f namefile] [-m magicfiles] file...\n"
#else
# define USAGE  "Usage: %s [-bciknsvz] [-f namefile] [-m magicfiles] file...\n"
#endif

#ifdef __EMX__
private char *apptypeName = NULL;
int os2_apptype (const char *fn, char *buf, int nb);
#endif /* __EMX__ */

#ifndef MAGIC
# define MAGIC "/etc/magic"
#endif

#ifndef MAXPATHLEN
#define	MAXPATHLEN	512
#endif

private int 		/* Global command-line options 		*/
	bflag = 0,	/* brief output format	 		*/
	nopad = 0,	/* Don't pad output			*/
	nobuffer = 0,   /* Do not buffer stdout 		*/
	kflag = 0;	/* Keep going after the first match	*/

private const char *magicfile = 0;	/* where the magic is	*/
private const char *default_magicfile = MAGIC;
private char separator = ':';	/* Default field separator	*/

private char *progname;		/* used throughout 		*/

private struct magic_set *magic;

private void unwrap(char *);
private void usage(void);
#ifdef HAVE_GETOPT_H
private void help(void);
#endif
#if 0
private int byteconv4(int, int, int);
private short byteconv2(int, int, int);
#endif

int main(int, char *[]);
private void process(const char *, int);
private void load(const char *, int);


/*
 * main - parse arguments and handle options
 */
int
main(int argc, char *argv[])
{
	int c;
	int action = 0, didsomefiles = 0, errflg = 0;
	int flags = 0;
	char *mime, *home, *usermagic;
	struct stat sb;
#define OPTSTRING	"bcdf:F:ikm:nNsvzCL"
#ifdef HAVE_GETOPT_LONG
	int longindex;
	private struct option long_options[] =
	{
		{"version", 0, 0, 'v'},
		{"help", 0, 0, 0},
		{"brief", 0, 0, 'b'},
		{"checking-printout", 0, 0, 'c'},
		{"debug", 0, 0, 'd'},
		{"files-from", 1, 0, 'f'},
		{"separator", 1, 0, 'F'},
		{"mime", 0, 0, 'i'},
		{"keep-going", 0, 0, 'k'},
#ifdef S_IFLNK
		{"dereference", 0, 0, 'L'},
#endif
		{"magic-file", 1, 0, 'm'},
		{"uncompress", 0, 0, 'z'},
		{"no-buffer", 0, 0, 'n'},
		{"no-pad", 0, 0, 'N'},
		{"special-files", 0, 0, 's'},
		{"compile", 0, 0, 'C'},
		{0, 0, 0, 0},
	};
#endif

#ifdef LC_CTYPE
	setlocale(LC_CTYPE, ""); /* makes islower etc work for other langs */
#endif

#ifdef __EMX__
	/* sh-like wildcard expansion! Shouldn't hurt at least ... */
	_wildcard(&argc, &argv);
#endif

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];

	magicfile = default_magicfile;
	if ((usermagic = getenv("MAGIC")) != NULL)
		magicfile = usermagic;
	else
		if ((home = getenv("HOME")) != NULL) {
			if ((usermagic = malloc(strlen(home) + 8)) != NULL) {
				(void)strcpy(usermagic, home);
				(void)strcat(usermagic, "/.magic");
				if (stat(usermagic, &sb)<0) 
					free(usermagic);
				else
					magicfile = usermagic;
			}
		}

#ifndef HAVE_GETOPT_LONG
	while ((c = getopt(argc, argv, OPTSTRING)) != -1)
#else
	while ((c = getopt_long(argc, argv, OPTSTRING, long_options,
	    &longindex)) != -1)
#endif
		switch (c) {
#ifdef HAVE_GETOPT_LONG
		case 0 :
			if (longindex == 1)
				help();
			break;
#endif
		case 'b':
			++bflag;
			break;
		case 'c':
			action = CHECK;
			break;
		case 'C':
			action = COMPILE;
			break;
		case 'd':
			flags |= MAGIC_DEBUG;
			break;
		case 'f':
			if(action)
				usage();
			load(magicfile, flags);
			unwrap(optarg);
			++didsomefiles;
			break;
		case 'F':
			separator = *optarg;
			break;
		case 'i':
			flags |= MAGIC_MIME;
			if ((mime = malloc(strlen(magicfile) + 6)) != NULL) {
				(void)strcpy(mime, magicfile);
				(void)strcat(mime, ".mime");
				magicfile = mime;
			}
			break;
		case 'k':
			kflag = 1;
			break;
		case 'm':
			magicfile = optarg;
			break;
		case 'n':
			++nobuffer;
			break;
		case 'N':
			++nopad;
			break;
		case 's':
			flags |= MAGIC_DEVICES;
			break;
		case 'v':
			(void) fprintf(stdout, "%s-%d.%d\n", progname,
				       FILE_VERSION_MAJOR, patchlevel);
			(void) fprintf(stdout, "magic file from %s\n",
				       magicfile);
			return 1;
		case 'z':
			flags |= MAGIC_COMPRESS;
			break;
#ifdef S_IFLNK
		case 'L':
			flags |= MAGIC_SYMLINK;
			break;
#endif
		case '?':
		default:
			errflg++;
			break;
		}

	if (errflg) {
		usage();
	}

	switch(action) {
	case CHECK:
	case COMPILE:
		magic = magic_open(flags);
		if (magic == NULL) {
			(void)fprintf(stderr, "%s: %s\n", progname,
			    strerror(errno));
			return 1;
		}
		return action == CHECK ? magic_check(magic, magicfile) :
		    magic_compile(magic, magicfile);
	default:
		load(magicfile, flags);
		break;
	}

	if (optind == argc) {
		if (!didsomefiles) {
			usage();
		}
	}
	else {
		int i, wid, nw;
		for (wid = 0, i = optind; i < argc; i++) {
			nw = strlen(argv[i]);
			if (nw > wid)
				wid = nw;
		}
		for (; optind < argc; optind++)
			process(argv[optind], wid);
	}

	return 0;
}


private void
load(const char *m, int flags)
{
	if (magic)
		return;
	magic = magic_open(flags);
	if (magic == NULL) {
		(void)fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		exit(1);
	}
	if (magic_load(magic, magicfile) == -1) {
		(void)fprintf(stderr, "%s: %s\n",
		    progname, magic_error(magic));
		exit(1);
	}
}

/*
 * unwrap -- read a file of filenames, do each one.
 */
private void
unwrap(char *fn)
{
	char buf[MAXPATHLEN];
	FILE *f;
	int wid = 0, cwid;

	if (strcmp("-", fn) == 0) {
		f = stdin;
		wid = 1;
	} else {
		if ((f = fopen(fn, "r")) == NULL) {
			(void)fprintf(stderr, "%s: Cannot open `%s' (%s).\n",
			    progname, fn, strerror(errno));
			exit(1);
		}

		while (fgets(buf, MAXPATHLEN, f) != NULL) {
			cwid = strlen(buf) - 1;
			if (cwid > wid)
				wid = cwid;
		}

		rewind(f);
	}

	while (fgets(buf, MAXPATHLEN, f) != NULL) {
		buf[strlen(buf)-1] = '\0';
		process(buf, wid);
		if(nobuffer)
			(void) fflush(stdout);
	}

	(void) fclose(f);
}

private void
process(const char *inname, int wid)
{
	const char *type;

	if (wid > 0 && !bflag)
		(void) printf("%s%c%*s ", inname, separator,
		    (int) (nopad ? 0 : (wid - strlen(inname))), "");

	type = magic_file(magic, inname);
	if (type == NULL)
		printf("ERROR: %s\n", magic_error(magic));
	else
		printf("%s\n", type);
}


#if 0
/*
 * byteconv4
 * Input:
 *	from		4 byte quantity to convert
 *	same		whether to perform byte swapping
 *	big_endian	whether we are a big endian host
 */
private int
byteconv4(int from, int same, int big_endian)
{
	if (same)
		return from;
	else if (big_endian) {		/* lsb -> msb conversion on msb */
		union {
			int i;
			char c[4];
		} retval, tmpval;

		tmpval.i = from;
		retval.c[0] = tmpval.c[3];
		retval.c[1] = tmpval.c[2];
		retval.c[2] = tmpval.c[1];
		retval.c[3] = tmpval.c[0];

		return retval.i;
	}
	else
		return ntohl(from);	/* msb -> lsb conversion on lsb */
}

/*
 * byteconv2
 * Same as byteconv4, but for shorts
 */
private short
byteconv2(int from, int same, int big_endian)
{
	if (same)
		return from;
	else if (big_endian) {		/* lsb -> msb conversion on msb */
		union {
			short s;
			char c[2];
		} retval, tmpval;

		tmpval.s = (short) from;
		retval.c[0] = tmpval.c[1];
		retval.c[1] = tmpval.c[0];

		return retval.s;
	}
	else
		return ntohs(from);	/* msb -> lsb conversion on lsb */
}
#endif

private void
usage(void)
{
	(void)fprintf(stderr, USAGE, progname);
	(void)fprintf(stderr, "Usage: %s -C [-m magic]\n", progname);
#ifdef HAVE_GETOPT_H
	(void)fputs("Try `file --help' for more information.\n", stderr);
#endif
	exit(1);
}

#ifdef HAVE_GETOPT_H
private void
help(void)
{
	puts(
"Usage: file [OPTION]... [FILE]...\n"
"Determine file type of FILEs.\n"
"\n"
"  -m, --magic-file LIST      use LIST as a colon-separated list of magic\n"
"                               number files\n"
"  -z, --uncompress           try to look inside compressed files\n"
"  -b, --brief                do not prepend filenames to output lines\n"
"  -c, --checking-printout    print the parsed form of the magic file, use in\n"
"                               conjunction with -m to debug a new magic file\n"
"                               before installing it\n"
"  -f, --files-from FILE      read the filenames to be examined from FILE\n"
"  -F, --separator char       use char as separator instead of `:'\n"
"  -i, --mime                 output mime type strings\n"
"  -k, --keep-going           don't stop at the first match\n"
"  -L, --dereference          causes symlinks to be followed\n"
"  -n, --no-buffer            do not buffer output\n"
"  -N, --no-pad               do not pad output\n"
"  -s, --special-files        treat special (block/char devices) files as\n"
"                             ordinary ones\n"
"      --help                 display this help and exit\n"
"      --version              output version information and exit\n"
);
	exit(0);
}
#endif
