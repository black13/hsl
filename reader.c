/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/**
 * stuff for reading objects from a stream
 */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sysexits.h>
#include "strbuf.h"
#include "signals.h"
#include "names.h"
#include "io.h"
#include "reader.h"
#include "xmemory.h"
#include "gc.h"
#include "math.h"


#define TABWIDTH   8                    /* assumed */


typedef enum {
        T_NO_TOK,                       /* value for no token present */
        /* all atoms -- numbers, symbols, strings, etc. */
        T_ISATOM,
        /* syntactic tokens */
        T_OPAREN,                       /* ( */
        T_CPAREN,                       /* ) */
        T_OBRACE,                       /* { */
        T_CBRACE,                       /* } */
        T_OBRACK,                       /* [ */
        T_CBRACK,                       /* ] */
        T_SQUOTE,                       /* ' */
        T_QQUOTE,                       /* ` */
        T_UNQUOT,                       /* , */
        T_SPLICE,                       /* @ */
        T_PERIOD,                       /* . */
        T_RMACRO,                       /* #, reader macro */
        /* other tokens */
        T_ENDOFF,                       /* EOF */
        T_LERROR,                       /* input error, make reader abort */
        T_LAST_T                        /* number of types */
} token_t;


char *token_name[] = {
        "T_NO_TOK",
        "atom",
        "open paren",
        "close paren",
        "open brace",
        "close brace",
        "open bracket",
        "close bracket",
        "single quote",
        "backquote",
        "comma",
        "atsign",
        "period",
        "reader macro",
        "end of input",
        "error",
        "T_INVALD",
        0
};


typedef enum {
        CHRTOK,                         /* single-character token */
        DQUOTE,
        COMMMA,
        ATSIGN,
        PERIOD,
        SEMICL,
        NEWLIN,
        ENDOFF,
        IDENTF,
        BACKSL,                         /* \\ */
        WHITES,                         /* whitespace except newline */
        LASTCC                          /* last, unused entry */
} cclass_t;

/* Lexer state */
typedef enum {
        INIT,                           /* initial, top level */
        STRG,                           /* in string */
        SBCK,                           /* in string after backslash */
        SYMB,                           /* in symbol or number or char const */
        CMNT,                           /* in comment */
        MPRD,                           /* maybe period */
        LAST                            /* last, unused entry */
} l_state_t;

/* Lexer actions */
typedef enum {
        NONE,                           /* do nothing */
        ADDC,                           /* add character to token string */
        ADDB,                           /* add backslash-quoted character to
                                           string */
        FINI,                           /* finish multi-character token */
        MSCT,                           /* make single character token, or
                                           perhaps finish collected
                                           multi-character token */
        SPRD,                           /* save period, pushback current */
        MCAP,                           /* multi-character cont. after period */
        ERRR                            /* raise error */
} l_action_t;


int action[LASTCC][LAST] = {
        /*INIT  STRG  SBCK  SYMB  CMNT  MPRD */
        { MSCT, ADDC, ADDB, MSCT, NONE, SPRD }, /* CHRTOK */
        { NONE, FINI, ADDB, MSCT, NONE, SPRD }, /* DQUOTE */
        { MSCT, ADDC, ADDB, ADDC, NONE, SPRD }, /* COMMMA */
        { MSCT, ADDC, ADDB, ADDC, NONE, SPRD }, /* ATSIGN */
        { NONE, ADDC, ADDB, ADDC, NONE, MCAP }, /* PERIOD */
        { NONE, ADDC, ADDB, FINI, NONE, SPRD }, /* SEMICL */
        { NONE, ADDC, ADDB, FINI, NONE, SPRD }, /* NEWLIN */
        { MSCT, ERRR, ERRR, FINI, NONE, SPRD }, /* ENDOFF */
        { ADDC, ADDC, ADDB, ADDC, NONE, MCAP }, /* IDENTF */
        { ADDC, NONE, ADDB, ADDC, NONE, ADDC }, /* BACKSL */
        { NONE, ADDC, ADDB, FINI, NONE, SPRD }, /* WHITES */
};

l_state_t newstate[LASTCC][LAST] = {
        /*INIT  STRG  SBCK  SYMB  CMNT  MPRD */
        { INIT, STRG, STRG, INIT, CMNT, INIT }, /* CHRTOK */
        { STRG, INIT, STRG, STRG, CMNT, INIT }, /* DQUOTE */
        { INIT, STRG, STRG, SYMB, CMNT, INIT }, /* COMMMA */
        { INIT, STRG, STRG, SYMB, CMNT, INIT }, /* ATSIGN */
        { MPRD, STRG, STRG, SYMB, CMNT, SYMB }, /* PERIOD */
        { CMNT, STRG, STRG, CMNT, CMNT, INIT }, /* SEMICL */
        { INIT, STRG, STRG, INIT, INIT, INIT }, /* NEWLIN */
        { INIT, INIT, INIT, INIT, INIT, INIT }, /* ENDOFF */
        { SYMB, STRG, STRG, SYMB, CMNT, SYMB }, /* IDENTF */
        { SYMB, SBCK, STRG, SYMB, CMNT, SYMB }, /* BACKSL */
        { INIT, STRG, STRG, INIT, CMNT, INIT }, /* WHITES */
};


