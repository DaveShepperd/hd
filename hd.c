/* It's a 32 bit app. It has to be in order to work under MinGW32. Under Linux, however, still being 32 bit but with access to 64 bit functions. */
/* These defines allow access to the 64 bit functions provided by libc on Linux. MinGW32 ignores this stuff and stays 32 bit. */
/* These 3 defines must appear before any #include directives */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
	#define _LARGEFILE64_SOURCE
#endif
#define _FILE_OFFSET_BITS (64)
#if _WIN32
	#undef __STRICT_ANSI__
	#define __need___off64_t
	#include <sys/types.h>
	#define EOLSTR "\r\n"
#else
	#define EOLSTR "\n"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

static const char hex2asc[] = "0123456789ABCDEF";
typedef struct
{
	FILE *ourStdOut;	/* Have to have this on Windows */
	FILE *ourStdErr;	/* Have to have this on Windows */
	off64_t start;
	off64_t skip;
	off64_t fileSize;
	int format;
	int endian;
	int head;
	int tail;
	int wide;
} CmdOptions_t;

typedef struct
{
	char **tailArray;   /* Pointer to array of pointers */
	int numTails;       /* Total number of entries in array */
	int inIndex;        /* Index to next entry to insert */
	int numHeads;       /* flag indicatinng there's a head count */
	int head;           /* Number of records output so far */
	int skipToEnd;      /* bool indicating to skip to end of file */
} Tail_t;

static int recordString(FILE *ofp, Tail_t *tailPtr, const char *msg)
{
	if (    !tailPtr->tailArray                         /* either no tail pointers */
		 || !tailPtr->numHeads							/* nor a head count */
		 || (tailPtr->numHeads && tailPtr->head > 0)    /* or have a remaining head count */
	   )
	{
		fputs(msg, ofp);                                /* Output the string */
		if ( tailPtr->numHeads )
		{
			--tailPtr->head;                            /* take from head count */
			if ( tailPtr->head <= 0 )
			{
				if ( tailPtr->tailArray )
				{
					/* There are tails to save. Skip the to the "end" of the file. */
					tailPtr->skipToEnd = 1;
					return 2;
				}
				return 1;					/* if ran out of heads and there's no tails wanted */
			}
		}
		return 0;               /* Need to output another line */
	}
	if ( tailPtr->tailArray )   /* If we are saving tails */
	{
		int sLen;
		char *cPtr;
		sLen = strlen(msg) + 1;         /* compute length of string */
		cPtr = (char *)malloc(sLen);    /* Get a buffer to save it */
		if ( cPtr )
		{
			memcpy(cPtr, msg, sLen - 1);   /* make a copy */
			cPtr[sLen-1] = 0;             /* make sure it is null terminated */
		}
		if ( tailPtr->tailArray[tailPtr->inIndex] )
			free(tailPtr->tailArray[tailPtr->inIndex]); /* free any existing tail buffer */
		tailPtr->tailArray[tailPtr->inIndex] = cPtr;    /* save the new buffer */
		++tailPtr->inIndex;             /* count it */
		if ( tailPtr->inIndex >= tailPtr->numTails ) /* make the count modulo the amount wanted */
			tailPtr->inIndex = 0;
	}
	return 0;       /* keep saving more tail strings */
}

