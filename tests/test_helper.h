#ifndef TEST_HELPER
#define TEST_HELPER

// This is a subset of the very expansive cmocka library focused on simple asserts

#include <inttypes.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SOURCE_LOCATION_FORMAT "%s:%u"

# if __WORDSIZE == 64
#  define LargestIntegralType unsigned long int
# else
#  define LargestIntegralType unsigned long long int
# endif

# ifdef _WIN32
#  define LargestIntegralTypePrintfFormat "0x%I64x"
# else
#  if __WORDSIZE == 64
#   define LargestIntegralTypePrintfFormat "%ld"
#  else
#   define LargestIntegralTypePrintfFormat "%lld"
#  endif
# endif /* _WIN32 */

/* Perform an unsigned cast to uintptr_t. */
#define cast_to_pointer_integral_type(value) \
    ((uintptr_t)((size_t)(value)))

/* Perform a cast of a pointer to LargestIntegralType */
#define cast_ptr_to_largest_integral_type(value) \
cast_to_largest_integral_type(cast_to_pointer_integral_type(value))

/* Perform an unsigned cast to LargestIntegralType. */
#define cast_to_largest_integral_type(value) \
    ((LargestIntegralType)(value))


void vprint_error(const char* const format, va_list args) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    fprintf(stderr, "%s", buffer);
    fflush(stderr);
}


void cm_print_error(const char * const format, ...)
{
    va_list args;
    va_start(args, format);
    vprint_error(format, args);
    va_end(args);
}

/* Returns 1 if the specified values are equal.  If the values are not equal
 * an error is displayed and 0 is returned. */
static int values_equal_display_error(const LargestIntegralType left,
                                      const LargestIntegralType right) {
    const int equal = left == right;
    if (!equal) {
        cm_print_error(LargestIntegralTypePrintfFormat " != "
                       LargestIntegralTypePrintfFormat "\n", left, right);
    }
    return equal;
}

/*
 * Returns 1 if the specified values are not equal.  If the values are equal
 * an error is displayed and 0 is returned. */
static int values_not_equal_display_error(const LargestIntegralType left,
                                          const LargestIntegralType right) {
    const int not_equal = left != right;
    if (!not_equal) {
        cm_print_error(LargestIntegralTypePrintfFormat " == "
                       LargestIntegralTypePrintfFormat "\n", left, right);
    }
    return not_equal;
}


/*
 * Determine whether the specified strings are equal.  If the strings are equal
 * 1 is returned.  If they're not equal an error is displayed and 0 is
 * returned.
 */
static int string_equal_display_error(
        const char * const left, const char * const right) {
    if (strcmp(left, right) == 0) {
        return 1;
    }
    cm_print_error("\"%s\" != \"%s\"\n", left, right);
    return 0;
}


/*
 * Determine whether the specified strings are equal.  If the strings are not
 * equal 1 is returned.  If they're not equal an error is displayed and 0 is
 * returned
 */
static int string_not_equal_display_error(
        const char * const left, const char * const right) {
    if (strcmp(left, right) != 0) {
        return 1;
    }
    cm_print_error("\"%s\" == \"%s\"\n", left, right);
    return 0;
}


void _fail(const char * const file, const int line) {
    cm_print_error(SOURCE_LOCATION_FORMAT ": error: Failure!\n", file, line);
    exit(-1);
}

void _assert_true(const LargestIntegralType result,
                  const char * const expression,
                  const char * const file, const int line) {
    if (!result) {
        cm_print_error("%s\n", expression);
        _fail(file, line);
    }
}

void _assert_int_equal(
        const LargestIntegralType a, const LargestIntegralType b,
        const char * const file, const int line) {
    if (!values_equal_display_error(a, b)) {
        _fail(file, line);
    }
}


void _assert_int_not_equal(
        const LargestIntegralType a, const LargestIntegralType b,
        const char * const file, const int line) {
    if (!values_not_equal_display_error(a, b)) {
        _fail(file, line);
    }
}


void _assert_string_equal(const char * const a, const char * const b,
                          const char * const file, const int line) {
    if (!string_equal_display_error(a, b)) {
        _fail(file, line);
    }
}


void _assert_string_not_equal(const char * const a, const char * const b,
                              const char *file, const int line) {
    if (!string_not_equal_display_error(a, b)) {
        _fail(file, line);
    }
}

#define assert_true(c) _assert_true(cast_to_largest_integral_type(c), #c, \
                                    __FILE__, __LINE__)

#define assert_false(c) _assert_true(!(cast_to_largest_integral_type(c)), #c, \
                                     __FILE__, __LINE__)

#define assert_null(c) _assert_true(!(cast_ptr_to_largest_integral_type(c)), #c, \
                                    __FILE__, __LINE__)

#define assert_non_null(c) _assert_true(cast_ptr_to_largest_integral_type(c), #c, \
                                        __FILE__, __LINE__)

#define assert_ptr_not_equal(a, b) \
    _assert_int_not_equal(cast_ptr_to_largest_integral_type(a), \
                          cast_ptr_to_largest_integral_type(b), \
                          __FILE__, __LINE__)

#define assert_int_equal(a, b) \
    _assert_int_equal(cast_to_largest_integral_type(a), \
                      cast_to_largest_integral_type(b), \
                      __FILE__, __LINE__)

#define assert_int_not_equal(a, b) \
    _assert_int_not_equal(cast_to_largest_integral_type(a), \
                          cast_to_largest_integral_type(b), \
                          __FILE__, __LINE__)

#define assert_string_equal(a, b) \
    _assert_string_equal((const char*)(a), (const char*)(b), __FILE__, \
                         __LINE__)

#define assert_string_not_equal(a, b) \
    _assert_string_not_equal((const char*)(a), (const char*)(b), __FILE__, \
                             __LINE__)

#define EPSILON 0.4 //Yea, absurde episilon, but testing some large numbers
#define assert_float_equal(a, b) assert_true( ((a - b) < EPSILON && (b - a) < EPSILON) )

#endif