void new_reader(session_context_t *sc)
{
        sc->tokbuf = strbuf_new();
        sc->pushback_token = T_NO_TOK;
        sc->lineno = 1;
}

cclass_t charclass(int c, session_context_t *sc)
{
        if (c == '\t') {
                sc->column += TABWIDTH - (sc->column % TABWIDTH);
        } else {
                sc->column++;
        }
        switch (c) {
            case '\'':
            case '#':
            case '(':
            case ')':
            case '{':
            case '}':
            case '[':
            case ']':
            case '`':  return CHRTOK;
            case '\"': return DQUOTE;
            case '\\': return BACKSL;
            case ',':  return COMMMA;
            case '@':  return ATSIGN;
            case '.':  return PERIOD;
            case ';':  return SEMICL;
            case '\n':
                sc->lineno++;
                sc->column = 0;
                return NEWLIN;
            case EOF:  return ENDOFF;
            default:
                if (isspace(c)) {
                        return WHITES;
                }
                return IDENTF;
        }
}


char backslashed(char c)
{
        switch (c) {
            case 'a': return '\a';
            case 'b': return '\b';
            case 'f': return '\f';
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case 'v': return '\v';
            default: return c;
        }
}


int char_constant(char *s, uint len)
{
        enum Mode { number, literal } mode;
        int base = 0;
        int start;

        switch (s[2]) {
            case 'b': mode = number; base = 2; start = 3; break;
            case 'x': mode = number; base = 16; start = 3; break;
            case '\'': mode = literal; start = 3; break;
            default:
                if (isdigit(s[2])) {
                        mode = number;
                        base = 8;
                        start = 2;
                } else {
                        mode = literal;
                        start = 2;
                }
                break;
        }
        if (len <= start) {
                return -1;
        }
        if (mode == literal) {
                return s[start];
        } else {
                char *endp;
                int value;
                value = (int) strtol(s + start, &endp, base);
                if (endp != s + len) {
                        return -1;
                }
                return value;
        }
}


token_t make_sctoken(int c)
{
        switch (c) {
            case '(':  return T_OPAREN;
            case ')':  return T_CPAREN;
            case '{':  return T_OBRACE;
            case '}':  return T_CBRACE;
            case '[':  return T_OBRACK;
            case ']':  return T_CBRACK;
            case '\'': return T_SQUOTE;
            case '`':  return T_QQUOTE;
            case ',':  return T_UNQUOT;
            case '@':  return T_SPLICE;
            case '.':  return T_PERIOD;
            case '#':  return T_RMACRO;
            case EOF:  return T_ENDOFF;
            default:
                fprintf(stderr, "invalid sctoken \'%c\' 0x%x\n", c, c);
                exit(EX_SOFTWARE);
        }       
}


token_t make_atom(l_state_t state, session_context_t *sc)
{
        if (state == STRG) {
                sc->tok_atom = new_string(strbuf_string(sc->tokbuf),
                                           strbuf_size(sc->tokbuf));
                return T_ISATOM;
        }
        sc->tokbuf = strbuf_addc(sc->tokbuf, '\0'); /* end for strtol */
        char *s = strbuf_string(sc->tokbuf);
        int len = strbuf_size(sc->tokbuf) - 1;
        if (len > 2 && s[0] == '?' && s[1] == '\\') {
                int c = char_constant(s, len);
                if (c >= 0) {
                        sc->tok_atom = new_char(c);
                        return T_ISATOM;
                } else {
                        sc->tok_atom =
                                throw_error(sc->out, ERR_RSYNTAX, 0,
                                            "%s:%d:%d: invalid char constant",
                                            sc->name, sc->lineno, sc->column);
                        AS(sc->tok_atom, SIGNAL)->data = new_string(s, len);
                        return T_LERROR;
                }
        } else {
                char *end;
                long double value = strtold(s, &end);
                if (end - s == len) {
                        if (remainderl(value, 1) == 0) {
                                sc->tok_atom = new_integer((long) value);
                        } else {
                                sc->tok_atom = new_ldouble(value);
                        }
                        return T_ISATOM;
                }
                sc->tok_atom = intern(s, len);
                return T_ISATOM;
        }
}