void dump_bytes(FILE *ofp, FILE *ifp, CmdOptions_t *opts)
{
	char line[256], *strp;
	unsigned char *lp;
	int lineCnt, col, dups, sts, sLen;
	Tail_t tails;
	unsigned char newBuf[32];
	unsigned char oldBuf[sizeof(newBuf)];
	int bufSize = opts->wide ? sizeof(newBuf) : sizeof(newBuf) / 2;

	memset(&tails, 0, sizeof(tails));         /* Start with all 0's */
	tails.numHeads = opts->head;            /* init the head line count */
	tails.head = opts->head;                /* two copies */
	if ( (tails.numTails = opts->tail) )      /* if he wants tails */
		tails.tailArray = (char **)calloc(opts->tail, sizeof(char *));   /* make an array to hold pointers */
	for ( dups = lineCnt = 0;; ++lineCnt, opts->start += bufSize ) /* for each line */
	{
		if ( tails.skipToEnd && tails.tailArray )
		{
			off64_t tailStart;
			tailStart = (opts->fileSize - bufSize * opts->tail) & -bufSize; /* backup n lines rounded down to multple of bufsize */
			if ( tailStart > opts->start )
			{
				int ssts;
#if _WIN32
				ssts = fseek(ifp, tailStart, SEEK_SET);
#else
				ssts = fseeko(ifp, tailStart, SEEK_SET);
#endif
				if ( ssts < 0 )
				{
					fprintf(opts->ourStdErr, "Error seeking to tail 0x%llX: %s%s", tailStart, strerror(errno),EOLSTR);
					return;
				}
				opts->start = tailStart;
			}
			tails.skipToEnd = 0;
		}
		sts = fread(newBuf, 1, bufSize, ifp);
		if ( lineCnt && sts == bufSize && memcmp(oldBuf, newBuf, bufSize) == 0 )
		{
			if ( !tails.numHeads || tails.head > 1 )
			{
				--tails.head;
				++dups;
				continue;
			}
		}
		if ( dups )
		{
			if ( opts->fileSize >= 0x10000LL || (opts->start - bufSize) >= 0x10000LL )
			{
				if ( opts->fileSize >= 0x100000000LL || (opts->start - bufSize) >= 0x100000000LL  )
				{
					if ( dups > 1 )
						recordString(ofp, &tails, "****************" EOLSTR);
					snprintf(line, sizeof(line), "%016llX", opts->start - bufSize);
					line[16] = ' ';
				}
				else
				{
					if ( dups > 1 )
						recordString(ofp, &tails, "********" EOLSTR);
					snprintf(line, sizeof(line), "%08llX", opts->start - bufSize);
					line[8] = ' ';
				}
			}
			else
			{
				if ( dups > 1 )
					recordString(ofp, &tails, "****" EOLSTR);
				snprintf(line, sizeof(line), "%04llX", opts->start - bufSize);
				line[4] = ' ';
			}
			recordString(ofp, &tails, line);
			dups = 0;
		}
		if ( sts <= 0 )
			break;
		memcpy(oldBuf, newBuf, bufSize);
		if ( opts->start >= 0x100000000LL || opts->fileSize >= 0x100000000LL )
		{
			sLen = snprintf(line, sizeof(line), "%016llX  ", opts->start);
		}
		else if ( opts->start >= 0x10000LL || opts->fileSize >= 0x10000LL )
		{
			sLen = snprintf(line, sizeof(line), "%08llX  ", opts->start);
		}
		else
		{
			sLen = snprintf(line, sizeof(line), "%04llX  ", opts->start);
		}
		strp = line + sLen;
		switch (opts->format)
		{
		default:
		case 1:
			for ( col = 0, lp = newBuf; col < sts; ++col, ++lp )
			{
				if ( col && !(col & 0x7) )
					*strp++ = ' ';
				*strp++ = hex2asc[(*lp >> 4) & 0xF];
				*strp++ = hex2asc[*lp & 0xF];
				*strp++ = ' ';
			}
			break;
		case 2:
			for ( col = 0, lp = newBuf; col < sts; col += 2, lp += 2 )
			{
				int high, low;
				if ( col && !(col & 0x7) )
					*strp++ = ' ';
				low  = lp[opts->endian];
				high = lp[opts->endian ^ 1];
				*strp++ = hex2asc[(high >> 4) & 0xF];
				*strp++ = hex2asc[(high)&0xF];
				*strp++ = hex2asc[(low >> 4) & 0xF];
				*strp++ = hex2asc[(low)&0xF];
				*strp++ = ' ';
			}
			break;
		case 4:
			for ( col = 0, lp = newBuf; col < sts; col += 4 )
			{
				int hh, hl, lh, ll;
				if ( col && !(col & 0x7) )
					*strp++ = ' ';
				if ( !opts->endian )
				{
					ll = *lp++;
					lh = *lp++;
					hl = *lp++;
					hh = *lp++;
				}
				else
				{
					lp += 3;
					ll = *lp;
					lh = *--lp;
					hl = *--lp;
					hh = *--lp;
					lp += 4;
				}
				*strp++ = hex2asc[(hh >> 4) & 0xF];
				*strp++ = hex2asc[(hh)&0xF];
				*strp++ = hex2asc[(hl >> 4) & 0xF];
				*strp++ = hex2asc[(hl)&0xF];
				*strp++ = hex2asc[(lh >> 4) & 0xF];
				*strp++ = hex2asc[(lh)&0xF];
				*strp++ = hex2asc[(ll >> 4) & 0xF];
				*strp++ = hex2asc[(ll)&0xF];
				*strp++ = ' ';
			}
			break;
		}
		if ( col < bufSize )
		{
			int skipAmt;
			switch (opts->format)
			{
			default:
			case 1:
				skipAmt = (bufSize - col) * 3;      /* Number of positions left on line */
				break;
			case 2:
				skipAmt = ((bufSize - col) / 2) * 5;
				break;
			case 4:
				skipAmt = ((bufSize - col) / 4) * 9;
				break;
			}
			skipAmt += (bufSize - col) / 8;
			memset(strp, ' ', skipAmt);
			strp += skipAmt;
		}
		*strp++ = ' ';
		*strp++ = '|';
		for ( col = 0, lp = newBuf; col < sts; ++col, ++lp )
		{
			if ( isprint(*lp) )
				*strp++ = *lp;
			else
				*strp++ = '.';
		}
		if ( col < bufSize )
		{
			memset(strp, ' ', bufSize - col);
			strp += bufSize - col;
		}
		*strp++ = '|';
#if _WIN32
		*strp++ = '\r';
#endif
		*strp++ = '\n';
		*strp = 0;
		if ( recordString(ofp, &tails, line) )    /* output and save if collecting tails */
		{
			if ( tails.skipToEnd )
				continue;
			break;
		}
	}
	if ( tails.tailArray )          /* if we've got some tails saved */
	{
		int first, idx, limit = tails.inIndex;
		idx = limit;
		first = tails.numHeads ? 1 : 0;
		do                          /* for each item in the save array */
		{
			char *cPtr;
			cPtr = tails.tailArray[idx];
			if ( cPtr )
			{
				if ( first )
				{
					/* Put a separator line between the heads and the first tail */
					if ( cPtr[0] != '*' )
					{
						if ( cPtr[4] == ' ' )
							fputs("****" EOLSTR, ofp);
						else if ( cPtr[8] == ' ' )
							fputs("********" EOLSTR, ofp);
						else
							fputs("****************" EOLSTR, ofp);
					}
					first = 0;
				}
				fputs(cPtr, ofp);
				free(cPtr);
				tails.tailArray[idx] = NULL;
			}
			++idx;
			if ( idx >= tails.numTails )
				idx = 0;
		} while ( idx != limit );
	}
}

