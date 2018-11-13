/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "cbasics.h"
#include <math.h>
#include <limits.h>
#include "objects.h"
#include "signals.h"
#include "builtins.h"
#include "names.h"
#include "numbers.h"


#define INTRANGE(value) ((value) >= LONG_MIN && (value) <= LONG_MAX)

/**
 * Return the sum of all arguments, which must be numbers.
 * (+ [n1 ...])
 */
obp_t bf_plus(int nargs, obp_t args, session_context_t *sc, int level)
{
        long double value = 0;
        int is_int = 1;
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                value += AS(arg, NUMBER)->value;
                is_int &= IS_INT(arg) && INTRANGE(value);
                args = CDR(args);
        }
        obp_t newval = new_ldouble(value);
        IS_INT(newval) = is_int;
        return newval;
}

/**
 * Return a number that is one more than its argument number.
 * (1+ number)
 */
obp_t bf_successor(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        long double value = AS(arg, NUMBER)->value + 1;
        obp_t newval = new_ldouble(value);
        IS_INT(newval) = IS_INT(arg) && INTRANGE(value);
        return newval;
}


/**
 * Return a number that is one less than its argument number.
 * (1+ number)
 */
obp_t bf_predecessor(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        long double value = AS(arg, NUMBER)->value - 1;
        obp_t newval = new_ldouble(value);
        IS_INT(newval) = IS_INT(arg) && INTRANGE(value);
        return newval;
}


/**
 * Return the numeric value of the first argument minus all others.
 * (- n1 [n2 ...])
 */
obp_t bf_minus(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t first = CAR(args);
        CHECKTYPE_RET(sc->out, first, NUMBER);

        long double value = AS(first, NUMBER)->value;
        int is_int = 1;
        args = CDR(args);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                value -= AS(arg, NUMBER)->value;
                is_int &= IS_INT(arg) && INTRANGE(value);
                args = CDR(args);
        }
        obp_t newval = new_ldouble(value);
        IS_INT(newval) = is_int;
        return newval;
}

/**
 * Return the product of all arguments, which must be numbers.
 * (* [n1 ...])
 */
obp_t bf_times(int nargs, obp_t args, session_context_t *sc, int level)
{
        long double value = 1;
        int is_int = 1;
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                value *= AS(arg, NUMBER)->value;
                is_int &= IS_INT(arg) && INTRANGE(value);
                args = CDR(args);
        }
        obp_t newval = new_ldouble(value);
        IS_INT(newval) = is_int;
        return newval;
}

/**
 * Return the numeric value of the first argument divided by all others.
 * (/ n1 [n2 ...])
 */
obp_t bf_divide(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t start = CAR(args);
        CHECKTYPE_RET(sc->out, start, NUMBER);

        long double value = AS(start, NUMBER)->value;
        int is_int = 1;
        args = CDR(args);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                value /= AS(arg, NUMBER)->value;
                is_int &= IS_INT(arg) && remainderl(value, 1.0) == 0;
                args = CDR(args);
        }
        obp_t newval = new_ldouble(value);
        IS_INT(newval) = is_int;
        return newval;
}

/**
 * Return the numeric value of the first argument modulo all others.
 * (% n1 [n2 ...])
 */
obp_t bf_modulo(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t start = CAR(args);
        CHECKTYPE_RET(sc->out, start, NUMBER);

        long double value = AS(start, NUMBER)->value;
        int is_int = 1;
        args = CDR(args);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                value = fmodl(value, AS(arg, NUMBER)->value);
                is_int &= IS_INT(arg) && remainderl(value, 1.0) == 0;
                args = CDR(args);
        }
        obp_t newval = new_ldouble(value);
        IS_INT(newval) = is_int && remainderl(value, 1.0) == 0;
        return newval;
}

/**
 * Return t iff all arguments, which must be numbers, have the same value.
 * (= n1 [n2 ...])
 */