void pushback(token_t token, session_context_t *sc)
{
        sc->pushback_token = token;
}
        

token_t lexer_error(l_state_t state, int c, session_context_t *sc)
{
        char *name;
        switch (state) {
            case INIT: name = "INIT"; break;
            case STRG: name = "STRG"; break;
            case SBCK: name = "SBCK"; break;
            case SYMB: name = "SYMB"; break;
            case CMNT: name = "CMNT"; break;
            case LAST: name = "LAST"; break;
            default: name = "<invalid!>"; break;
        }
        sc->tok_atom =
                throw_error(sc->out, ERR_RSYNTAX, 0,
                            "%s:%d:%d: lexer error in state %d with char %d",
                            sc->name, sc->lineno, sc->column, name, c);
        return T_LERROR;
}


/**
 * return the next token from the input stream; if it is multi-character, the
 * contents is in the tokbuf
 */
token_t read_next_token(session_context_t *sc)
{
        l_state_t state = INIT;
        sc->tokbuf = strbuf_reinit(sc->tokbuf);

        if (sc->pushback_token != T_NO_TOK) {
                token_t token = sc->pushback_token;
                sc->pushback_token = T_NO_TOK;
                return token;
        }

        int c;
        while (1) {
                obp_t ch = port_getc(sc->in);
                if (IS_EXIT(ch)) {
                        sc->tok_atom = ch;
                        return T_LERROR;
                }
                if (IS_EOF(ch)) {
                        c = EOF;
                } else {
                        c = AS(ch, CHAR)->value;
                }
                
                cclass_t class = charclass(c, sc);
                switch (action[class][state]) {
                    case NONE: break;
                    case ADDB:
                        c = backslashed(c);
                        /* FALLTHROUGH */
                    case ADDC:
                        sc->tokbuf = strbuf_addc(sc->tokbuf, c);
                        break;
                    case MCAP:
                        sc->tokbuf = strbuf_addc(sc->tokbuf, '.');
                        sc->tokbuf = strbuf_addc(sc->tokbuf, c);
                        break;
                    case SPRD:
                        port_ungetc(sc->in, c);
                        return T_PERIOD;
                        break;
                    case MSCT:
                        if (strbuf_size(sc->tokbuf) == 0) {
                                return make_sctoken(c);
                        } else {
                                /* do the assembled multi-character token
                                 * first */
                                port_ungetc(sc->in, c);
                        }
                        /* FALLTHROUGH */
                    case FINI:
                        return make_atom(state, sc);
                    case ERRR:
                        return lexer_error(state, c, sc);
                    default:
                        fprintf(stderr,
                                "%s:%d:%d: invalid lexer action %d, c = %d\n",
                                sc->name, sc->lineno, sc->column,
                                action[charclass(c, sc)][state], c);
                        exit(EX_SOFTWARE);
                        break;
                }
                state = newstate[class][state];
        }
        return T_LERROR;
}

/**
   The grammar.

   sexpr        ::= T_ISATOM | compound
   compound     ::= proper_list | improper_list | vector | map
   proper_list  ::= T_OPAREN sexpr* T_CPAREN
   improper_list::= T_OPAREN sexpr+ PERIOD sexpr T_CPAREN
   vector       ::= T_OBRACK sexpr* T_CBRACK
   map          ::= T_OBRACE {T_ISATOM | proper_list | improper list}+ T_CBRACE
*/

typedef enum R_STATE { r_initial } r_state_t;


obp_t do_special(char *name, session_context_t *sc)
{
        PROTECT;
        PROTVAR(arg);
        PROTVAR(argpair);
        PROTVAR(retval);

        arg = read_expr(sc);
        if (!arg) {
                ERROR(sc->out, ERR_RSYNTAX, 0,
                      "%s:%d:%d: unexpected eof",
                      sc->name, sc->lineno, sc->column);
        }
        argpair = new_pair(arg, the_Nil);
        retval = new_pair(intern_z(name), argpair);
    EXIT:
        UNPROTECT;
        return retval;
}


/**
 * return a list of read expressions
 */
