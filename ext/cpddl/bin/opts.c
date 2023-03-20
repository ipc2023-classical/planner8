#include "opts.h"

#define FLAG 1
#define INT 2
#define FLT 3
#define STR 4
#define STR_TAGS 5
#define FLAG_FN 6
#define FLAG_FN2 7
#define PARAMS 8
#define PARAMS_AND_FN 9
#define INT_SWITCH 10
#define FLT_FN 11
#define STR_FN 12


struct opt_group {
    char *header;
};
typedef struct opt_group opt_group_t;

struct opt_opt {
    int type;
    int group;
    char *long_name;
    char short_name;
    int idefault;
    float fdefault;
    char *sdefault;
    void *set;
    char *desc;
    int (*parse_tags)(const char *tag);
    int (*flag_fn)(int);
    void (*flag_fn2)(void);
    int (*flt_fn)(float);
    int (*str_fn)(const char *);
    opts_params_t params;
    void (*params_fn)(void *ud);
    void *params_userdata;

    int switch_size;
    char **switch_tag;
    int *switch_ival;
};
typedef struct opt_opt opt_opt_t;

struct opt {
    opt_group_t *group;
    int group_size;
    int group_alloc;

    opt_opt_t *opt;
    int opt_size;
    int opt_alloc;
};
typedef struct opt opt_t;

static opt_t o = { 0 };

static int cur_group = -1;

void optsFree(void)
{
    for (int i = 0; i < o.group_size; ++i){
        PDDL_FREE(o.group[i].header);
    }
    if (o.group != NULL)
        PDDL_FREE(o.group);

    for (int i = 0; i < o.opt_size; ++i){
        if (o.opt[i].long_name != NULL)
            PDDL_FREE(o.opt[i].long_name);
        if (o.opt[i].sdefault != NULL)
            PDDL_FREE(o.opt[i].sdefault);
        if (o.opt[i].desc != NULL)
            PDDL_FREE(o.opt[i].desc);
        optsParamsFree(&o.opt[i].params);
        for (int i = 0; i < o.opt[i].switch_size; ++i)
            PDDL_FREE(o.opt[i].switch_tag[i]);
        if (o.opt[i].switch_tag != NULL)
            PDDL_FREE(o.opt[i].switch_tag);
        if (o.opt[i].switch_ival != NULL)
            PDDL_FREE(o.opt[i].switch_ival);
    }
    if (o.opt != NULL)
        PDDL_FREE(o.opt);
}

void optsStartGroup(const char *header)
{
    if (o.group_size == o.group_alloc){
        if (o.group_alloc == 0)
            o.group_alloc = 1;
        o.group_alloc *= 2;
        o.group = PDDL_REALLOC_ARR(o.group, opt_group_t, o.group_alloc);
    }

    cur_group = o.group_size;
    opt_group_t *g = o.group + o.group_size++;
    g->header = PDDL_STRDUP(header);
}

static opt_opt_t *optsAdd(int type,
                          const char *long_name,
                          char short_name,
                          void *set,
                          const char *desc)
{
    if (o.opt_size == o.opt_alloc){
        if (o.opt_alloc == 0)
            o.opt_alloc = 1;
        o.opt_alloc *= 2;
        o.opt = PDDL_REALLOC_ARR(o.opt, opt_opt_t, o.opt_alloc);
    }

    opt_opt_t *opt = o.opt + o.opt_size++;
    bzero(opt, sizeof(*opt));
    opt->group = cur_group;
    opt->type = type;
    if (long_name != NULL)
        opt->long_name = PDDL_STRDUP(long_name);
    opt->short_name = short_name;
    opt->set = set;
    if (desc != NULL)
        opt->desc = PDDL_STRDUP(desc);
    return opt;
}

void optsAddFlag(const char *long_name,
                 char short_name,
                 int *set,
                 int default_value,
                 const char *desc)
{
    opt_opt_t *opt = optsAdd(FLAG, long_name, short_name, set, desc);
    opt->idefault = default_value;
    *(int *)set = default_value;
}

void optsAddFlagFn(const char *long_name,
                   char short_name,
                   int (*fn)(int),
                   const char *desc)
{
    opt_opt_t *opt = optsAdd(FLAG_FN, long_name, short_name, NULL, desc);
    opt->flag_fn = fn;
}

