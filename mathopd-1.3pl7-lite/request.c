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

/* Mysterons */

//static const char rcsid[] = "$Id: request.c,v 1.3 2004/01/21 14:58:41 mclark Exp $";

#include "mathopd.h"

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif

static const char br_bad_url[] =		"bad or missing url";
static const char br_bad_protocol[] =		"bad protocol";
static const char br_bad_date[] =		"bad date";
static const char br_bad_path_name[] =		"bad path name";
static const char fb_not_plain[] =		"file not plain";
static const char fb_symlink[] =		"symlink spotted";
static const char fb_active[] =			"actively forbidden";
static const char fb_access[] =			"no permission";
static const char fb_post_file[] =		"POST to file";
static const char ni_not_implemented[] =	"method not implemented";
static const char se_alias[] =			"cannot resolve pathname";
static const char se_get_path_info[] =		"cannot determine path args";
static const char se_no_class[] =		"unknown class!?";
static const char se_no_mime[] =		"no MIME type";
static const char se_no_specialty[] =		"unconfigured specialty";
static const char se_no_virtual[] =		"virtual server does not exist";
static const char se_open[] =			"open failed";
static const char su_open[] =			"too many open files";
static const char ni_version_not_supp[] =	"version not supported";

static const char m_get[] =			"GET";
static const char m_head[] =			"HEAD";
static const char m_post[] =			"POST";