obp_t read_loop(token_t end_token, uint *n_exprs, session_context_t *sc)
{
        PROTECT;
        uint n = 0;
        PROTVAR(retval);                  /* the whole collected list */
        PROTVAR(last);                    /* last element of the list */
        PROTVAR(expr);
        PROTVAR(pair);

        while (1) {
                token_t token = read_next_token(sc);
                if (token == end_token) {
                        *n_exprs = n;
                        break;
                }
                if (token == T_ENDOFF) {
                        ERROR(sc->out, ERR_RSYNTAX, 0,
                              "%s:%d:%d: unexpected eof",
                              sc->name, sc->lineno, sc->column);
                }
                if (token == T_PERIOD) {
                        if (end_token != T_CPAREN || last == the_Nil) {
                                ERROR(sc->out, ERR_RSYNTAX, 0,
                                      "%s:%d:%d: unexpected period",
                                      sc->name, sc->lineno, sc->column);
                        }
                        expr = read_expr(sc);
                        CHECK_ERROR(expr);
                        CDR(last) = expr;
                        /* now we need the closing paren */
                        token = read_next_token(sc);
                        if (token != T_CPAREN) {
                                ERROR(sc->out, ERR_RSYNTAX, 0,
                                      "%s:%d:%d: unexpected %s", sc->name,
                                      sc->lineno, sc->column, token_name[token]);
                        }
                        break;
                }

                pushback(token, sc);
                expr = read_expr(sc);
                CHECK_ERROR(expr);
                pair = new_pair(expr, the_Nil);
                if (retval == the_Nil) {
                        retval = pair;
                } else {
                        CDR(last) = pair;
                }
                last = pair;
                n++;
        }
    EXIT:
        UNPROTECT;
        return retval;
}


obp_t reader_macro(session_context_t *sc)
{
        token_t nextt = read_next_token(sc);
        PROTECT;
        PROTVAR(retval);
        
        switch (nextt) {
            case T_SQUOTE: return do_special(FUNCTION_NAME, sc);
                /* others to follow here */
            case T_ENDOFF: ERROR(sc->out, ERR_RSYNTAX, 0,
                                 "%s:%d:%d: unexpected eof",
                                 sc->name, sc->lineno, sc->column);
            default: ERROR(sc->out, ERR_RSYNTAX, 0,
                           "%s:%d:%d: unknown reader macro, # and %s",
                           sc->name, sc->lineno, sc->column,
                           token_name[nextt]);
        }
    EXIT:
        UNPROTECT;
        return retval;
}


obp_t read_expr(session_context_t *sc)
{
        PROTECT;
        uint nelem = 0;
        PROTVAR(retval);
        PROTVAR(kvpairs);
        PROTVAR(map);
        PROTVAR(first);
        PROTVAR(elems);
        PROTVAR(vec);
        
        token_t token = read_next_token(sc);
        switch (token) {
            case T_ISATOM:
                retval = sc->tok_atom;
                break;
            case T_OPAREN:
                retval = read_loop(T_CPAREN, &nelem, sc);
                break;
            case T_CPAREN:
                ERROR(sc->out, ERR_RSYNTAX, 0,
                      "%s:%d:%d: unexpected close paren",
                      sc->name, sc->lineno, sc->column);
            case T_OBRACE:
                kvpairs = read_loop(T_CBRACE, &nelem, sc);
                CHECK_ERROR(kvpairs);
                map = new_map(EQ_EQV, 0);
                hashmap_t hashmap = AS(map, MAP)->map;
                while (kvpairs != the_Nil) {
                        first = CAR(kvpairs);
                        hashmap_put(hashmap, CAR(first), CDR(first));
                        kvpairs = CDR(kvpairs);
                }
                retval = map;
                break;
            case T_CBRACE:
                ERROR(sc->out, ERR_RSYNTAX, 0,
                      "%s:%d:%d: unexpected close brace",
                      sc->name, sc->lineno, sc->column);
            case T_OBRACK:
                elems = read_loop(T_CBRACE, &nelem, sc);
                CHECK_ERROR(elems);
                vec = new_vector(nelem);
                while (elems != the_Nil) {
                        vector_append(vec, CAR(elems));
                        elems = CDR(elems);
                }
                retval = vec;
                break;
            case T_CBRACK:
                ERROR(sc->out, ERR_RSYNTAX, 0,
                      "%s:%d:%d: unexpected close bracket",
                      sc->name, sc->lineno, sc->column);
            case T_SQUOTE:
                retval = do_special(QUOTE_NAME, sc);
                break;
            case T_QQUOTE:
                retval = do_special(QUASIQUOTE_NAME, sc);
                break;
            case T_UNQUOT:
                retval = do_special(UNQUOTE_NAME, sc);
                break;
            case T_SPLICE:
                retval = do_special(SPLICE_NAME, sc);
                break;
            case T_PERIOD:
                ERROR(sc->out, ERR_RSYNTAX, 0, "unexpected period");
            case T_ENDOFF:
                retval = 0;
                break;
            case T_LERROR:
                retval = sc->tok_atom;
                break;
            case T_RMACRO:
                retval = reader_macro(sc);
                break;
            default:
                fprintf(stderr, "%s:%d:%d: invalid token type %d in parser",
                        sc->name, sc->lineno, sc->column, token);
                exit(EX_SOFTWARE);
                break;
        }
    EXIT:
        UNPROTECT;
        return retval;
}


void init_reader(void)
{
}


/* EOF */
