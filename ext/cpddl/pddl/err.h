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

#ifndef __PDDL_ERR_H__
#define __PDDL_ERR_H__

#include <pddl/timer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Maximal length of an error message */
#define PDDL_ERR_MSG_MAXLEN 256
/** Maximal length of an error prefix */
#define PDDL_ERR_MSG_PREFIX_MAXLEN 32
/** Maximal depth of a trace */
#define PDDL_ERR_TRACE_DEPTH 32
/** Maximal length of a prefix */
#define PDDL_ERR_PREFIX_MAXLEN 32
/** Number of prefixes */
#define PDDL_ERR_PREFIX_NUM 8
/** Maximum number of contexts */
#define PDDL_ERR_CTX_MAXLEN 32
/** Maximum length of an INFO prefix */
#define PDDL_ERR_CTX_INFO_MAXLEN 32
/** Maximum length of a keyword */
#define PDDL_ERR_CTX_KW_MAXLEN 32


struct pddl_err_trace {
    const char *filename;
    int line;
    const char *func;
};
typedef struct pddl_err_trace pddl_err_trace_t;

struct pddl_err_ctx {
    char info[PDDL_ERR_CTX_INFO_MAXLEN];
    char kw[PDDL_ERR_CTX_KW_MAXLEN];
    int use_time;
    pddl_timer_t timer;
};
typedef struct pddl_err_ctx pddl_err_ctx_t;

struct pddl_err {
    pddl_err_trace_t trace[PDDL_ERR_TRACE_DEPTH];
    int trace_depth;
    int trace_more;
    char msg[PDDL_ERR_MSG_MAXLEN];
    int err;

    pddl_err_ctx_t ctx[PDDL_ERR_CTX_MAXLEN];
    int ctx_size;
    pddl_timer_t ctx_timer;
    int ctx_timer_started;

    FILE *warn_out;
    FILE *info_out;
    FILE *prop_out;
    int info_print_resources_disabled;
    pddl_timer_t info_timer;
    int info_timer_init;
};
typedef struct pddl_err pddl_err_t;

#define PDDL_ERR_INIT { 0 }

/**
 * Initialize error structure.
 */
void pddlErrInit(pddl_err_t *err);

/**
 * Start global context timer.
 */
void pddlErrStartCtxTimer(pddl_err_t *err);

/**
 * Returns true if an error message is set.
 */
int pddlErrIsSet(const pddl_err_t *err);

/**
 * Print the stored error message.
 */
void pddlErrPrint(const pddl_err_t *err, int with_traceback, FILE *fout);

/**
 * Enable/disable warnings.
 * Sets the output stream, if fout is NULL the warnings are disabled.
 */
void pddlErrWarnEnable(pddl_err_t *err, FILE *fout);

/**
 * Enable/disable info messages.
 */
void pddlErrInfoEnable(pddl_err_t *err, FILE *fout);

/**
 * Enable/disabled property output.
 */
void pddlErrPropEnable(pddl_err_t *err, FILE *fout);

/**
 * Disable printing resources with PDDL_INFO
 */
void pddlErrInfoDisablePrintResources(pddl_err_t *err, int disable);

/**
 * Flush all buffers.
 */
void pddlErrFlush(pddl_err_t *err);

/**
 * Sets error message and starts tracing the calls.
 */
#define PDDL_ERR(E, ...) \
    _pddlErr((E), __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * Same as PDDL_ERR() but also returns the value V immediatelly.
 */
#define PDDL_ERR_RET(E, V, ...) do { \
        PDDL_ERR((E), __VA_ARGS__); \
        return (V); \
    } while (0)


/**
 * Fatal error that causes exit.
 */
#define PDDL_PANIC(...) _pddlPanic(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define PDDL_PANIC_IF(COND, ...) do { \
        if (!!(COND)) PDDL_PANIC(__VA_ARGS__); \
    } while (0)


/**
 * Prints warning.
 */
