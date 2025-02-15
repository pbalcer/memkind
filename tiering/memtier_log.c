// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (C) 2021 Intel Corporation. */

#include "memtier_log.h"
#include "../config.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>

static const char *message_prefixes[MESSAGE_TYPE_MAX_VALUE] = {
    [MESSAGE_TYPE_ERROR] = "MEMKIND_MEM_TIERING_LOG_ERROR",
    [MESSAGE_TYPE_INFO] = "MEMKIND_MEM_TIERING_LOG_INFO",
    [MESSAGE_TYPE_DEBUG] = "MEMKIND_MEM_TIERING_LOG_DEBUG",
};

char *utils_get_env(const char *name)
{
#ifdef MEMKIND_HAVE_SECURE_GETENV
    return secure_getenv(name);
#else
    return getenv(name);
#endif
}

static ssize_t swrite(int fd, const void *buf, size_t count)
{
    return syscall(SYS_write, fd, buf, count);
}

static unsigned log_level;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static void log_generic(message_type_t type, const char *format, va_list args)
{
    static char buf[4096], *b;

    b = buf + sprintf(buf, "%s: ", message_prefixes[type]);
    int blen = sizeof(buf) + (buf - b) - 1;
    int len = vsnprintf(b, blen, format, args);
    sprintf(b + len, "\n");
    b += len + 1;

    if (pthread_mutex_lock(&log_lock) != 0) {
        assert(0 && "failed to acquire log mutex");
    }

    const char overflow_msg[] = "Warning: message truncated.\n";
    if (len >= blen)
        swrite(STDERR_FILENO, overflow_msg, sizeof(overflow_msg));
    swrite(STDERR_FILENO, buf, b - buf);

    if (pthread_mutex_unlock(&log_lock) != 0) {
        assert(0 && "failed to release log mutex");
    }
}

void log_info(const char *format, ...)
{
    if (log_level >= MESSAGE_TYPE_INFO) {
        va_list args;
        va_start(args, format);
        log_generic(MESSAGE_TYPE_INFO, format, args);
        va_end(args);
    }
}

void log_err(const char *format, ...)
{
    // logging errors can't be disabled
    va_list args;
    va_start(args, format);
    log_generic(MESSAGE_TYPE_ERROR, format, args);
    va_end(args);
}

void log_debug(const char *format, ...)
{
    if (log_level >= MESSAGE_TYPE_DEBUG) {
        va_list args;
        va_start(args, format);
        log_generic(MESSAGE_TYPE_DEBUG, format, args);
        va_end(args);
    }
}

void log_init_once(void)
{
    log_level = MESSAGE_TYPE_ERROR;

    char *log_level_env = utils_get_env("MEMKIND_MEM_TIERING_LOG_LEVEL");
    if (log_level_env) {
        char *end;
        errno = 0;
        log_level = strtoul(log_level_env, &end, 10);
        if ((log_level >= MESSAGE_TYPE_MAX_VALUE) || (*end != '\0') ||
            (errno != 0)) {
            log_err("Wrong value of MEMKIND_MEM_TIERING_LOG_LEVEL=%s",
                    log_level_env);
            abort();
        } else {
            log_debug("Setting log level to: %u", log_level);
        }
    }
}