void optsAddFlagFn2(const char *long_name,
                    char short_name,
                    void (*fn)(void),
                    const char *desc)
{
    opt_opt_t *opt = optsAdd(FLAG_FN2, long_name, short_name, NULL, desc);
    opt->flag_fn2 = fn;
}

void optsAddInt(const char *long_name,
                char short_name,
                int *set,
                int default_value,
                const char *desc)
{
    opt_opt_t *opt = optsAdd(INT, long_name, short_name, set, desc);
    opt->idefault = default_value;
    *(int *)set = default_value;
}

void optsAddFlt(const char *long_name,
                char short_name,
                float *set,
                float default_value,
                const char *desc)
{
    opt_opt_t *opt = optsAdd(FLT, long_name, short_name, set, desc);
    opt->fdefault = default_value;
    *(float *)set = default_value;
}

void optsAddFltFn(const char *long_name,
                  char short_name,
                  int (*fn)(float),
                  const char *desc)
{
    opt_opt_t *opt = optsAdd(FLT_FN, long_name, short_name, NULL, desc);
    opt->flt_fn = fn;
}

void optsAddStr(const char *long_name,
                char short_name,
                char **set,
                const char *default_value,
                const char *desc)
{
    opt_opt_t *opt = optsAdd(STR, long_name, short_name, set, desc);
    if (default_value != NULL){
        opt->sdefault = PDDL_STRDUP(default_value);
        *(char **)set = opt->sdefault;
    }
}

void optsAddStrFn(const char *long_name,
                  char short_name,
                  int (*fn)(const char *),
                  const char *desc)
{
    opt_opt_t *opt = optsAdd(STR_FN, long_name, short_name, NULL, desc);
    opt->str_fn = fn;
}

void optsAddTags(const char *long_name,
                 char short_name,
                 const char *default_value,
                 int (*fn)(const char *tag),
                 const char *desc)
{
    opt_opt_t *opt = optsAdd(STR_TAGS, long_name, short_name, NULL, desc);
    if (default_value != NULL){
        opt->sdefault = PDDL_STRDUP(default_value);
    }
    opt->parse_tags = fn;
}

opts_params_t *optsAddParams(const char *long_name,
                             char short_name,
                             const char *desc)
{
    opt_opt_t *opt = optsAdd(PARAMS, long_name, short_name, NULL, desc);
    optsParamsInit(&opt->params);
    return &opt->params;
}

opts_params_t *optsAddParamsAndFn(const char *long_name,
                                  char short_name,
                                  const char *desc,
                                  void *ud,
                                  void (*fn)(void *ud))
{
    opt_opt_t *opt = optsAdd(PARAMS_AND_FN, long_name, short_name, NULL, desc);
    optsParamsInit(&opt->params);
    opt->params_fn = fn;
    opt->params_userdata = ud;
    return &opt->params;
}

void optsAddIntSwitch(const char *long_name,
                      char short_name,
                      int *set,
                      const char *desc,
                      int size, ...)
{
    opt_opt_t *opt = optsAdd(INT_SWITCH, long_name, short_name, set, desc);
    opt->switch_size = size;
    opt->switch_tag = PDDL_ALLOC_ARR(char *, opt->switch_size);
    opt->switch_ival = PDDL_ALLOC_ARR(int, opt->switch_size);

    va_list arg;
    va_start(arg, size);
    for (int i = 0; i < size; ++i){
        const char *tag = va_arg(arg, const char *);
        int val = va_arg(arg, int);
        opt->switch_tag[i] = PDDL_STRDUP(tag);
        opt->switch_ival[i] = val;
    }
    va_end(arg);
}

static void optsParamsPrintHelp(const opt_opt_t *opt, FILE *fout)
{
    if (opt->desc == NULL)
        return;

    if (opt->long_name != NULL){
        if (opt->short_name != '\x0'){
            fprintf(fout, "Option --%s/-%c:\n", opt->long_name, opt->short_name);
        }else{
            fprintf(fout, "Option --%s:\n", opt->long_name);
        }
    }else{
        fprintf(fout, "Option -%c:\n", opt->short_name);
    }
    fprintf(fout, "%s\n", opt->desc);
}

static opt_opt_t *findOptLong(const char *name)
{
    for (int i = 0; i < o.opt_size; i++){
        if (o.opt[i].long_name && strcmp(o.opt[i].long_name, name) == 0)
            return o.opt + i;
    }
    return NULL;
}

