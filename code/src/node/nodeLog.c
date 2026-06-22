#include "nodeLog.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
<<<<<<< HEAD
#include <unistd.h>
=======
#include <unistd.h>   // getpid()
>>>>>>> 2683b2ba9f8ade9be39f6612cc76490a87443c71

void log_msg(NodeContext *ctx, const char *fmt, ...) {
    if (ctx == NULL || ctx->log_file == NULL) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm *tm_info = localtime(&ts.tv_sec);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(ctx->log_file, "[%s.%03ld] [NODE-%d|PID-%d] ",
            time_buf,
            ts.tv_nsec / 1000000,
            ctx->node_id,
            (int)getpid());

    va_list args;
    va_start(args, fmt);
    vfprintf(ctx->log_file, fmt, args);
    va_end(args);

    fprintf(ctx->log_file, "\n");
    fflush(ctx->log_file);
}