static time_t timerfc(char *s)
{
	static const int daytab[2][12] = {
		{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
		{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
	};
	unsigned sec, min, hour, day, mon, year;
	char month[3];
	register char c;
	register unsigned n;
	register char flag;
	register char state;
	register char isctime;
	enum { D_START, D_END, D_MON, D_DAY, D_YEAR, D_HOUR, D_MIN, D_SEC };

	sec = 60;
	min = 60;
	hour = 24;
	day = 32;
	year = 1969;
	isctime = 0;
	month[0] = 0;
	state = D_START;
	n = 0;
	flag = 1;
	do {
		c = *s++;
		switch (state) {
		case D_START:
			if (c == ' ') {
				state = D_MON;
				isctime = 1;
			} else if (c == ',') state = D_DAY;
			break;
		case D_MON:
			if (isalpha(c)) {
				if (n < 3) month[n++] = c;
			} else {
				if (n < 3) return -1;
				n = 0;
				state = isctime ? D_DAY : D_YEAR;
			}
			break;
		case D_DAY:
			if (c == ' ' && flag)
				;
			else if (isdigit(c)) {
				flag = 0;
				n = 10 * n + (c - '0');
			} else {
				day = n;
				n = 0;
				state = isctime ? D_HOUR : D_MON;
			}
			break;
		case D_YEAR:
			if (isdigit(c))
				n = 10 * n + (c - '0');
			else {
				year = n;
				n = 0;
				state = isctime ? D_END : D_HOUR;
			}
			break;
		case D_HOUR:
			if (isdigit(c))
				n = 10 * n + (c - '0');
			else {
				hour = n;
				n = 0;
				state = D_MIN;
			}
			break;
		case D_MIN:
			if (isdigit(c))
				n = 10 * n + (c - '0');
			else {
				min = n;
				n = 0;
				state = D_SEC;
			}
			break;
		case D_SEC:
			if (isdigit(c))
				n = 10 * n + (c - '0');
			else {
				sec = n;
				n = 0;
				state = isctime ? D_YEAR : D_END;
			}
			break;
		}
	} while (state != D_END && c);
	switch (month[0]) {
	case 'A':
		mon = (month[1] == 'p') ? 4 : 8;
		break;
	case 'D':
		mon = 12;
		break;
	case 'F':
		mon = 2;
		break;
	case 'J':
		mon = (month[1] == 'a') ? 1 : ((month[2] == 'l') ? 7 : 6);
		break;
	case 'M':
		mon = (month[2] == 'r') ? 3 : 5;
		break;
	case 'N':
		mon = 11;
		break;
	case 'O':
		mon = 10;
		break;
	case 'S':
		mon = 9;
		break;
	default:
		return -1;
	}
	if (year <= 100)
		year += (year < 70) ? 2000 : 1900;
	--mon;
	--day;
	if (sec >= 60 || min >= 60 || hour >= 60 || day >= 31 || year < 1970)
		return -1;
	return sec + 60L * (min + 60L * (hour + 24L * (
		day + daytab[year % 4 == 0][mon] + 365L * (year - 1970L) + ((year - 1969L) >> 2))));
}

static char *rfctime(time_t t, char *buf)
{
	struct tm *tp;

	tp = gmtime(&t);
	if (tp == 0) {
		log_d("gmtime failed!?!?!?");
		return 0;
	}
	strftime(buf, 31, "%a, %d %b %Y %H:%M:%S GMT", tp);
	return buf;
}

static char *__getline(struct pool *p)
{
	register char *s;
	char *olds, *sp, *end;
	register int f;

	end = p->end;
	s = p->start;
	if (s >= end)
		return 0;
	olds = s;
	sp = s;
	f = 0;
	while (s < end) {
		switch (*s++) {
		case '\n':
			if (s == end || (*s != ' ' && *s != '\t')) {
				if (f)
					*sp = 0;
				else
					s[-1] = 0;
				p->start = s;
				return olds;
			}
		case '\r':
		case '\t':
			if (f == 0) {
				f = 1;
				sp = s - 1;
			}
			s[-1] = ' ';
			break;
		default:
			f = 0;
			break;
		}
	}
	log_d("__getline: fallen off the end");
	return 0;
}

static int putstring(struct pool *p, char *s)
{
	int l;

	l = strlen(s);
	if (l > p->ceiling - p->end) {
		log_d("no more room to put string!?!?");
		return -1;
	}
	memcpy(p->end, s, l);
	p->end += l;
	return 0;
}

static int output_headers(struct pool *p, struct request *r)
{
	long cl;
	char tmp_outbuf[2048], gbuf[40], *b;
	int port;

	if (r->cn->assbackwards)
		return 0;
	b = tmp_outbuf;
	b += sprintf(b, "HTTP/%d.%d %s\r\n"
		"Server: %s\r\n"
		"Date: %s\r\n",
		r->protocol_major, r->protocol_minor, r->status_line,
		server_version,
		rfctime(current_time, gbuf));
	if (r->allowedmethods)
		b += sprintf(b, "Allow: %s\r\n", r->allowedmethods);
	if (r->c) {
		if (r->c->refresh)
			b += sprintf(b, "Refresh: %d\r\n", r->c->refresh);
		if (r->status == 401 && r->c->realm)
			b += sprintf(b, "WWW-Authenticate: Basic realm=\"%s\"\r\n", r->c->realm);
	}
	if (r->num_content >= 0) {
		b += sprintf(b, "Content-type: %s\r\n", r->content_type);
		cl = r->content_length;
		if (cl >= 0)
			b += sprintf(b, "Content-length: %ld\r\n", cl);
		if (r->last_modified)
			b += sprintf(b, "Last-Modified: %s\r\n", rfctime(r->last_modified, gbuf));
	}
	if (r->location) {
		if (r->location[0] == '/') {
			port = r->cn->s->port;
				b += sprintf(b, "Location: http://%s%s\r\n", r->servername, r->location);
		} else
			b += sprintf(b, "Location: %s\r\n", r->location);
	}
	if (r->cn->keepalive) {
		if (r->protocol_minor == 0)
			b += sprintf(b, "Connection: Keep-Alive\r\n");
	} else if (r->protocol_minor)
		b += sprintf(b, "Connection: Close\r\n");
	b += sprintf(b, "\r\n");
	return putstring(p, tmp_outbuf);
}

static char *dirmatch(char *s, char *t)
{
	int n;

	n = strlen(t);
	if (n == 0)
		return s;
	return !strncmp(s, t, n) && (s[n] == '/' || s[n] == 0 || s[n-1] == '~') ? s + n : 0;
}

static char *exactmatch(char *s, char *t)
{
	int n;

	n = strlen(t);
	return !strncmp(s, t, n) && s[n] == '/' && s[n + 1] == 0 ? s + n : 0;
}

static int evaluate_access(unsigned long ip, struct access *a)
{
	while (a && ((ip & a->mask) != a->addr))
		a = a->next;
	return a ? a->type : ALLOW;
}

static int get_mime(struct request *r, const char *s)
{
	struct mime *m;
	char *saved_type;
	int saved_class;
	int l, le, lm;

	saved_type = 0;
	saved_class = 0;
	lm = 0;
	l = strlen(s);
	m = r->c->mimes;
	while (m) {
		if (m->ext) {
			le = strlen(m->ext);
			if (le > lm && le <= l && !strcasecmp(s + l - le, m->ext)) {
				lm = le;
				saved_type = m->name;
				saved_class = m->class;
			}
		} else if (saved_type == 0) {
			saved_type = m->name;
			saved_class = m->class;
		}
		m = m->next;
	}
	if (saved_type) {
		r->content_type = saved_type;
		r->class = saved_class;
		r->num_content = lm;
		return 0;
	}
	return -1;
}

static int get_path_info(struct request *r)
{
	char *p, *pa, *end, *cp;
	struct stat *s;
	int rv;

	s = &r->finfo;
	p = r->path_translated;
	end = p + strlen(p);
	pa = r->path_args;
	*pa = 0;
	cp = end;
	while (cp > p && cp[-1] == '/')
		--cp;
	while (cp > p) {
		if (cp != end)
			*cp = 0;
		rv = stat(p, s);
		if (debug) {
			if (rv == -1)
				log_d("get_path_info: stat(\"%s\") = %d", p, rv);
			else
				log_d("get_path_info: stat(\"%s\") = %d; st_mode = %#o", p, rv, s->st_mode);
		}
		if (cp != end)
			*cp = '/';
		if (rv != -1) {
			strcpy(pa, cp);
			if (S_ISDIR(s->st_mode))
				*cp++ = '/';
			*cp = 0;
			return 0;
		}
		while (--cp > p && *cp != '/')
			;
	}
	return -1;
}

static int check_path(struct request *r)
{
	char *p;

	p = r->path;
	if (*p != '/')
		return -1;
	while (1)
		switch (*p++) {
		case 0:
			return 0;
		case '/':
			switch (*p) {
			case '.':
			case '/':
				return -1;
			}
		}
}

static int check_symlinks(struct request *r)
{
	char *p, *s, *t, b[PATHLEN];
	struct control *c;
	struct stat buf;
	int flag, rv;

	p = r->path_translated;
	c = r->c;
	if (c->symlinksok)
		return 0;
	strcpy(b, p);
	t = b + (c->locations->name ? strlen(c->locations->name) : 0);
	s = b + strlen(b);
	flag = 1;
	while (--s > t) {
		if (*s == '/') {
			*s = 0;
			flag = 1;
		} else if (flag) {
			flag = 0;
			rv = lstat(b, &buf);
			if (debug) {
				if (rv == -1)
					log_d("check_symlinks: lstat(\"%s\") = %d", b, rv);
				else
					log_d("check_symlinks: lstat(\"%s\") = %d; st_mode = %#o", b, rv, buf.st_mode);
			}
			if (rv == -1) {
				lerror("lstat");
				return -1;
			}
			if (S_ISLNK(buf.st_mode)) {
				log_d("%s is a symbolic link", b);
					return -1;
			}
		}
	}
	return 0;
}

static int makedir(struct request *r)
{
	char *buf, *e;

	buf = r->newloc;
	strcpy(buf, r->url);
	e = buf + strlen(buf);
	*e++ = '/';
	*e = 0;
	r->location = buf;
	return 302;
}

static int append_indexes(struct request *r)
{
	char *p, *q;
	struct simple_list *i;
	int rv;

	p = r->path_translated;
	q = p + strlen(p);
	r->isindex = 1;
	i = r->c->index_names;
	while (i) {
		strcpy(q, i->name);
		rv = stat(p, &r->finfo);
		if (debug)
			log_d("append_indexes: stat(\"%s\") = %d", p, rv);
		if (rv != -1)
			break;
		i = i->next;
	}
	if (i == 0) {
		*q = 0;
		r->error_file = r->c->error_404_file;
		return 404;
	}
	return 0;
}

static int process_special(struct request *r)
{
	const char *ct;

	ct = r->content_type;
	r->num_content = -1;
	if (!strcasecmp(ct, REDIRECT_MAGIC_TYPE))
		return process_redirect(r);
	if (!strcasecmp(ct, SAFTEMONITOR_MAGIC_TYPE))
		return process_safte(r);
	r->error = se_no_specialty;
	return 500;
}

static int process_fd(struct request *r)
{
	int fd, rv;

	if (r->path_args[0] && r->c->path_args_ok == 0 && (r->path_args[1] || r->isindex == 0)) {
		r->error_file = r->c->error_404_file;
		return 404;
	}
	if (r->method == M_POST) {
		r->error = fb_post_file;
		return 405;
	}
	r->content_length = r->finfo.st_size;
	r->last_modified = r->finfo.st_mtime;
	if (r->last_modified <= r->ims) {
		r->num_content = -1;
		return 304;
	}
	if (r->method == M_GET) {
		fd = open(r->path_translated, O_RDONLY);
		if (debug)
			log_d("process_fd: open(\"%s\") = %d", r->path_translated, fd);
		if (fd == -1) {
			switch (errno) {
			case EACCES:
				r->error = fb_access;
				r->error_file = r->c->error_403_file;
				return 403;
			case EMFILE:
				r->error = su_open;
				return 503;
			default:
				lerror("open");
				r->error = se_open;
				return 500;
			}
		}
		rv = fcntl(fd, F_SETFD, FD_CLOEXEC);
		if (debug)
			log_d("process_fd: fcntl(%d, F_SETFD, FD_CLOEXEC) = %d", fd, rv);
		r->cn->rfd = fd;
	}
	return 200;
}

static int add_fd(struct request *r, const char *filename)
{
	int fd, rv;
	struct stat s;

	if (filename == 0)
		return -1;
	if (get_mime(r, filename) == -1)
		return -1;
	fd = open(filename, O_RDONLY);
	if (debug)
		log_d("add_fd: open(\"%s\") = %d", filename, fd);
	if (fd == -1)
		return -1;
	rv = fstat(fd, &s);
	if (debug) {
		if (rv == -1)
			log_d("add_fd: fstat(%d) = %d", fd, rv);
		else
			log_d("add_fd: fstat(%d) = %d; st_mode = %#o", fd, rv, s.st_mode);
	}
	if (!S_ISREG(s.st_mode)) {
		log_d("non-plain file %s", filename);
		rv = close(fd);
		if (debug)
			log_d("add_fd: close(%d) = rv", fd);
		return -1;
	}
	rv = fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (debug)
		log_d("add_fd: fcntl(%d, F_SETFD, FD_CLOEXEC) = %d", fd, rv);
	r->cn->rfd = fd;
	r->content_length = s.st_size;
	return 0;
}

static int find_vs(struct request *r)
{
	struct virtual *v;

	/* virtual servers disabled - always use first one */
	v = r->cn->s->children;
	v->nrequests++;
	r->vs = v;
	r->servername = r->host;
	return 0;
}

static int check_realm(struct request *r)
{
	char *a;

	if (r == 0 || r->c == 0 || r->c->realm == 0 || r->c->userfile == 0)
		return 0;
	a = r->authorization;
	if (a == 0)
		return -1;
	if (strncasecmp(a, "basic", 5))
		return -1;
	a += 5;
	while (*a == ' ')
		++a;
	if (webuserok(a, r->c->userfile, r->user, sizeof r->user, r->c->do_crypt))
		return 0;
	return -1;
}

struct control *faketoreal(char *x, char *y, struct request *r, int update)
{
	unsigned long ip;
	struct control *c;
	char *s;

	if (r->vs == 0) {
		log_d("virtualhost not initialized!");
		return 0;
	}
	ip = r->cn->peer.sin_addr.s_addr;
	s = 0;
	c = r->vs->controls;
	while (c) {
		if (c->locations && c->alias) {
			s = c->exact_match ? exactmatch(x, c->alias) : dirmatch(x, c->alias);
			if (s && (c->clients == 0 || evaluate_access(ip, c->clients) == APPLY))
				break;
		}
		c = c->next;
	}
	if (c) {
		if (update)
			c->locations = c->locations->next;
		strcpy(y, c->locations->name);
		if (c->locations->name[0] == '/' || !c->path_args_ok)
			strcat(y, s);
	}
	return c;
}

static int process_path(struct request *r)
{
	int rv;

	switch (find_vs(r)) {
	case -1:
		r->error = se_no_virtual;
		return 500;
	case 1:
		r->error = se_no_virtual;
		return 404;
	}
	if ((r->c = faketoreal(r->path, r->path_translated, r, 1)) == 0) {
		r->error = se_alias;
		return 500;
	}
	if (r->c->locations == 0) {
		log_d("raah... no locations found");
		r->error_file = r->c->error_404_file;
		return 404;
	}
	if (r->path_translated[0] == 0) {
		r->error = se_alias;
		return 500;
	}
	if (r->path_translated[0] != '/') {
		escape_url(r->path_translated, r->newloc);
		r->location = r->newloc;
		return 302;
	}
	if (evaluate_access(r->cn->peer.sin_addr.s_addr, r->c->accesses) == DENY) {
		r->error = fb_active;
		r->error_file = r->c->error_403_file;
		return 403;
	}
	if (check_realm(r) == -1) {
		r->error_file = r->c->error_401_file;
		return 401;
	}
	if (check_path(r) == -1) {
		r->error = br_bad_path_name;
		return 400;
	}
	if (get_path_info(r) == -1) {
		r->error = se_get_path_info;
		return 500;
	}
	if (S_ISDIR(r->finfo.st_mode)) {
		if (r->path_args[0] != '/')
			return makedir(r);
		rv = append_indexes(r);
		if (rv)
			return rv;
	}
	if (!S_ISREG(r->finfo.st_mode)) {
		r->error = fb_not_plain;
		r->error_file = r->c->error_403_file;
		return 403;
	}
	if (check_symlinks(r) == -1) {
		r->error = fb_symlink;
		r->error_file = r->c->error_403_file;
		return 403;
	}
	if (get_mime(r, r->path_translated) == -1) {
		r->error = se_no_mime;
		return 500;
	}
	switch (r->class) {
	case CLASS_FILE:
		return process_fd(r);
	case CLASS_SPECIAL:
		return process_special(r);
	}
	r->error = se_no_class;
	return 500;
}

static int process_headers(struct request *r)
{
	char *l, *u, *s, *t;
	unsigned long x, y;
	time_t i;

	while (1) {
		l = __getline(r->cn->input);
		if (l == 0) {
			return -1;
		}
		while (*l == ' ')
			++l;
		u = strchr(l, ' ');
		if (u)
			break;
		log_d("ignoring garbage \"%.80s\" from %s", l, r->cn->ip);
	}
	r->method_s = l;
	*u++ = 0;
	while (*u == ' ')
		++u;
	s = strrchr(u, ' ');
	if (s) {
		r->version = s + 1;
		do {
			*s-- = 0;
		} while (*s == ' ');
	}
	r->url = u;
	s = strchr(u, '?');
	if (s) {
		r->args = s + 1;
		*s = 0;
	}
	while ((l = __getline(r->cn->input)) != 0) {
		s = strchr(l, ':');
		if (s == 0)
			continue;
		*s++ = 0;
		while (*s == ' ')
			++s;
		if (*s == 0)
			continue;
		if (!strcasecmp(l, "User-agent"))
			r->user_agent = s;
		else if (!strcasecmp(l, "Referer"))
			r->referer = s;
		else if (!strcasecmp(l, "From"))
			r->from = s;
		else if (!strcasecmp(l, "Authorization"))
			r->authorization = s;
		else if (!strcasecmp(l, "Cookie"))
			r->cookie = s;
		else if (!strcasecmp(l, "Host"))
			r->host = s;
		else if (!strcasecmp(l, "Connection"))
			r->connection = s;
		else if (!strcasecmp(l, "If-modified-since"))
			r->ims_s = s;
		else if (!strcasecmp(l, "Content-type"))
			r->in_content_type = s;
		else if (!strcasecmp(l, "Content-length"))
			r->in_content_length = s;
	}
	if (debug) {
		if (r->method_s)
			log_d("method_s = \"%.80s\"", r->method_s);
		if (r->version)
			log_d("version = \"%.80s\"", r->version);
		if (r->url)
			log_d("url = \"%.80s\"", r->url);
		if (r->args)
			log_d("args = \"%.80s\"", r->args);
		if (r->user_agent)
			log_d("user_agent = \"%.80s\"", r->user_agent);
		if (r->referer)
			log_d("referer = \"%.80s\"", r->referer);
		if (r->from)
			log_d("from = \"%.80s\"", r->from);
		if (r->authorization)
			log_d("authorization = \"%.80s\"", r->authorization);
		if (r->cookie)
			log_d("cookie = \"%.80s\"", r->cookie);
		if (r->host)
			log_d("host = \"%.80s\"", r->host);
		if (r->connection)
			log_d("connection = \"%.80s\"", r->connection);
		if (r->ims_s)
			log_d("ims_s = \"%.80s\"", r->ims_s);
		if (r->in_content_type)
			log_d("in_content_type = \"%.80s\"", r->in_content_type);
		if (r->in_content_length)
			log_d("in_content_length = \"%.80s\"", r->in_content_length);
	}
	s = r->method_s;
	if (s == 0) {
		log_d("method_s == 0 !?");
		return -1;
	}
	if (strcmp(s, m_get) == 0) {
		r->method = M_GET;
	} else {
		if (r->cn->assbackwards) {
			log_d("method \"%.80s\" not implemented for old-style connections", s);
			r->error = ni_not_implemented;
			return 501;
		}
		if (strcmp(s, m_head) == 0)
			r->method = M_HEAD;
		else if (strcmp(s, m_post) == 0)
			r->method = M_POST;
		else {
			log_d("method \"%.80s\" not implemented", s);
			r->error = ni_not_implemented;
			return 501;
		}
	}
	s = r->url;
	if (s == 0) {
		log_d("url == 0 !?");
		return -1;
	}
	if (strlen(s) > STRLEN) {
		log_d("url too long from %s", r->cn->ip);
		r->error = br_bad_url;
		return 400;
	}
	if (unescape_url(s, r->path) == -1) {
		r->error = br_bad_url;
		return 400;
	}
	if (r->path[0] != '/') {
		r->error = br_bad_url;
		return 400;
	}
	if (r->cn->assbackwards) {
		r->protocol_major = 0;
		r->protocol_minor = 9;
	} else {
		s = r->version;
		if (s == 0) {
			log_d("version == 0 !?");
			return -1;
		}
		if (strncmp(s, "HTTP/", 5)) {
			log_d("unsupported version \"%.32s\" from %s", s, r->cn->ip);
			r->error = br_bad_protocol;
			return 400;
		}
		t = strchr(s + 5, '.');
		if (t == 0) {
			log_d("unsupported version \"%.32s\" from %s", s, r->cn->ip);
			r->error = br_bad_protocol;
			return 400;
		}
		*t = 0;
		x = atoi(s + 5);
		y = atoi(t + 1);
		*t = '.';
		if (x != 1 || y > 1) {
			log_d("unsupported version \"%.32s\" from %s", s, r->cn->ip);
			r->error = ni_version_not_supp;
			return 505;
		}
		r->protocol_major = x;
		r->protocol_minor = y;
		s = r->connection;
		if (y) {
			r->cn->keepalive = !(s && strcasecmp(s, "Close") == 0);
		} else {
			r->cn->keepalive = s && strcasecmp(s, "Keep-Alive") == 0;
		}
	}
	if (r->method == M_GET) {
		s = r->ims_s;
		if (s) {
			i = timerfc(s);
			if (i == (time_t) -1) {
				log_d("illegal date \"%.80s\" in If-Modified-Since", s);
				r->error = br_bad_date;
				return 400;
			}
			r->ims = i;
		}
	}
	return 0;
}

int prepare_reply(struct request *r)
{
	struct pool *p;
	char *b, buf[PATHLEN];
	int send_message;

	send_message = r->method != M_HEAD;
	if (r->status >= 400)
		r->last_modified = 0;
	switch (r->status) {
	case 200:
		r->status_line = "200 OK";
		send_message = 0;
		break;
	case 204:
		r->status_line = "204 No Content";
		send_message = 0;
		break;
	case 302:
		r->status_line = "302 Moved";
		break;
	case 304:
		r->status_line = "304 Not Modified";
		send_message = 0;
		break;
	case 400:
		r->status_line = "400 Bad Request";
		break;
	case 401:
		r->status_line = "401 Not Authorized";
		break;
	case 403:
		r->status_line = "403 Forbidden";
		break;
	case 404:
		r->status_line = "404 Not Found";
		break;
	case 405:
		r->status_line = "405 Method Not Allowed";
		r->allowedmethods = "GET, HEAD";
		break;
	case 501:
		r->status_line = "501 Not Implemented";
		break;
	case 503:
		r->status_line = "503 Service Unavailable";
		break;
	case 505:
		r->status_line = "505 HTTP Version Not Supported";
		break;
	default:
		r->status_line = "500 Internal Server Error";
		break;
	}
	if (r->error_file) {
		if (add_fd(r, r->error_file) != -1)
			send_message = 0;
	}
	if (send_message) {
		b = buf;
		b += sprintf(b, "<title>%s</title>\n"
			"<h1>%s</h1>\n", r->status_line, r->status_line);
		switch (r->status) {
		case 302:
			b += sprintf(b, "This document has moved to URL <a href=\"%s\">%s</a>.\n", r->location, r->location);
			break;
		case 401:
			b += sprintf(b, "You need proper authorization to use this resource.\n");
			break;
		case 400:
		case 405:
		case 501:
		case 505:
			b += sprintf(b, "Your request was not understood or not allowed by this server.\n");
			break;
		case 403:
			b += sprintf(b, "Access to this resource has been denied to you.\n");
			break;
		case 404:
			b += sprintf(b, "The resource requested could not be found on this server.\n");
			break;
		case 503:
			b += sprintf(b, "The server is temporarily busy.\n");
			break;
		default:
			b += sprintf(b, "An internal server error has occurred.\n");
			break;
		}
		if (r->c && r->c->admin)
			b += sprintf(b, "<p>Please contact the site administrator at <i>%s</i>.\n", r->c->admin);
		r->content_length = strlen(buf);
		r->num_content = 0;
		r->content_type = "text/html";
	}
	if (r->status >= 400 && r->method != M_GET && r->method != M_HEAD)
		r->cn->keepalive = 0;
	p = r->cn->output;
	return (output_headers(p, r) == -1 || (send_message && putstring(p, buf) == -1)) ? -1 : 0;
}

static void init_request(struct request *r)
{
	r->vs = 0;
	r->user_agent = 0;
	r->referer = 0;
	r->from = 0;
	r->authorization = 0;
	r->cookie = 0;
	r->host = 0;
	r->in_content_type = 0;
	r->in_content_length = 0;
	r->connection = 0;
	r->ims_s = 0;
	r->path[0] = 0;
	r->path_translated[0] = 0;
	r->path_args[0] = 0;
	r->num_content = -1;
	r->class = 0;
	r->content_length = -1;
	r->last_modified = 0;
	r->ims = 0;
	r->location = 0;
	r->status_line = 0;
	r->error = 0;
	r->method_s = 0;
	r->url = 0;
	r->args = 0;
	r->version = 0;
	r->protocol_major = 0;
	r->protocol_minor = 0;
	r->method = 0;
	r->status = 0;
	r->isindex = 0;
	r->c = 0;
	r->error_file = 0;
	r->user[0] = 0;
	r->servername = 0;
	r->allowedmethods = 0;
}

int process_request(struct request *r)
{
	init_request(r);
	if ((r->status = process_headers(r)) == 0)
		r->status = process_path(r);
	if (r->status > 0 && prepare_reply(r) == -1) {
		log_d("cannot prepare reply for client");
		return -1;
	}
	if (r->status_line && r->c)
		r->processed = 1;
	return r->status > 0 ? 0 : -1;
}
