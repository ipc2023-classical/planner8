/*** Adapted from https://github.com/cktan/tomlc99 */
/**
  MIT License

  Copyright (c) CK Tan
  https://github.com/cktan/tomlc99

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
#ifndef _PDDL_TOML_H_
#define _PDDL_TOML_H_

#include <pddl/common.h>
#include <pddl/err.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct pddl_toml_timestamp_t pddl_toml_timestamp_t;
typedef struct pddl_toml_table_t pddl_toml_table_t;
typedef struct pddl_toml_array_t pddl_toml_array_t;
typedef struct pddl_toml_datum_t pddl_toml_datum_t;

/* Parse a file. Return a table on success, or 0 otherwise.
 * Caller must pddl_toml_free(the-return-value) after use.
 */
pddl_toml_table_t *pddl_toml_parse_file(FILE *fp, pddl_err_t *err);

/* Parse a string containing the full config.
 * Return a table on success, or 0 otherwise.
 * Caller must pddl_toml_free(the-return-value) after use.
 */
pddl_toml_table_t *pddl_toml_parse(char *conf, /* NUL terminated, please. */
                                   pddl_err_t *err);

/* Free the table returned by pddl_toml_parse() or pddl_toml_parse_file(). Once
 * this function is called, any handles accessed through this tab
 * directly or indirectly are no longer valid.
 */
void pddl_toml_free(pddl_toml_table_t *tab);

/* Timestamp types. The year, month, day, hour, minute, second, z
 * fields may be NULL if they are not relevant. e.g. In a DATE
 * type, the hour, minute, second and z fields will be NULLs.
 */
struct pddl_toml_timestamp_t {
  struct { /* internal. do not use. */
    int year, month, day;
    int hour, minute, second, millisec;
    char z[10];
  } __buffer;
  int *year, *month, *day;
  int *hour, *minute, *second, *millisec;
  char *z;
};

/*-----------------------------------------------------------------
 *  Enhanced access methods
 */
struct pddl_toml_datum_t {
  int ok;
  union {
    pddl_toml_timestamp_t *ts; /* ts must be freed after use */
    char *s;              /* string value. s must be freed after use */
    int b;                /* bool value */
    int64_t i;            /* int value */
    double d;             /* double value */
  } u;
};

/* on arrays: */
/* ... retrieve size of array. */
int pddl_toml_array_nelem(const pddl_toml_array_t *arr);
/* ... retrieve values using index. */
pddl_toml_datum_t pddl_toml_string_at(const pddl_toml_array_t *arr, int idx);
pddl_toml_datum_t pddl_toml_bool_at(const pddl_toml_array_t *arr, int idx);
pddl_toml_datum_t pddl_toml_int_at(const pddl_toml_array_t *arr, int idx);
pddl_toml_datum_t pddl_toml_double_at(const pddl_toml_array_t *arr, int idx);
pddl_toml_datum_t pddl_toml_timestamp_at(const pddl_toml_array_t *arr, int idx);
/* ... retrieve array or table using index. */
pddl_toml_array_t *pddl_toml_array_at(const pddl_toml_array_t *arr, int idx);
pddl_toml_table_t *pddl_toml_table_at(const pddl_toml_array_t *arr, int idx);

/* on tables: */
/* ... retrieve the key in table at keyidx. Return 0 if out of range. */
const char *pddl_toml_key_in(const pddl_toml_table_t *tab, int keyidx);
/* ... returns 1 if key exists in tab, 0 otherwise */
int pddl_toml_key_exists(const pddl_toml_table_t *tab, const char *key);
/* ... retrieve values using key. */
pddl_toml_datum_t pddl_toml_string_in(const pddl_toml_table_t *arr,
                                        const char *key);
pddl_toml_datum_t pddl_toml_bool_in(const pddl_toml_table_t *arr, const char *key);
pddl_toml_datum_t pddl_toml_int_in(const pddl_toml_table_t *arr, const char *key);
pddl_toml_datum_t pddl_toml_double_in(const pddl_toml_table_t *arr,
                                        const char *key);
pddl_toml_datum_t pddl_toml_timestamp_in(const pddl_toml_table_t *arr,
                                           const char *key);
/* .. retrieve array or table using key. */
pddl_toml_array_t *pddl_toml_array_in(const pddl_toml_table_t *tab,
                                        const char *key);
pddl_toml_table_t *pddl_toml_table_in(const pddl_toml_table_t *tab,
                                        const char *key);

/*-----------------------------------------------------------------
 * lesser used
 */
/* Return the array kind: 't'able, 'a'rray, 'v'alue, 'm'ixed */
char pddl_toml_array_kind(const pddl_toml_array_t *arr);

/* For array kind 'v'alue, return the type of values
   i:int, d:double, b:bool, s:string, t:time, D:date, T:timestamp, 'm'ixed
   0 if unknown
*/
char pddl_toml_array_type(const pddl_toml_array_t *arr);

/* Return the key of an array */
const char *pddl_toml_array_key(const pddl_toml_array_t *arr);

/* Return the number of key-values in a table */
int pddl_toml_table_nkval(const pddl_toml_table_t *tab);

/* Return the number of arrays in a table */
int pddl_toml_table_narr(const pddl_toml_table_t *tab);

/* Return the number of sub-tables in a table */
int pddl_toml_table_ntab(const pddl_toml_table_t *tab);

/* Return the key of a table*/
const char *pddl_toml_table_key(const pddl_toml_table_t *tab);

/*--------------------------------------------------------------
 * misc
 */
int pddl_toml_utf8_to_ucs(const char *orig, int len, int64_t *ret);
int pddl_toml_ucs_to_utf8(int64_t code, char buf[6]);

/*--------------------------------------------------------------
 *  deprecated
 */
/* A raw value, must be processed by pddl_toml_rto* before using. */
typedef const char *pddl_toml_raw_t;
pddl_toml_raw_t pddl_toml_raw_in(const pddl_toml_table_t *tab, const char *key);
pddl_toml_raw_t pddl_toml_raw_at(const pddl_toml_array_t *arr, int idx);
int pddl_toml_rtos(pddl_toml_raw_t s, char **ret);
int pddl_toml_rtob(pddl_toml_raw_t s, int *ret);
int pddl_toml_rtoi(pddl_toml_raw_t s, int64_t *ret);
int pddl_toml_rtod(pddl_toml_raw_t s, double *ret);
int pddl_toml_rtod_ex(pddl_toml_raw_t s, double *ret, char *buf, int buflen);
int pddl_toml_rtots(pddl_toml_raw_t s, pddl_toml_timestamp_t *ret);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PDDL_TOML_H_ */
