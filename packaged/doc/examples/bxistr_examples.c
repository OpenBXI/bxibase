#include <unistd.h>
#include <sysexits.h>
#include <string.h>

#include <bxi/base/err.h>
#include <bxi/base/str.h>
#include <bxi/base/log.h>


SET_LOGGER(LOGGER, "bxi.str.examples");

int main(int argc, char** argv) {

//! [Thread-safe basename() equivalent function]
    const char * prog = strdup(argv[0]);
    const char * progname = bxistr_rsub(prog, strlen(prog), '/');
    char * filename = bxistr_new("/tmp/%s.bxilog", progname);
//! [Thread-safe basename() equivalent function]

    // Produce logs on stdout/stderr, and also in filename (append=false)
    bxilog_config_p config = bxilog_basic_config(argv[0], filename, false);
    bxierr_p err = bxilog_init(config);
    BXIFREE(filename);

    // If the logging library raises an error, nothing can be logged!
    // Use the bxierr_report() convenience method in this case
    bxierr_report(&err, STDERR_FILENO);
    if (argc != 1) exit(EX_SOFTWARE);

//! [Multi-line prefixer]
    // Create a multi-line string: strdup() is mandatory because the line must be
    // modified
    char * multiline_str = strdup("This is the first line of a multi-line string\n"
                                  "This is the second line\n"
                                  "And this is the third\n");

    bxistr_prefixer_s prefixer;
    bxistr_prefixer_init(&prefixer, "*prefix*", ARRAYLEN("*prefix*") - 1);
    bxistr_apply_lines(multiline_str, bxistr_prefixer_line, &prefixer);
    for(size_t i = 0; i < prefixer.lines_nb; i++) {
        OUT(LOGGER, "%s", prefixer.lines[i]);
    }
    bxistr_prefixer_cleanup(&prefixer);
    BXIFREE(multiline_str);
//! [Multi-line prefixer]

    err = bxilog_finalize(true);
    bxierr_report(&err, STDERR_FILENO);
    BXIFREE(prog);

    return 0;
}
