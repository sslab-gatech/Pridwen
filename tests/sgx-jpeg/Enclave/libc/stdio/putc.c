#include <stdio.h>
#include "putc.h"

int __towrite(FILE *f)
{
	f->mode |= f->mode-1;
	if (f->flags & F_NOWR) {
		f->flags |= F_ERR;
		return EOF;
	}
	/* Clear read buffer (easier than summoning nasal demons) */
	f->rpos = f->rend = 0;

	/* Activate write through the buffer. */
	f->wpos = f->wbase = f->buf;
	f->wend = f->buf + f->buf_size;

	return 0;
}

int __overflow(FILE *f, int _c)
{
	unsigned char c = _c;
	if (!f->wend && __towrite(f)) return EOF;
	if (f->wpos != f->wend && c != f->lbf) return *f->wpos++ = c;
	if (f->write(f, &c, 1)!=1) return EOF;
	return c;
}

#define putc_unlocked(c, f) \
	( (((unsigned char)(c)!=(f)->lbf && (f)->wpos!=(f)->wend)) \
	? *(f)->wpos++ = (unsigned char)(c) \
	: __overflow((f),(unsigned char)(c)) )

int putc(int c, FILE *f)
{
	return do_putc(c, f);
}

weak_alias(putc, _IO_putc);
