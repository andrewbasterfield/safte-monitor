/*
 *   Copyright 1996, 1997, 1998, 1999 Michiel Boland.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *
 *   1. Redistributions of source code must retain the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials
 *      provided with the distribution.
 *
 *   3. The name of the author may not be used to endorse or promote
 *      products derived from this software without specific prior
 *      written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 *   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 *   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *   IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *   THE POSSIBILITY OF SUCH DAMAGE.
 */

/* San Jacinto */

//static const char rcsid[] = "$Id: util.c,v 1.1.1.1 2002/04/20 10:26:45 mclark Exp $";

#include "mathopd.h"

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif

static const char hex[] = "0123456789abcdef";

#define hexdigit(x) (((x) <= '9') ? (x) - '0' : ((x) & 7) + 9)

void escape_url(const char *from, char *to)
{
	register char c;

	while ((c = *from++) != 0) {
		switch (c) {
		case '%':
		case ' ':
		case '?':
		case '+':
		case '&':
			*to++ = '%';
			*to++ = hex[(c >> 4) & 15];
			*to++ = hex[c & 15];
			break;
		default:
			*to++ = c;
			break;
		}
	}
	*to = 0;
}

int unescape_url(const char *from, char *to)
{
	register char c, x1, x2;

	while ((c = *from++) != 0) {
		if (c == '%') {
			x1 = *from++;
			if (!isxdigit(x1))
				return -1;
			x2 = *from++;
			if (!isxdigit(x2))
				return -1;
			*to++ = (hexdigit(x1) << 4) + hexdigit(x2);
		} else
			*to++ = c;
	}
	*to = 0;
	return 0;
}

int unescape_url_n(const char *from, char *to, size_t n)
{
	register char c, x1, x2;

	while (n-- && (c = *from++) != 0) {
		if (c == '%') {
			x1 = *from++;
			if (!isxdigit(x1))
				return -1;
			x2 = *from++;
			if (!isxdigit(x2))
				return -1;
			*to++ = (hexdigit(x1) << 4) + hexdigit(x2);
		} else
			*to++ = c;
	}
	*to = 0;
	return 0;
}
