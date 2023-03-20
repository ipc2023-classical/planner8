#ifndef OPTS_H
#define OPTS_H

#include <pddl/pddl.h>

typedef void (*opts_params_flag_fn)(int value, void *dst);
typedef void (*opts_params_int_fn)(int value, void *dst);
typedef void (*opts_params_flt_fn)(float value, void *dst);

struct opts_param {
    char *name;
    void *dst;
    opts_params_flag_fn flag_fn;
    opts_params_int_fn int_fn;
    opts_params_flt_fn flt_fn;
    int switch_size;
    char **switch_tag;
    int *switch_ival;
    int is_int;
    int is_flt;
    int is_flag;
    int is_int_switch;
    int is_str;
};
typedef struct opts_param opts_param_t;

struct opts_params {
    opts_param_t *param;
    int param_size;
    int param_alloc;
};
typedef struct opts_params opts_params_t;


void optsFree(void);

void optsStartGroup(const char *header);

void optsAddFlag(const char *long_name,
                 char short_name,
                 int *set,
                 int default_value,
                 const char *desc);

void optsAddFlagFn(const char *long_name,
                   char short_name,
                   int (*fn)(int enabled),
                   const char *desc);

void optsAddFlagFn2(const char *long_name,
                    char short_name,
                    void (*fn)(void),
                    const char *desc);

void optsAddInt(const char *long_name,
                char short_name,
                int *set,
                int default_value,
                const char *desc);

void optsAddFlt(const char *long_name,
                char short_name,
                float *set,
                float default_value,
                const char *desc);

void optsAddFltFn(const char *long_name,
                  char short_name,
                  int (*fn)(float v),
                  const char *desc);

void optsAddStr(const char *long_name,
                char short_name,
                char **set,
                const char *default_value,
                const char *desc);

void optsAddStrFn(const char *long_name,
                 char short_name,
                 int (*fn)(const char *v),
                 const char *desc);

void optsAddTags(const char *long_name,
                 char short_name,
                 const char *default_value,
                 int (*fn)(const char *tag),
                 const char *desc);

opts_params_t *optsAddParams(const char *long_name,
                             char short_name,
                             const char *desc);

opts_params_t *optsAddParamsAndFn(const char *long_name,
                                  char short_name,
                                  const char *desc,
                                  void *ud,
                                  void (*fn)(void *ud));

void optsAddIntSwitch(const char *long_name,
                      char short_name,
                      int *set,
                      const char *desc,
                      int size, ...);


int opts(int *argc, char **argv);
void optsPrint(FILE *fout);

int optsProcessTags(const char *_s, int (*fn)(const char *t));

void optsParamsInit(opts_params_t *params);
void optsParamsFree(opts_params_t *params);
void optsParamsAddInt(opts_params_t *params, const char *name, int *dst);
void optsParamsAddFlt(opts_params_t *params, const char *name, float *dst);
void optsParamsAddFlag(opts_params_t *params, const char *name, int *dst);
void optsParamsAddStr(opts_params_t *params, const char *name, char **dst);
void optsParamsAddIntFn(opts_params_t *params, const char *name, void *dst,
                        opts_params_int_fn fn);
void optsParamsAddFltFn(opts_params_t *params, const char *name, void *dst,
                        opts_params_flt_fn fn);
void optsParamsAddFlagFn(opts_params_t *params, const char *name, void *dst,
                         opts_params_flag_fn fn);
void optsParamsAddIntSwitch(opts_params_t *params,
                            const char *name,
                            void *dst,
                            int size, ...);
int optsParamsParse(opts_params_t *params, const char *text);

#endif /* OPTS_H */