static void optSetFlag(opt_opt_t *opt)
{
    if (opt->set){
        *(int *)opt->set = 1;
    }
}

static void optSetNoFlag(opt_opt_t *opt)
{
    if (opt->set){
        *(int *)opt->set = 0;
    }
}

static int optSet(opt_opt_t *opt, const char *oname, const char *val)
{
    if (opt->type == INT){
        char *end;
        long v = strtol(val, &end, 10);
        if (*end != 0x0){
            fprintf(stderr, "Error: Invalid value for the option %s\n", oname);
            return -1;
        }
        if (v >= INT_MAX || v <= INT_MIN){
            fprintf(stderr, "Error: The value for the option %s is not an integer\n", oname);
            return -1;
        }
        *(int *)opt->set = v;

    }else if (opt->type == FLT || opt->type == FLT_FN){
        char *end;
        float v = strtof(val, &end);
        if (*end != 0x0){
            fprintf(stderr, "Error: Invalid value for the option %s\n", oname);
            return -1;
        }
        if (opt->type == FLT){
            *(float *)opt->set = v;
        }else if (opt->type == FLT_FN){
            if (opt->flt_fn(v) != 0)
                return -1;
        }

    }else if (opt->type == STR || opt->type == STR_FN){
        if (opt->type == STR_FN){
            if (opt->str_fn(val) != 0)
                return -1;

        }else if (opt->set){
            *(char **)opt->set = (char *)val;
        }

    }else if (opt->type == STR_TAGS){
        if (optsProcessTags(val, opt->parse_tags) != 0)
            return -1;

    }else if (opt->type == PARAMS){
        if (strcmp(val, "help") == 0 || strcmp(val, "?") == 0){
            optsParamsPrintHelp(opt, stderr);
            return -1;
        }
        if (optsParamsParse(&opt->params, val) != 0)
            return -1;

    }else if (opt->type == PARAMS_AND_FN){
        if (strcmp(val, "help") == 0 || strcmp(val, "?") == 0){
            optsParamsPrintHelp(opt, stderr);
            return -1;
        }
        if (optsParamsParse(&opt->params, val) != 0)
            return -1;
        opt->params_fn(opt->params_userdata);

    }else if (opt->type == INT_SWITCH){
        int found = 0;
        for (int i = 0; i < opt->switch_size; ++i){
            if (strcmp(opt->switch_tag[i], val) == 0){
                *(int *)opt->set = opt->switch_ival[i];
                found = 1;
            }
        }
        if (!found){
            fprintf(stderr, "Error: Unkown value '%s' to the option %s\n",
                    val, oname);
            return -1;
        }

    }else{
        fprintf(stderr, "Unkown type %d!\n", opt->type);
        exit(-1);
    }

    return 0;
}

static opt_opt_t *findOptShort(char name)
{
    for (int i = 0; i < o.opt_size; i++){
        if (name == o.opt[i].short_name)
            return o.opt + i;
    }
    return NULL;
}

static opt_opt_t *findOpt(char *_arg)
{
    char *arg = _arg;
    opt_opt_t *opt;

    if (arg[0] == '-'){
        if (arg[1] == '-'){
            return findOptLong(arg + 2);
        }else{
            if (arg[1] == 0x0)
                return NULL;

            for (++arg; *arg != 0x0; ++arg){
                opt = findOptShort(*arg);
                if (opt == NULL){
                    return NULL;
                }else if (arg[1] == 0x0){
                    return opt;
                }else if (opt->type == FLAG){
                    optSetFlag(opt);
                }else if (opt->type == FLAG_FN){
                    if (opt->flag_fn(1))
                        return NULL;
                }else if (opt->type == FLAG_FN2){
                    opt->flag_fn2();
                }else{
                    fprintf(stderr, "Error: Unknown option %s.\n", _arg);
                    return NULL;
                }
            }
            
        }
    }

    return NULL;
}


