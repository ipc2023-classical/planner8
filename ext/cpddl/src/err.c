/***
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
 * Faculty of Electrical Engineering, Czech Technical University in Prague.
 * All rights reserved.
 *
 * This file is part of cpddl.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file BDS-LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#include "internal.h"
#include "pddl/err.h"

static void _pddlProp_kw(pddl_err_t *err, const char *kw)
{
    for (int i = 0; i < err->ctx_size; ++i)
        fprintf(err->prop_out, "%s.", err->ctx[i].kw);
    fprintf(err->prop_out, "%s = ", kw);
}

static void _pddlProp_i(pddl_err_t *err, const char *kw, const char *v, int vlen)
{
    if (err == NULL || err->prop_out == NULL)
        return;
    _pddlProp_kw(err, kw);
    fwrite(v, sizeof(char), vlen, err->prop_out);
    fwrite("\n", sizeof(char), 1, err->prop_out);
    fflush(err->prop_out);
}

static void _pddlProp_s(pddl_err_t *err, const char *kw, const char *v, int vlen)
{
    if (err == NULL || err->prop_out == NULL)
        return;
    _pddlProp_kw(err, kw);
    fwrite("\"", sizeof(char), 1, err->prop_out);
    fwrite(v, sizeof(char), vlen, err->prop_out);
    fwrite("\"\n", sizeof(char), 2, err->prop_out);
    fflush(err->prop_out);
}

static void pddlErrPrintMsg(const pddl_err_t *err, FILE *fout)
{
    if (!err->err)
        return;

    fprintf(fout, "Error: %s\n", err->msg);
    fflush(fout);
}

static void pddlErrPrintTraceback(const pddl_err_t *err, FILE *fout)
{
    if (!err->err)
        return;

    for (int i = 0; i < err->trace_depth; ++i){
        for (int j = 0; j < i; ++j)
            fprintf(fout, "  ");
        fprintf(fout, "  ");
        fprintf(fout, "%s:%d (%s)\n",
                err->trace[i].filename,
                err->trace[i].line,
                err->trace[i].func);
    }
    fflush(fout);
}

void pddlErrInit(pddl_err_t *err)
{
    ZEROIZE(err);
}

void pddlErrStartCtxTimer(pddl_err_t *err)
{
    pddlTimerStart(&err->ctx_timer);
    err->ctx_timer_started = 1;
}

int pddlErrIsSet(const pddl_err_t *err)
{
    return err->err;
}

void pddlErrPrint(const pddl_err_t *err, int with_traceback, FILE *fout)
{
    pddlErrPrintMsg(err, fout);
    if (with_traceback)
        pddlErrPrintTraceback(err, fout);
}

void pddlErrWarnEnable(pddl_err_t *err, FILE *fout)
{
    err->warn_out = fout;
}

void pddlErrInfoEnable(pddl_err_t *err, FILE *fout)
{
    err->info_out = fout;
}

void pddlErrPropEnable(pddl_err_t *err, FILE *fout)
{
    err->prop_out = fout;
}

void pddlErrInfoDisablePrintResources(pddl_err_t *err, int disable)
{
    err->info_print_resources_disabled = disable;
}

void pddlErrFlush(pddl_err_t *err)
{
    if (err == NULL)
        return;
    if (err->warn_out != NULL)
        fflush(err->warn_out);
    if (err->info_out != NULL)
        fflush(err->info_out);
    if (err->prop_out != NULL)
        fflush(err->prop_out);
}


void _pddlErr(pddl_err_t *err, const char *filename, int line, const char *func,
              const char *format, ...)
{
    if (err == NULL)
        return;

    va_list ap;

    err->trace[0].filename = filename;
    err->trace[0].line = line;
    err->trace[0].func = func;
    err->trace_depth = 1;
    err->trace_more = 0;

    va_start(ap, format);
    vsnprintf(err->msg, PDDL_ERR_MSG_MAXLEN, format, ap);
    va_end(ap);
    err->err = 1;
}

void _pddlPanic(const char *filename, int line, const char *func,
                const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    fprintf(stderr, "FATAL ERROR: %s:%d [%s]: ", filename, line, func);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(-1);
}

void _pddlErrPrepend(pddl_err_t *err, const char *format, ...)
{
    if (err == NULL)
        return;

    va_list ap;
    char msg[PDDL_ERR_MSG_MAXLEN];
    int size;

    strcpy(msg, err->msg);
    va_start(ap, format);
    size = vsnprintf(err->msg, PDDL_ERR_MSG_MAXLEN, format, ap);
    snprintf(err->msg + size, PDDL_ERR_MSG_MAXLEN - size, "%s", msg);
    va_end(ap);

}

void _pddlTrace(pddl_err_t *err, const char *filename, int line, const char *func)
{
    if (err == NULL)
        return;

    if (err->trace_depth == PDDL_ERR_TRACE_DEPTH){
        err->trace_more = 1;
    }else{
        err->trace[err->trace_depth].filename = filename;
        err->trace[err->trace_depth].line = line;
        err->trace[err->trace_depth].func = func;
        ++err->trace_depth;
    }
}

void _pddlCtx(pddl_err_t *err, const char *kw, const char *info, int time)
{
    if (err == NULL || err->ctx_size == PDDL_ERR_CTX_MAXLEN)
        return;

    pddl_err_ctx_t *ctx = err->ctx + err->ctx_size++;
    strncpy(ctx->kw, kw, PDDL_ERR_CTX_KW_MAXLEN - 1);
    ctx->kw[PDDL_ERR_CTX_KW_MAXLEN - 1] = '\0';
    strncpy(ctx->info, info, PDDL_ERR_CTX_INFO_MAXLEN - 1);
    ctx->info[PDDL_ERR_CTX_INFO_MAXLEN - 1] = '\0';
    ctx->use_time = time;
    if (time){
        pddlTimerStart(&ctx->timer);
        _pddlLog(err, "BEGIN");
        if (err->ctx_timer_started){
            pddlTimerStop(&err->ctx_timer);
            PDDL_PROP_DBL(err, "ctx_start_time",
                          pddlTimerElapsedInSF(&err->ctx_timer));
        }
    }
}

void _pddlCtxFmt(pddl_err_t *err, const char *_kw, const char *_info,
                 int time, ...)
{
    char kw[PDDL_ERR_CTX_MAXLEN];
    char info[PDDL_ERR_MSG_MAXLEN];

    va_list ap;
    va_start(ap, time);
    vsnprintf(kw, PDDL_ERR_CTX_MAXLEN - 1, _kw, ap);
    va_end(ap);
    kw[PDDL_ERR_CTX_MAXLEN - 1] = '\x0';

    va_start(ap, time);
    vsnprintf(info, PDDL_ERR_MSG_MAXLEN - 1, _info, ap);
    va_end(ap);
    info[PDDL_ERR_MSG_MAXLEN - 1] = '\x0';

    _pddlCtx(err, kw, info, time);
}

void _pddlCtxEnd(pddl_err_t *err)
{
    if (err != NULL && err->ctx_size > 0){
        pddl_err_ctx_t *ctx = err->ctx + err->ctx_size - 1;
        if (ctx->use_time){
            if (err->ctx_timer_started){
                pddlTimerStop(&err->ctx_timer);
                PDDL_PROP_DBL(err, "ctx_end_time",
                              pddlTimerElapsedInSF(&err->ctx_timer));
            }
            pddlTimerStop(&ctx->timer);
            _pddlLog(err, "END elapsed time: %{ctx_elapsed_time}.3f",
                     pddlTimerElapsedInSF(&ctx->timer));
        }
        --err->ctx_size;
    }
}

void _pddlWarn(pddl_err_t *err, const char *filename, int line, const char *func,
               const char *format, ...)
{
    if (err == NULL)
        return;

    va_list ap;

    if (err->warn_out == NULL)
        return;

    va_start(ap, format);
    fprintf(err->warn_out, "Warning: %s:%d [%s]: ", filename, line, func);
    vfprintf(err->warn_out, format, ap);
    va_end(ap);
    fprintf(err->warn_out, "\n");
    fflush(err->warn_out);
}


static void infoResources(pddl_err_t *err)
{
    if (!err->info_timer_init){
        pddlTimerStart(&err->info_timer);
        err->info_timer_init = 1;
    }

    if (err->info_out == NULL)
        return;

    if (!err->info_print_resources_disabled){
        struct rusage usg;
        long peak_mem = 0L;
        if (getrusage(RUSAGE_SELF, &usg) == 0)
            peak_mem = usg.ru_maxrss / 1024L;
        pddlTimerStop(&err->info_timer);
        fprintf(err->info_out, "[%.3fs %ldMB] ",
                pddlTimerElapsedInSF(&err->info_timer), peak_mem);
    }
}
void _pddlInfo(pddl_err_t *err, const char *filename, int line, const char *func,
               const char *format, ...)
{
    if (err == NULL || err->info_out == NULL)
        return;

    infoResources(err);

    va_list ap;
    va_start(ap, format);
    for (int pi = 0; pi < err->ctx_size; ++pi)
        fprintf(err->info_out, "%s: ", err->ctx[pi].info);
    vfprintf(err->info_out, format, ap);
    va_end(ap);
    fprintf(err->info_out, "\n");
    fflush(err->info_out);
}

static void logInfo(pddl_err_t *err, const char *buf, int len)
{
    if (err->info_out != NULL)
        fwrite(buf, sizeof(char), len, err->info_out);
}

static char digit_c[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'a', 'b', 'c', 'd', 'e', 'f' };

static const char *itos(int value, int radix, char *buf, int buflen)
{
    int neg = 0;
    if (value < 0){
        neg = 1;
        value = -value;
    }

    do {
        int digit = value % radix;
        buf[--buflen] = digit_c[digit];
        value /= radix;
    } while (value > 0);

    if (neg)
        buf[--buflen] = '-';

    return buf + buflen;
}

static const char *utos(unsigned int value, unsigned int radix, char *buf, int buflen)
{
    do {
        unsigned int digit = value % radix;
        buf[--buflen] = digit_c[digit];
        value /= radix;
    } while (value > 0);
    return buf + buflen;
}

static const char *ltos(long value, long radix, char *buf, int buflen)
{
    int neg = 0;
    if (value < 0L){
        neg = 1;
        value = -value;
    }

    do {
        long digit = value % radix;
        buf[--buflen] = digit_c[digit];
        value /= radix;
    } while (value > 0);

    if (neg)
        buf[--buflen] = '-';

    return buf + buflen;
}

static const char *ultos(unsigned long value, unsigned long radix, char *buf, int buflen)
{
    do {
        unsigned long digit = value % radix;
        buf[--buflen] = digit_c[digit];
        value /= radix;
    } while (value > 0);
    return buf + buflen;
}

#define NUM_BUFSIZE 128
#define MOD_BUFSIZE 128
#define KW_BUFSIZE PDDL_ERR_CTX_KW_MAXLEN
void _pddlLog(pddl_err_t *err, const char *fmt, ...)
{
    if (err == NULL)
        return;

    char bf[NUM_BUFSIZE];
    char mod[MOD_BUFSIZE];
    char keyword[KW_BUFSIZE];
    int ins;
    char ch;

    if (err->info_out != NULL){
        infoResources(err);
        for (int pi = 0; pi < err->ctx_size; ++pi)
            fprintf(err->info_out, "%s: ", err->ctx[pi].info);
    }

    va_list va;
    va_start(va, fmt);
    while (1){
        keyword[0] = '\0';
        const char *fmt_begin = fmt;
        int len = 0;
        for (ch = *(fmt++); ch != '%' && ch != '\0'; ch = *(fmt++), ++len)
            ;
        if (len > 0)
            logInfo(err, fmt_begin, len);
        if (ch == '\0')
            break;

        ch = *(fmt++);
        if (ch == '\0')
            break;

        if (ch == '%'){
            logInfo(err, "%", 1);
            continue;
        }

        if (ch == '{'){
            ch = *(fmt++);
            for (ins = 0; ch != '}' && ch != '\0'; ch = *(fmt++)){
                if (ins < KW_BUFSIZE - 1)
                    keyword[ins++] = ch;
            }
            keyword[ins] = '\0';
            if (ch == '\0')
                break;
            ch = *(fmt++);
        }

        int is_long = 0;
        if (ch == 'l'){
            is_long = 1;
            ch = *(fmt++);
            if (ch == '\0')
                break;
        }

        int bval;
        const char *s;
        switch (ch){
            case 'b':
                if (is_long){
                    bval = va_arg(va, long);
                }else{
                    bval = va_arg(va, int);
                }
                if (bval){
                    logInfo(err, "true", 4);
                    if (keyword[0] != '\0')
                        _pddlProp_i(err, keyword, "true", 4);
                }else{
                    logInfo(err, "false", 5);
                    if (keyword[0] != '\0')
                        _pddlProp_i(err, keyword, "false", 5);
                }
                break;

            case 'u':
                if (is_long){
                    unsigned long v = va_arg(va, unsigned long);
                    s = ultos(v, 10, bf, NUM_BUFSIZE);
                }else{
                    unsigned int v = va_arg(va, unsigned int);
                    s = utos(v, 10, bf, NUM_BUFSIZE);
                }
                logInfo(err, s, NUM_BUFSIZE - (s - bf));
                if (keyword[0] != '\0')
                    _pddlProp_i(err, keyword, s, NUM_BUFSIZE - (s - bf));
                break;

            case 'd':
                if (is_long){
                    long v = va_arg(va, long);
                    s = ltos(v, 10, bf, NUM_BUFSIZE);
                }else{
                    int v = va_arg(va, int);
                    s = itos(v, 10, bf, NUM_BUFSIZE);
                }
                logInfo(err, s, NUM_BUFSIZE - (s - bf));
                if (keyword[0] != '\0')
                    _pddlProp_i(err, keyword, s, NUM_BUFSIZE - (s - bf));
                break;

            case 'x':
                if (is_long){
                    unsigned long v = va_arg(va, unsigned long);
                    s = ultos(v, 16, bf, NUM_BUFSIZE);
                }else{
                    unsigned int v = va_arg(va, unsigned int);
                    s = utos(v, 16, bf, NUM_BUFSIZE);
                }
                logInfo(err, s, NUM_BUFSIZE - (s - bf));
                if (keyword[0] != '\0')
                    _pddlProp_i(err, keyword, s, NUM_BUFSIZE - (s - bf));
                break;

            case 'c':
                ch = (char)(va_arg(va, int));
                logInfo(err, &ch, 1);
                if (keyword[0] != '\0')
                    _pddlProp_s(err, keyword, &ch, 1);
                break;

            case 's':
                s = va_arg(va, char*);
                len = strlen(s);
                logInfo(err, s, len);
                if (keyword[0] != '\0')
                    _pddlProp_s(err, keyword, s, len);
                break;

            default:
                mod[0] = '%';
                for (ins = 1; ch != 'f' && ch != 'g' && ch != '\0';
                        ch = *(fmt++)){
                    mod[ins++] = ch;
                }
                mod[ins++] = ch;
                mod[ins] = '\0';

                if (ch == 'f' || ch == 'g'){
                    double v = va_arg(va, double);
                    int len = snprintf(bf, NUM_BUFSIZE, mod, v);
                    len = PDDL_MIN(len, NUM_BUFSIZE);
                    logInfo(err, bf, len);
                    if (keyword[0] != '\0')
                        _pddlProp_i(err, keyword, bf, len);
                }else{
                    fprintf(stderr, "Fatal error: unkown format flag '%c'\n", ch);
                    exit(-1);
                }
        }
    }

    if (err->info_out != NULL){
        fprintf(err->info_out, "\n");
        fflush(err->info_out);
    }

    va_end(va);
}

#define BUFSIZE 1024
void _pddlProp(pddl_err_t *err, const char *key, const char *fmt, ...)
{
    if (err == NULL || err->prop_out == NULL)
        return;

    char bf[BUFSIZE];
    va_list va;
    va_start(va, fmt);
    int len = vsnprintf(bf, BUFSIZE - 1, fmt, va);
    len = PDDL_MIN(len, BUFSIZE - 1);
    va_end(va);
    bf[BUFSIZE - 1] = '\0';
    _pddlProp_i(err, key, bf, len);
}