#define PDDL_WARN(E, ...) \
    _pddlWarn((E), __FILE__, __LINE__, __func__,  __VA_ARGS__)

/**
 * Prints info line with timestamp.
 */
#define PDDL_INFO(E, ...) \
    _pddlInfo((E), __FILE__, __LINE__, __func__, __VA_ARGS__)


/**
 * Enter another level of context.
 */
#define PDDL_CTX(E, KW, I) _pddlCtx((E), (KW), (I), 1)
#define PDDL_CTX_F(E, KW, I, ...) _pddlCtxFmt((E), (KW), (I), 1, __VA_ARGS__)
#define PDDL_CTX_NO_TIME(E, KW, I) _pddlCtx((E), (KW), (I), 0)
#define PDDL_CTX_NO_TIME_F(E, KW, I, ...) _pddlCtxFmt((E), (KW), (I), 0, __VA_ARGS__)

/**
 * Leave current context.
 */
#define PDDL_CTXEND(E) _pddlCtxEnd(E)

/**
 * Prints info line with timestamp.
 */
#define PDDL_LOG(E, ...) _pddlLog((E), __VA_ARGS__)
#define PDDL_LOG_IN_CTX(E, CTX_KW, CTX_I, format, ...) \
    do { \
        PDDL_CTX_NO_TIME((E), (CTX_KW), (CTX_I)); \
        PDDL_LOG((E), format, __VA_ARGS__); \
        PDDL_CTXEND((E)); \
    } while (0)

#define PDDL_PROP_BOOL(E, KEY, V) \
    _pddlProp((E), (KEY), "%s", ((V) ? "true" : "false"))
#define PDDL_PROP_INT(E, KEY, V) \
    _pddlProp((E), (KEY), "%d", (V))
#define PDDL_PROP_LONG(E, KEY, V) \
    _pddlProp((E), (KEY), "%ld", (V))
#define PDDL_PROP_DBL(E, KEY, V) \
    _pddlProp((E), (KEY), "%.4f", (V))
#define PDDL_PROP_STR(E, KEY, V) \
    _pddlProp((E), (KEY), "\"%s\"", (V))

/**
 * Trace the error -- record the current file, line and function.
 */
#define PDDL_TRACE(E) \
   _pddlTrace((E), __FILE__, __LINE__, __func__)

/**
 * Same as PDDL_TRACE() but also returns the value V.
 */
#define PDDL_TRACE_RET(E, V) do { \
        PDDL_TRACE(E); \
        return (V); \
    } while (0)

/**
 * Prepends the message before the current error message and trace the
 * call.
 */
#define PDDL_TRACE_PREPEND(E, format, ...) do { \
        _pddlErrPrepend((E), format, __VA_ARGS__); \
        PDDL_TRACE(E); \
    } while (0)
#define PDDL_TRACE_PREPEND_RET(E, V, format, ...) do { \
        _pddlErrPrepend((E), format, __VA_ARGS__); \
        PDDL_TRACE_RET((E), V); \
    } while (0)


void _pddlErr(pddl_err_t *err, const char *filename, int line, const char *func,
              const char *format, ...);
void _pddlPanic(const char *filename, int line, const char *func,
                const char *format, ...);
void _pddlErrPrepend(pddl_err_t *err, const char *format, ...);
void _pddlTrace(pddl_err_t *err, const char *fn, int line, const char *func);
void _pddlCtx(pddl_err_t *err, const char *kw, const char *info, int time);
void _pddlCtxFmt(pddl_err_t *err, const char *kw, const char *info, int time, ...);
void _pddlCtxEnd(pddl_err_t *err);
void _pddlWarn(pddl_err_t *err, const char *filename, int line, const char *func,
               const char *format, ...);
void _pddlInfo(pddl_err_t *err, const char *filename, int line, const char *func,
               const char *format, ...);
void _pddlLog(pddl_err_t *err, const char *fmt, ...);
void _pddlProp(pddl_err_t *err, const char *key, const char *fmt, ...);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_ERR_H__ */