int opts(int *argc, char **argv)
{
    if (*argc <= 1)
        return 0;

    int args_remaining = 1;
    for (int i = 1; i < *argc; i++){
        opt_opt_t *opt = findOpt(argv[i]);

        if (opt){
            if (opt->type == FLAG){
                optSetFlag(opt);
            }else if (opt->type == FLAG_FN){
                if (opt->flag_fn(1) != 0)
                    return -1;
            }else if (opt->type == FLAG_FN2){
                opt->flag_fn2();
            }else{
                if (i + 1 < *argc){
                    ++i;
                    if (optSet(opt, argv[i - 1], argv[i]) != 0)
                        return -1;
                }else{
                    fprintf(stderr, "Error: Missing value for the option %s\n", argv[i]);
                    return -1;
                }
            }
        }else{
            int found = 0;
            if (strncmp(argv[i], "--no-", 5) == 0){
                char arg[strlen(argv[i]) + 1];
                sprintf(arg, "--%s", argv[i] + 5);
                opt = findOpt(arg);
                if (opt != NULL && opt->type == FLAG){
                    optSetNoFlag(opt);
                    found = 1;
                }else if (opt != NULL && opt->type == FLAG_FN){
                    if (opt->flag_fn(0) != 0)
                        return -1;
                    found = 1;
                }
            }

            if (!found){
                if (strncmp(argv[i], "-", 1) == 0){
                    fprintf(stderr, "Error: Invalid option %s\n", argv[i]);
                    return -1;
                }
                argv[args_remaining++] = argv[i];
            }
        }
    }

    *argc = args_remaining;
    return 0;
}

static int maxLen(int group)
{
    int maxlen = 0;
    for (int i = 0; i < o.opt_size; ++i){
        const opt_opt_t *opt = o.opt + i;
        if (opt->group != group)
            continue;

        int len = 0;
        if (opt->long_name != NULL){
            len = 2 + strlen(opt->long_name);
            if (opt->type == FLAG
                    || opt->type == FLAG_FN
                    || opt->type == FLAG_FN2){
                len += 5;
            }
            if (opt->short_name != 0x0){
                len += 3;
            }
        }else{
            len = 2;
        }
        maxlen = PDDL_MAX(maxlen, len);
    }
    return maxlen;
}

static void optsPrintDefault(const opt_opt_t *opt, FILE *fout)
{
    if (opt->type == FLAG_FN
            || opt->type == FLAG_FN2
            || opt->type == PARAMS
            || opt->type == PARAMS_AND_FN
            || opt->type == INT_SWITCH){
        return;
    }

    fprintf(fout, " (default: ");
    if (opt->type == FLAG){
        if (opt->idefault){
            fprintf(fout, "enabled");
        }else{
            fprintf(fout, "disabled");
        }

    }else if (opt->type == FLAG_FN){
        fprintf(fout, "disabled");

    }else if (opt->type == INT){
        fprintf(fout, "%d", opt->idefault);

    }else if (opt->type == FLT){
        fprintf(fout, "%.4f", opt->fdefault);

    }else if (opt->type == STR || opt->type == STR_TAGS){
        if (opt->sdefault == NULL){
            fprintf(fout, "nil");
        }else{
            fprintf(fout, "%s", opt->sdefault);
        }
    }
    fprintf(fout, ")");
}

static void optsPrintOpts(int group, FILE *fout)
{
    int width = maxLen(group);
    for (int i = 0; i < o.opt_size; ++i){
        const opt_opt_t *opt = o.opt + i;
        if (opt->group != group)
            continue;

        fprintf(fout, "    ");
        int prefixlen = 4;

        int len = 0;
        if (opt->long_name != NULL){
            if (opt->type == FLAG || opt->type == FLAG_FN){
                fprintf(fout, "--(no-)%s", opt->long_name);
                len += 7 + strlen(opt->long_name);
            }else{
                fprintf(fout, "--%s", opt->long_name);
                len += 2 + strlen(opt->long_name);
            }
        }

        if (opt->short_name != 0x0){
            if (opt->long_name != NULL){
                fprintf(fout, "/");
                len += 1;
            }
            fprintf(fout, "-%c", opt->short_name);
            len += 2;
        }
        for (int i = len; i < width; ++i)
            fprintf(fout, " ");
        prefixlen += width;

        fprintf(fout, "  ");
        prefixlen += 2;

        if (opt->type == FLAG){
            fprintf(fout, "    ");
        }else if (opt->type == FLAG_FN){
            fprintf(fout, "    ");
        }else if (opt->type == FLAG_FN2){
            fprintf(fout, "    ");
        }else if (opt->type == INT){
            fprintf(fout, "int ");
        }else if (opt->type == FLT || opt->type == FLT_FN){
            fprintf(fout, "flt ");
        }else if (opt->type == STR
                    || opt->type == STR_FN
                    || opt->type == PARAMS
                    || opt->type == PARAMS_AND_FN
                    || opt->type == INT_SWITCH){
            fprintf(fout, "str ");
        }else if (opt->type == STR_TAGS){
            fprintf(fout, "tags");
        }
        prefixlen += 4;

        if (opt->desc != NULL){
            fprintf(fout, "  ");
            prefixlen += 2;

            const char *c = opt->desc;
            while (*c != 0x0){
                fprintf(fout, "%c", *c);
                if (*c == '\n'){
                    for (int j = 0; j < prefixlen; ++j)
                        fprintf(fout, " ");
                }
                ++c;
            }
        }
        optsPrintDefault(opt, fout);
        fprintf(fout, "\n");
    }
}