static int helpEm(FILE *ofp)
{
	fprintf(ofp,"Usage: hd [-bhwB?] [--head=n] [--tail=n] [--skip=n] file [... file]" EOLSTR
		   "where:\n"
		   "--bytes  or -b   print in bytes (default)" EOLSTR
		   "--shorts or -h   print in halfwords (16 bits)" EOLSTR
		   "--longs  or -w   print in words (32 bits)" EOLSTR
		   "--big    or -B   print assuming Big Endian" EOLSTR
		   "--head=n or -Hn  print only 'n' leading lines" EOLSTR
		   "--tail=n or -Tm  print only 'n' last lines" EOLSTR
		   "--skip=n or -Sn  first skip 'n' bytes on all inputs" EOLSTR
		   "--wide   or -W   set number of columns to 32. (Default is 16.)" EOLSTR
		   "--out=file       reassign stdout to 'file'" EOLSTR
		   "--err=file       reassign stderr to 'file'" EOLSTR
		   "--help   or -?   this message" EOLSTR
		  );
	return 1;
}

typedef enum
{
	OPT_BYTES,
	OPT_SHORTS,
	OPT_WORDS,
	OPT_BIGENDIAN,
	OPT_HEAD,
	OPT_TAIL,
	OPT_WIDE,
	OPT_SKIP,
	OPT_HELP,
	OPT_STDOUT,
	OPT_STDERR,
	OPT_MAX
} CmdOpts_t;