obp_t bf_equals(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t start = CAR(args);
        CHECKTYPE_RET(sc->out, start, NUMBER);

        long value = AS(start, NUMBER)->value;
        args = CDR(args);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                if (value != AS(arg, NUMBER)->value) {
                        return the_Nil;
                }
                args = CDR(args);
        }
        return the_T;
}

/**
 * Return t iff each argument, which must be a number, is greater than the one on
 * its right side.
 * (> n1 [n2 ...])
 */
obp_t bf_greater(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t start = CAR(args);
        CHECKTYPE_RET(sc->out, start, NUMBER);

        long double value = AS(start, NUMBER)->value;
        args = CDR(args);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                long newarg = AS(arg, NUMBER)->value;
                //printf("%ld > %ld? %d\n", value, newarg, value > newarg);
                if (!(value > newarg)) {
                        return the_Nil;
                }
                value = newarg;
                args = CDR(args);
        }
        return the_T;
}


/**
 * Return t iff each argument, which must be a number, is greater than or equal
 * to the one on its right side.
 * (>= n1 [n2 ...])
 */
obp_t bf_greatere(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t start = CAR(args);
        CHECKTYPE_RET(sc->out, start, NUMBER);

        long double value = AS(start, NUMBER)->value;
        args = CDR(args);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                int newarg = AS(arg, NUMBER)->value;
                if (!(value >= newarg)) {
                        return the_Nil;
                }
                value = newarg;
                args = CDR(args);
        }
        return the_T;
}


/**
 * Return t iff each argument, which must be a number, is less than the one on
 * its right side.
 * (< n1 [n2 ...])
 */
obp_t bf_less(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t start = CAR(args);
        CHECKTYPE_RET(sc->out, start, NUMBER);

        long double value = AS(start, NUMBER)->value;
        args = CDR(args);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                int newarg = AS(arg, NUMBER)->value;
                if (!(value < newarg)) {
                        return the_Nil;
                }
                value = newarg;
                args = CDR(args);
        }
        return the_T;
}

/**
 * Return t iff each argument, which must be a number, is less than or equal to
 * the one on its right side.
 * (< n1 [n2 ...])
 */
obp_t bf_lesse(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t start = CAR(args);
        CHECKTYPE_RET(sc->out, start, NUMBER);

        long double value = AS(start, NUMBER)->value;
        args = CDR(args);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                CHECKTYPE_RET(sc->out, arg, NUMBER);
                int newarg = AS(arg, NUMBER)->value;
                if (!(value <= newarg)) {
                        return the_Nil;
                }
                value = newarg;
                args = CDR(args);
        }
        return the_T;
}

/**
 * Return t iff the argument is equal to zero.
 * (zerop n1)
 */
obp_t bf_zerop(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        CHECKTYPE_RET(sc->out, arg, NUMBER);
        return (AS(arg, NUMBER)->value == 0 ? the_T : the_Nil);
}



void init_numbers(void)
{
        register_builtin(PLUS_NAME, bf_plus, 0, 0, -1);
        register_builtin(MINUS_NAME, bf_minus, 0, 2, -1);
        register_builtin(TIMES_NAME, bf_times, 0, 0, -1);
        register_builtin(DIVIDE_NAME, bf_divide, 0, 2, -1);
        register_builtin(MODULO_NAME, bf_modulo, 0, 2, -1);
        register_builtin(EQUALS_NAME, bf_equals, 0, 2, -1);
        register_builtin(GREATER_NAME, bf_greater, 0, 2, -1);
        register_builtin(GREATERE_NAME, bf_greatere, 0, 2, -1);
        register_builtin(LESS_NAME, bf_less, 0, 2, -1);
        register_builtin(LESSE_NAME, bf_lesse, 0, 2, -1);
        register_builtin(ZEROP_NAME, bf_zerop, 0, 1, 1);
        register_builtin(SUCCESSOR_NAME, bf_successor, 0, 1, 1);
        register_builtin(PREDECESSOR_NAME, bf_predecessor, 0, 1, 1);
}