void optsPrint(FILE *fout)
{
    optsPrintOpts(-1, fout);
    for (int gi = 0; gi < o.group_size; ++gi){
        fprintf(fout, "\n%s\n", o.group[gi].header);
        optsPrintOpts(gi, fout);
    }
    fprintf(fout, "\n");
}

int optsProcessTags(const char *_s, int (*fn)(const char *t))
{
    if (_s == NULL)
        return 0;
    if (*_s == 0x0)
        return 0;

    char *s = PDDL_STRDUP(_s);
    char *next = s;
    char *cur;
    while ((cur = strsep(&next, ":")) != NULL){
        if (fn(cur) != 0){
            PDDL_FREE(s);
            return -1;
        }
    }
    PDDL_FREE(s);

    return 0;
}

void optsParamsInit(opts_params_t *params)
{
    bzero(params, sizeof(*params));
}

void optsParamsFree(opts_params_t *params)
{
    for (int i = 0; i < params->param_size; ++i){
        PDDL_FREE(params->param[i].name);
        for (int j = 0; j < params->param[i].switch_size; ++j)
            PDDL_FREE(params->param[i].switch_tag[j]);
        if (params->param[i].switch_tag != NULL)
            PDDL_FREE(params->param[i].switch_tag);
        if (params->param[i].switch_ival != NULL)
            PDDL_FREE(params->param[i].switch_ival);
    }
    if (params->param != NULL)
        PDDL_FREE(params->param);
}

static opts_param_t *paramsAdd(opts_params_t *params,
                               const char *name,
                               void *dst)
{
    if (params->param_size == params->param_alloc){
        if (params->param_alloc == 0)
            params->param_alloc = 1;
        params->param_alloc *= 2;
        params->param = PDDL_REALLOC_ARR(params->param,
                                         opts_param_t,
                                         params->param_alloc);
    }
    opts_param_t *p = params->param + params->param_size++;
    bzero(p, sizeof(*p));
    p->name = PDDL_STRDUP(name);
    p->dst = dst;
    return p;
}

void optsParamsAddInt(opts_params_t *params, const char *name, int *dst)
{
    optsParamsAddIntFn(params, name, dst, NULL);
}

void optsParamsAddFlt(opts_params_t *params, const char *name, float *dst)
{
    optsParamsAddFltFn(params, name, dst, NULL);
}

void optsParamsAddFlag(opts_params_t *params, const char *name, int *dst)
{
    optsParamsAddFlagFn(params, name, dst, NULL);
}

void optsParamsAddStr(opts_params_t *params, const char *name, char **dst)
{
    opts_param_t *p = paramsAdd(params, name, dst);
    p->is_str = 1;
}

void optsParamsAddIntFn(opts_params_t *params, const char *name, void *dst,
                        opts_params_int_fn fn)
{
    opts_param_t *p = paramsAdd(params, name, dst);
    p->is_int = 1;
    p->int_fn = fn;
}

void optsParamsAddFltFn(opts_params_t *params, const char *name, void *dst,
                        opts_params_flt_fn fn)
{
    opts_param_t *p = paramsAdd(params, name, dst);
    p->is_flt = 1;
    p->flt_fn = fn;
}

void optsParamsAddFlagFn(opts_params_t *params, const char *name, void *dst,
                         opts_params_flag_fn fn)
{
    opts_param_t *p = paramsAdd(params, name, dst);
    p->is_flag = 1;
    p->flag_fn = fn;
}