int main(int argc, char *argv[])
{
	int multipleFiles;
	int fileCnt;
	char *endp;
	CmdOptions_t opts;
	const char *stdOutName=NULL, *stdErrName=NULL;
	static const struct option longOpts[] =
	{
		{ "bytes", no_argument, NULL, OPT_BYTES },
		{ "shorts", no_argument, NULL, OPT_SHORTS },
		{ "longs", no_argument, NULL, OPT_WORDS },
		{ "big", no_argument, NULL, OPT_BIGENDIAN },
		{ "head", required_argument, NULL, OPT_HEAD },
		{ "wide", no_argument, NULL, OPT_WIDE },
		{ "tail", required_argument, NULL, OPT_TAIL },
		{ "skip", required_argument, NULL, OPT_SKIP },
		{ "out", required_argument, NULL, OPT_STDOUT },
		{ "err", required_argument, NULL, OPT_STDERR },
		{ NULL, 0, NULL, 0 }
	};

	memset(&opts, 0, sizeof(opts));
	opts.format = 1;
	while ( 1 )
	{
		int cc;

		opterr = 0;
		cc = getopt_long(argc, argv, "?bBhH:S:T:wW", longOpts, NULL);

		if ( cc == -1 )
			break;

		switch (cc)
		{
		case OPT_BIGENDIAN:
		case 'B':
			opts.endian = 1;
			break;
		case OPT_BYTES:
		case 'b':
			opts.format = 1;
			break;
		case OPT_SHORTS:
		case 'h':
			opts.format = 2;
			break;
		case OPT_WORDS:
		case 'w':
			opts.format = 4;
			break;
		case OPT_HEAD:
		case 'H':
			endp = NULL;
			opts.head = strtol(optarg, &endp, 0);
			if ( !endp || *endp || opts.head <= 0 )
			{
				fprintf(opts.ourStdErr, "Invalid --head option: %s" EOLSTR, optarg);
				return helpEm(opts.ourStdErr);
			}
			break;
		case OPT_TAIL:
		case 'T':
			endp = NULL;
			opts.tail = strtol(optarg, &endp, 0);
			if ( !endp || *endp || opts.tail <= 0 )
			{
				fprintf(opts.ourStdErr, "Invalid --tail option: %s" EOLSTR, optarg);
				return helpEm(opts.ourStdErr);
			}
			break;
		case OPT_SKIP:
		case 'S':
			endp = NULL;
			opts.skip = strtoll(optarg, &endp, 0);
			if ( !endp || *endp || opts.skip < 0 )
			{
				fprintf(opts.ourStdErr, "Invalid --skip option: %s" EOLSTR, optarg);
				return helpEm(opts.ourStdErr);
			}
			break;
		case OPT_WIDE:
		case 'W':
			opts.wide = 1;
			break;
		case OPT_STDOUT:
			stdOutName = optarg;
			break;
		case OPT_STDERR:
			stdErrName = optarg;
			break;
		case '?':
		default:
			return helpEm(opts.ourStdErr);
		}
	}
	if ( stdOutName )
		opts.ourStdOut = fopen(stdOutName,"wb");
	if ( !opts.ourStdOut )
		opts.ourStdOut = stdout;
	if ( stdErrName )
		opts.ourStdErr = fopen(stdErrName,"wb");
	if ( !opts.ourStdErr )
		opts.ourStdErr = stderr;
	if ( optind >= argc )
		return helpEm(opts.ourStdErr);
	if ( opts.skip && !opts.head && opts.tail )
		fprintf(opts.ourStdErr, "Warning: --skip is ignored if --tail is present without --head" EOLSTR);
	multipleFiles = optind + 1 < argc;
#if 0 && _WIN32
	fprintf(opts.ourStdOut,"head=%d, tail=%d, numFiles=%d, format=%d, endian=%s, wide=%d, skip=0x%llX" EOLSTR,
		   opts.head,
		   opts.tail,
		   argc - optind,
		   opts.format,
		   opts.endian ? "Big" : "Little",
		   opts.wide,
		   opts.skip);
#endif
	for ( fileCnt = 0; optind < argc; ++optind, ++fileCnt )
	{
		FILE *ifp;
		int sts;
#if _WIN32
		struct __stat64 st;
		sts = _stat64(argv[optind], &st);
#else
		struct stat st;
		sts = stat(argv[optind], &st);
#endif
		if ( sts < 0 )
		{
			fprintf(opts.ourStdErr, "Error stat()'ing '%s': %s" EOLSTR, argv[optind], strerror(errno));
			return 1;
		}
		opts.fileSize = st.st_size;
#if _WIN32
		if ( opts.fileSize >= 0x80000000LL )
		{
			fprintf(opts.ourStdErr, "Sorry, file %s is too big." EOLSTR, argv[optind]);
			continue;
		}
#endif
		ifp = fopen(argv[optind], "rb");
		if ( !ifp )
		{
			fprintf(opts.ourStdErr, "Error opening %s: %s" EOLSTR, argv[optind], strerror(errno));
			if ( !multipleFiles )
				return 3;
			continue;
		}
		if ( (opts.start = opts.skip) )
		{
			off64_t ssts;

			if ( opts.skip > opts.fileSize )
			{
				fprintf(opts.ourStdErr, "Error: Cannot skip to 0x%llX on %s. It has a size of 0x%llX" EOLSTR, opts.skip, argv[optind], opts.fileSize);
				fclose(ifp);
				continue;
			}
#if _WIN32
			ssts = fseek(ifp, opts.skip, SEEK_SET);
#else
			ssts = fseeko(ifp, opts.skip, SEEK_SET);
#endif
			if ( ssts < 0 )
			{
				fprintf(opts.ourStdErr, "Error doing fseek(%s,0x%llX,SEEK_SET): %s" EOLSTR, argv[optind], opts.skip, strerror(errno));
				fclose(ifp);
				continue;
			}
		}
		if ( opts.tail && !opts.head )
		{
			off64_t tailStart;
			off64_t ssts;
			int cols = (opts.wide ? 32 : 16);
			/* No head count but there's a tail count. Skip to "end" of file. */
			tailStart = (opts.fileSize - cols * opts.tail) & -cols;
			if ( tailStart > opts.start )
			{
#if _WIN32
				ssts = fseek(ifp, tailStart, SEEK_SET);
#else
				ssts = fseeko(ifp, tailStart, SEEK_SET);
#endif
				if ( ssts < 0 )
				{
					fprintf(opts.ourStdErr, "Error doing fseek(%s,0x%llX,SEEK_SET): %s" EOLSTR, argv[optind], tailStart, strerror(errno));
					fclose(ifp);
					continue;
				}
				opts.start = tailStart;
			}
		}
		if ( multipleFiles )
			fprintf(opts.ourStdOut,"%s%s:" EOLSTR, fileCnt ? EOLSTR : "", argv[optind]);
		dump_bytes(opts.ourStdOut, ifp, &opts);
		fclose(ifp);
	}
	return 0;
}
