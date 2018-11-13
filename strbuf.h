/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/** $Id: strbuf.h,v 1.4 2008/02/08 15:18:21 jni Exp $
 *
 * A string buffer with create, append (zero-terminated or with count), and
 * chop (from the end) operations. The strbuf_t data type is exported as an
 * opaque pointer.
 *
 * Append operations may realloc the space, so the returned strbuf_t *must* be
 * used as the new string buffer object.
 *
 * The strbuf_t object may be passed to free(3) at any time to discard a
 * string buffer.
 */

#ifndef __STRBUF_H_INC
#define __STRBUF_H_INC


/**
 * A string buffer data type. Can be free()d at any time.
 */
typedef struct _string_buffer *strbuf_t;


/**
 * Allocate a new string buffer.
 */
strbuf_t strbuf_new(void);

/**
 * Create a new string buffer from a string. The string argument is copied into
 * the string buffer.
 */
strbuf_t strbuf_init(char *s, unsigned int len);

/**
 * Reinitialize the string buffer, such that the string is empty. No
 * reallocation takes place.
 */
strbuf_t strbuf_reinit(strbuf_t sb);

/**
 * Append `slen' characters of a string to the string buffer. Return the string
 * buffer. The string buffer may be newly allocated, so the old string buffer
 * my be invalid and the returned one *must* be used.
 */
strbuf_t strbuf_nappend(strbuf_t sb, char *s, int slen);

/**
 * Append a zero-terminated string to the string buffer. Return the string
 * buffer. The string buffer may be newly allocated, so the old string buffer
 * may be invalid and the returned one *must* be used.
 */
strbuf_t strbuf_append(strbuf_t sb, char *s);

/**
 * Add a character to the string buffer. Return the string buffer. The string
 * buffer may be newly allocated, so the old string buffer my be invalid and the
 * returned one *must* be used.
 */
strbuf_t strbuf_addc(strbuf_t sb, char c);


/**
 * Return the string content of the string buffer.
 */
char *strbuf_string(strbuf_t sb);

/**
 * Return the content size of the string buffer (without a terminating zero
 * byte).
 */
int strbuf_size(strbuf_t sb);

/**
 * Remove n characters from end of the string contents of the string buffer. 
 * Return the string buffer. The string buffer location is not changed, so using
 * the returned string buffer value is optional.
 */
strbuf_t strbuf_chop(strbuf_t sb, int n);

/**
 * Reinitialize an existing string buffer to be empty. The allocated memory is
 * reused. The string buffer location is not changed, so using the returned
 * string buffer value is optional.
 */
strbuf_t strbuf_reinit(strbuf_t sb);

/**
 * Read a number of characters from the strbuf from the current position. Return
 * the number of characters actually read. The beginning of the character region
 * is written into the character pointer pointed to by `begptr'. This character
 * region is not zero-terminated, so more characters *can* be accessed through
 * *begptr, but the read position is advanced only by the amount of the return
 * value.
 */
int strbuf_readn(strbuf_t sb, unsigned int nchars, char **begptr);

/**
 * Read the rest of the strbuf from the current position. Set the read position
 * to the end of the string. Return a string, which may empty if we are already
 * at the end of the string.
 */
char* strbuf_read_rest(strbuf_t sb);

/**
 * Read a character from the current position. Return the character or EOF as
 * appropriate.
 */
int strbuf_readc(strbuf_t sb);

/**
 * Reset the current read position to the beginning of the strbuf.
 */
void strbuf_read_reset(strbuf_t sb);


#endif  /* __STRBUF_H_INC */

/* EOF */