void optsParamsAddIntSwitch(opts_params_t *params,
                            const char *name,
                            void *dst,
                            int size, ...)
{
    opts_param_t *p = paramsAdd(params, name, dst);
    p->is_int_switch = 1;
    p->switch_size = size;
    p->switch_tag = PDDL_ALLOC_ARR(char *, p->switch_size);
    p->switch_ival = PDDL_ALLOC_ARR(int, p->switch_size);

    va_list arg;
    va_start(arg, size);
    for (int i = 0; i < size; ++i){
        const char *tag = va_arg(arg, const char *);
        int val = va_arg(arg, int);
        p->switch_tag[i] = PDDL_STRDUP(tag);
        p->switch_ival[i] = val;
    }
    va_end(arg);
}

static char *trimWhitespace(char *s)
{
    while (s != NULL
            && *s != 0x0
            && (*s == ' ' || *s == '\t' || *s == '\n')){
        *s = 0x0;
        ++s;
    }
    char *t = s + strlen(s) - 1;
    while (*t != 0x0 && (*t == ' ' || *t == '\t' || *t == '\n')){
        *t = 0x0;
        --t;
    }
    return s;
}

static int setParam(opts_params_t *params,
                    const char *name,
                    const char *value)
{
    for (int i = 0; i < params->param_size; ++i){
        const opts_param_t *p = params->param + i;
        if (strcmp(p->name, name) == 0){
            if (p->is_int){
                int val = atoi(value);
                if (p->int_fn != NULL){
                    p->int_fn(val, p->dst);
                }else{
                    *((int *)p->dst) = val;
                }

            }else if (p->is_flt){
                float val = atof(value);
                if (p->flt_fn != NULL){
                    p->flt_fn(val, p->dst);
                }else{
                    *((float *)p->dst) = val;
                }

            }else if (p->is_flag){
                if (strcmp(value, "1") == 0
                        || strcmp(value, "true") == 0
                        || strcmp(value, "True") == 0){
                    if (p->flag_fn != NULL){
                        p->flag_fn(1, p->dst);
                    }else{
                        *((int *)p->dst) = 1;
                    }

                }else if (strcmp(value, "0") == 0
                            || strcmp(value, "false") == 0
                            || strcmp(value, "False") == 0){
                    if (p->flag_fn != NULL){
                        p->flag_fn(0, p->dst);
                    }else{
                        *((int *)p->dst) = 0;
                    }

                }else{
                    fprintf(stderr, "Error: Invalid argument to the"
                            " parameter '%s'\n", name);
                    return -1;
                }

            }else if (p->is_int_switch){
                int found = 0;
                for (int i = 0; i < p->switch_size; ++i){
                    if (strcmp(p->switch_tag[i], value) == 0){
                        *(int *)p->dst = p->switch_ival[i];
                        found = 1;
                    }
                }
                if (!found){
                    fprintf(stderr, "Error: Unkown value '%s' to the option %s\n",
                            value, name);
                    return -1;
                }

            }else if (p->is_str){
                *(char **)p->dst = PDDL_STRDUP(value);
            }
            return 0;
        }
    }
    return -1;
}

static int setParamFlag(opts_params_t *params, const char *name)
{
    for (int i = 0; i < params->param_size; ++i){
        const opts_param_t *p = params->param + i;
        if (strcmp(p->name, name) == 0 && p->is_flag){
            if (p->flag_fn != NULL){
                p->flag_fn(1, p->dst);
            }else{
                *((int *)p->dst) = 1;
            }
            return 0;
        }
    }
    return -1;
}

static int parseParam(opts_params_t *params, char *text)
{
    text = trimWhitespace(text);
    char *name = text;
    char *value = text;
    strsep(&value, "=");
    if (value != NULL && *value != 0x0){
        name = trimWhitespace(name);
        value = trimWhitespace(value);
        if (setParam(params, name, value) != 0){
            fprintf(stderr, "Error: Uknown parameter: '%s'\n", text);
            return -1;
        }

    }else{
        if (setParamFlag(params, name) != 0){
            fprintf(stderr, "Error: Uknown parameter: '%s'\n", text);
            return -1;
        }
    }
    return 0;
}

int optsParamsParse(opts_params_t *params, const char *text)
{
    char *s = PDDL_STRDUP(text);
    char *next = s;
    char *cur;
    while ((cur = strsep(&next, ",")) != NULL){
        if (parseParam(params, cur) != 0){
            PDDL_FREE(s);
            return -1;
        }
    }
    PDDL_FREE(s);

    return 0;
}
