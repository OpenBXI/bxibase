#include <unistd.h>
#include <sysexits.h>

#include <bxi/base/err.h>
#include <bxi/base/log.h>

// Create a logger for my module/main
SET_LOGGER(MY_LOGGER, "my.logger");
// And other loggers to play with
SET_LOGGER(LOGGER_A, "a.logger");
SET_LOGGER(LOGGER_AB, "a.b.logger");
SET_LOGGER(LOGGER_AC, "a.c.logger");

char ** LEVEL_NAMES = NULL;

void log_stuff(bxilog_p logger) {
    WARNING(logger, "A message");
    OUT(logger, "A message");
    DEBUG(logger, "A message");
}

void display_loggers(size_t n, bxilog_p loggers[n]) {

    for (size_t i = 0; i < n; i++) {
        OUT(MY_LOGGER, "%s: %s",
            loggers[i]->name,
            LEVEL_NAMES[loggers[i]->level]);
    }
}

int main(int argc, char** argv) {
    // Produce logs on stdout/stderr, and also in /tmp/foo.log (append=false)
    bxilog_config_p config = bxilog_basic_config(argv[0], "/tmp/foo.log", false);
    bxierr_p err = bxilog_init(config);
    // If the logging library raises an error, nothing can be logged!
    // Use the bxierr_report() convenience method in this case
    bxierr_report(err, STDERR_FILENO);
    if (argc != 1) exit(EX_SOFTWARE);

    // Fetching log level names
    size_t n = 0;
    n = bxilog_get_all_level_names(&LEVEL_NAMES);
    // Use BXIASSERT() instead of assert(), this guarantee all logs
    // are flushed before exiting.
    BXIASSERT(MY_LOGGER, n > 0 && NULL != LEVEL_NAMES);

    // Fetching all registered loggers
    bxilog_p *loggers = {NULL};
    n = bxilog_registry_getall(&loggers);
    BXIASSERT(MY_LOGGER, n>0 && NULL != loggers);

    OUT(MY_LOGGER, "Before configuration:");
    display_loggers(n, loggers);
    log_stuff(LOGGER_A);
    log_stuff(LOGGER_AB);
    log_stuff(LOGGER_AC);

    bxilog_filter_s filters[] = {{.prefix="", .level=BXILOG_LOWEST},
                                 {.prefix="a", .level=BXILOG_OUTPUT},
                                 {.prefix="a.b", .level=BXILOG_WARNING},
    };
    bxilog_registry_set_filters(3, filters);
    OUT(MY_LOGGER, "After configuration:");
    display_loggers(n, loggers);
    log_stuff(LOGGER_A);
    log_stuff(LOGGER_AB);
    log_stuff(LOGGER_AC);

    err = bxilog_finalize(true);
    // Again, the logging library is not in a normal state,
    // use bxierr_report()
    bxierr_report(err, STDERR_FILENO);
    return 0;
}
