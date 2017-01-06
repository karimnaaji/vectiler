
#ifndef _FLAG_H_
#define _FLAG_H_

#include <stdio.h>
#include <stdbool.h>

/*
 * Max flags that may be defined.
 */

#define FLAGS_MAX 128

/*
 * Max arguments supported for set->argv.
 */

#define FLAGS_MAX_ARGS 128

/*
 * Flag errors.
 */

typedef enum {
  FLAG_OK,
  FLAG_ERROR_PARSING,
  FLAG_ERROR_ARG_MISSING,
  FLAG_ERROR_UNDEFINED_FLAG
} flag_error;

/*
 * Flag types supported.
 */

typedef enum {
  FLAG_TYPE_INT,
  FLAG_TYPE_FLOAT,
  FLAG_TYPE_BOOL,
  FLAG_TYPE_STRING
} flag_type;

/*
 * Flag represents as single user-defined
 * flag with a name, help description,
 * type, and pointer to a value which
 * is replaced upon precense of the flag.
 */

typedef struct {
  const char *name;
  const char *help;
  flag_type type;
  void *value;
} flag_t;

/*
 * Flagset contains a number of flags,
 * and is populated wth argc / argv with the
 * remaining arguments.
 *
 * In the event of an error the error union
 * is populated with either the flag or the
 * associated argument.
 */

typedef struct {
  const char *usage;
  int nflags;
  flag_t flags[FLAGS_MAX];
  int argc;
  const char *argv[FLAGS_MAX_ARGS];
  union {
    flag_t *flag;
    const char *arg;
  } error;
} flagset_t;

// Flagset.

flagset_t *
flagset_singleton();

flagset_t *
flagset_new();

void
flagset_free(flagset_t *self);

void
flagset_int(flagset_t *self, int *value, const char *name, const char *help);

void
flagset_float(flagset_t *self, float *value, const char *name, const char *help);

void
flagset_bool(flagset_t *self, bool *value, const char *name, const char *help);

void
flagset_string(flagset_t *self, const char **value, const char *name, const char *help);

void
flagset_write_usage(flagset_t *self, FILE *fp, const char *name);

flag_error
flagset_parse(flagset_t *self, int argc, const char **args);

// Singleton.

void
flag_int(int *value, const char *name, const char *help);

void
flag_bool(bool *value, const char *name, const char *help);

void
flag_string(const char **value, const char *name, const char *help);

void
flag_parse(int argc, const char **args, const char *version, int reqargc);

void
flag_usage(const char *msg);

#define flag_str flag_string

#endif /* _FLAG_H_ */