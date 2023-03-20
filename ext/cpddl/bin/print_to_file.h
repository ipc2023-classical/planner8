#ifndef _PRINT_TO_FILE_H_
#define _PRINT_TO_FILE_H_

_pddl_inline FILE *openFileMode(const char *fn, const char *mode)
{
    if (strcmp(fn, "-") == 0
            || strcmp(fn, "stdout") == 0)
        return stdout;
    if (strcmp(fn, "stderr") == 0)
        return stderr;
    FILE *fout = fopen(fn, mode);
    return fout;
}

_pddl_inline FILE *openFile(const char *fn)
{
    return openFileMode(fn, "w");
}

_pddl_inline FILE *openFileAppend(const char *fn)
{
    return openFileMode(fn, "a");
}

_pddl_inline void closeFile(FILE *f)
{
    if (f != NULL && f != stdout && f != stderr)
        fclose(f);
}

#define PRINT_TO_FILE(ERR, OUT, S, CMD) \
    do { \
    if ((OUT) != NULL){ \
        FILE *fout = openFile((OUT)); \
        if (fout != NULL){ \
            PDDL_INFO((ERR), "Printing %s to %s ...", (S), (OUT)); \
            CMD; \
            closeFile(fout); \
        }else{ \
            PDDL_ERR_RET((ERR), -1, "Could not open '%s'", (OUT)); \
        } \
    } \
    } while (0) 

#define APPEND_TO_FILE(ERR, OUT, S, CMD) \
    do { \
    if ((OUT) != NULL){ \
        FILE *fout = openFileAppend((OUT)); \
        if (fout != NULL){ \
            PDDL_INFO((ERR), "Printing %s to %s ...", (S), (OUT)); \
            CMD; \
            closeFile(fout); \
        }else{ \
            PDDL_ERR_RET((ERR), -1, "Could not open '%s'", (OUT)); \
        } \
    } \
    } while (0) 

#endif /* _PRINT_TO_FILE_H_ */
