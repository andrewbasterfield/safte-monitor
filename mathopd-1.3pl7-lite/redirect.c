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

/* De verpletterende werkelijkheid */

//static const char rcsid[] = "$Id: redirect.c,v 1.1.1.1 2002/04/20 10:26:45 mclark Exp $";

#include "mathopd.h"

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif

int process_redirect(struct request *r)
{
	char buf[STRLEN];
	char *c;
	FILE *fp;

	if (r->method != M_GET && r->method != M_HEAD) {
		r->error = "invalid method for redirect";
		return 405;
	}
	fp = fopen(r->path_translated, "r");
	if (fp == 0) {
		lerror("fopen");
		r->error = "cannot open redirect file";
		return 500;
	}
	fgets(buf, STRLEN, fp);
	fclose(fp);
	c = strchr(buf, '\n');
	if (c)
		*c = 0;
	else {
		r->error = "redirect url too long";
		return 500;
	}
	escape_url(buf, r->newloc);
	r->location = r->newloc;
	return 302;
}
