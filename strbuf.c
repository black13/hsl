/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/** $Id$
 *
 * A string buffer that does not rely on zero-terminated strings, ecept where
 * specified.
 *
 * Append operations may realloc the space, so the returned strbuf_t *must* be
 * used as the new string buffer object.
 *
 * The strbuf_t object may be passed to free() at any time to discard the whole
 * string buffer.
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "strbuf.h"
#include "xmemory.h"

/**
 * The string buffer data type.
 */
struct _string_buffer {
        unsigned int size;              /* allocated size of the buffer,
                                         * including the space for the zero
                                         * byte */
        unsigned int used;              /* used content size of the buffer,
                                         * *not* including the terminating zero
                                         * byte */
        unsigned int rpos;              /* read position */
        char s[1];                      /* the actual string buffer, used
                                         * always from the beginning */
};


/**
 * Reset the current read position to the beginning of the strbuf.
 */
void strbuf_read_reset(strbuf_t sb)
{
        sb->rpos = 0;
}

/**
 * Read a character from the current position. Return the character or EOF as
 * appropriate.
 */
int strbuf_readc(strbuf_t sb)
{
        assert(sb->rpos <= sb->used);
        if (sb->rpos == sb->used) {
                return EOF;
        } else {
                return sb->s[sb->rpos++];
        }
}

/**
 * Read the rest of the strbuf from the current position. Set the read position
 * to the end of the string. Return a string, which may be empty if we are
 * already at the end of the string.
 */
char *strbuf_read_rest(strbuf_t sb)
{
        assert(sb->rpos <= sb->used);
        sb->rpos = sb->used;
        return sb->s + sb->rpos;
}

/**
 * Read a number of characters from the strbuf from the current position. Return
 * the number of characters actually read. The beginning of the character region
 * is written into the character pointer pointed to by `begptr'. This character
 * region is not zero-terminated, so more characters *can* be accessed through
 * *begptr, but the read position is advanced only by the amount of the return
 * value.
 */
int strbuf_readn(strbuf_t sb, unsigned int nchars, char **begptr)
{
        assert(sb->rpos <= sb->used);
        *begptr = sb->s + sb->rpos;
        int ret = nchars;
        if (sb->rpos + nchars > sb->used) {
                ret = sb->used - sb->rpos;
        }
        sb->rpos += ret;
        return ret;
}


/**
 * Allocate a new string buffer.
 */
strbuf_t strbuf_new(void)
{
        /* the strbuf is allocated with a string size of one to exercise the
         * expansion code early; the performance hit should be negligible if the
         * strbuf is reused frequently
         */
        strbuf_t sb = (strbuf_t) xcalloc(1, sizeof(struct _string_buffer),
                                         "new strbuf");
        sb->size = 1;
        return sb;
}


/**
 * Create a new string buffer from a string. The string argument is copied into
 * the string buffer.
 */
strbuf_t strbuf_init(char *s, unsigned int len)
{
        strbuf_t sb = (strbuf_t) xmalloc(sizeof(struct _string_buffer) + len,
                                         "strbuf init");
        sb->used = len;
        sb->size = len + 1;
        sb->rpos = 0;
        memcpy(sb->s, s, len);
        sb->s[len] = 0;

        return sb;
}


/**
 * Append `slen' characters of a string to the string buffer. Return the string
 * buffer. The string buffer may be newly allocated, so the old string buffer
 * may be invalid and the returned one *must* be used.
 */
strbuf_t strbuf_nappend(strbuf_t sb, char *s, int slen)
{
        int needed = sb->used + slen + 1;
        int newsize;
        strbuf_t newsb;

        if (needed > sb->size) {
                newsize = sb->size;
                do {
                        newsize = newsize ? 2 * newsize : 1;
                } while (newsize < needed);
                newsb = realloc(sb, sizeof(struct _string_buffer)
                                + newsize);
                if (newsb == NULL) {
                        return newsb;
                }
                sb = newsb;
                sb->size = newsize;
        }
        memmove(sb->s + sb->used, s, slen + 1);
        sb->used += slen;
        sb->s[sb->used] = 0;
        return sb;
}


/**
 * Append a zero-terminated string to the string buffer. Return the string
 * buffer. The string buffer may be newly allocated, so the old string buffer my
 * be invalid and the returned one *must* be used.
 */
strbuf_t strbuf_append(strbuf_t sb, char *s)
{
        return strbuf_nappend(sb, s, strlen(s));
}

/**
 * Add a character to the string buffer. Return the string buffer. The string
 * buffer may be newly allocated, so the old string buffer my be invalid and the
 * returned one *must* be used.
 */
strbuf_t strbuf_addc(strbuf_t sb, char c)
{
        return strbuf_nappend(sb, &c, 1);
}


/**
 * Return the string content of the string buffer.
 */
char *strbuf_string(strbuf_t sb)
{
        return sb->s;
}

/**
 * Return the content size of the string buffer (without the terminating zero
 * byte).
 */
int strbuf_size(strbuf_t sb)
{
        return sb->used;
}

/**
 * Remove n characters from end of the string contents of the string buffer. 
 * Return the string buffer. The string buffer location is not changed, so using
 * the returned string buffer value is optional.
 */
strbuf_t strbuf_chop(strbuf_t sb, int n)
{
        if (n > sb->used) {
                n = sb->used;
        }
        sb->used -= n;
        sb->s[sb->used] = 0;
        return sb;
}


/**
 * Reinitialize an existing string buffer to be empty. The allocated memory is
 * reused. The string buffer is not changed, so using the returned string
 * buffer value is optional.
 */
strbuf_t strbuf_reinit(strbuf_t sb)
{
        sb->used = 0;
        sb->rpos = 0;
        sb->s[0] = 0;
        return sb;
}

#ifdef TEST

void check(int n, char *result, char *expected)
{
        if (strcmp(result, expected)) {
                printf("test %d: >>%s<< ne >>%s<<!!\n", n, result, expected);
        } else {
                printf("test %d ok\n", n);
        }
}

int main(void)
{
        strbuf_t sb = strbuf_new();
        check(1, strbuf_string(sb), "");
        sb = strbuf_append(sb, "3");
        check(2, strbuf_string(sb), "3");
        sb = strbuf_append(sb, " ");
        check(3, strbuf_string(sb), "3 ");
        sb = strbuf_append(sb, "Chinesen");
        check(4, strbuf_string(sb), "3 Chinesen");
        sb = strbuf_append(sb, " ");
        check(5, strbuf_string(sb), "3 Chinesen ");
        sb = strbuf_append(sb, "mit");
        check(6, strbuf_string(sb), "3 Chinesen mit");
        sb = strbuf_append(sb, " ");
        check(7, strbuf_string(sb), "3 Chinesen mit ");
        sb = strbuf_append(sb, "dem");
        check(8, strbuf_string(sb), "3 Chinesen mit dem");
        sb = strbuf_append(sb, " ");
        check(9, strbuf_string(sb), "3 Chinesen mit dem ");
        sb = strbuf_append(sb, "Kontrabass");
        check(10, strbuf_string(sb), "3 Chinesen mit dem Kontrabass");
        strbuf_chop(sb, 1);
        check(11, strbuf_string(sb), "3 Chinesen mit dem Kontrabas");
        strbuf_chop(sb, 5);
        check(12, strbuf_string(sb), "3 Chinesen mit dem Kont");
        strbuf_chop(sb, -3);
        check(13, strbuf_string(sb), "3 Chinesen mit dem Kont");
        strbuf_chop(sb, 49);
        check(14, strbuf_string(sb), "");
        free(sb);
}
#endif  /* TEST */

/* EOF */
