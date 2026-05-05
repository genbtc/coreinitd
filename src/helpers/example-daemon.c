/*
 * example-daemon.c
 * A minimal example daemon for testing coreinitd with example unit services.
 *
 * This daemon demonstrates:
 * - Command-line argument handling (--verbose, --foreground, --config)
 * - Flag file-based condition checking (/tmp/example-daemon-control)
 * - Logging and status output
 * - Graceful signal handling
 * - Suitability for testing socket activation and timer-based execution
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Control flag file location */
#define CONTROL_FLAG_FILE "/tmp/example-daemon-control"
#define LOG_FILE "/tmp/example-daemon.log"

/* Global state */
static volatile int should_exit = 0;
static int verbose = 0;
static int foreground = 0;

/* Signal handlers */
static void sigterm_handler(int sig) {
    (void)sig;
    should_exit = 1;
}
static void sigusr1_handler(int sig) {
    (void)sig;
    fprintf(stdout, "[%d] SIGUSR1 received - dumping status\n", getpid());
}

/* Log a message with timestamp */
static void log_message(const char *level, const char *msg) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    char full_msg[512];
    snprintf(full_msg, sizeof(full_msg), "[%s] %s: %s\n",
             timestamp, level, msg);
    if (verbose || foreground) {
        fprintf(stdout, "%s", full_msg);
        fflush(stdout);
    }
    /* Also write to log file for non-foreground mode */
    FILE *logf = fopen(LOG_FILE, "a");
    if (logf) {
        fprintf(logf, "%s", full_msg);
        fclose(logf);
    }
}

/* Check if the control flag file exists and what it contains */
static int check_flag_file(char *content, size_t max_len) {
    FILE *f = fopen(CONTROL_FLAG_FILE, "r");
    if (!f) {
        return 0; /* File doesn't exist */
    }
    if (fgets(content, max_len, f)) {
        /* Remove trailing newline */
        size_t len = strlen(content);
        if (len > 0 && content[len - 1] == '\n') {
            content[len - 1] = '\0';
        }
        fclose(f);
        return 1; /* File exists and we read content */
    }
    fclose(f);
    return 0;
}

/* Write status to a file (simulation of daemon status) */
static void write_status_file(const char *status) {
    FILE *f = fopen("/tmp/example-daemon-status", "w");
    if (f) {
        fprintf(f, "%s\n", status);
        fclose(f);
    }
}

int main(int argc, char *argv[]) {
    char config_file[256] = "";
    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--foreground") == 0 || 
                   strcmp(argv[i], "-f") == 0) {
            foreground = 1;
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            strncpy(config_file, argv[++i], sizeof(config_file) - 1);
            config_file[sizeof(config_file) - 1] = '\0';
        } else {
            fprintf(stderr, "Usage: %s [--verbose] [--foreground|-f] [--config FILE]\n", 
                    argv[0]);
            return 1;
        }
    }
    /* Setup signal handlers */
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);
    signal(SIGUSR1, sigusr1_handler);
    /* Log startup */
    char startup_msg[256];
    snprintf(startup_msg, sizeof(startup_msg),
             "Example daemon started (PID: %d, verbose: %d, foreground: %d)",
             getpid(), verbose, foreground);
    log_message("INFO", startup_msg);
    if (config_file[0] != '\0') {
        char config_msg[256];
        snprintf(config_msg, sizeof(config_msg), "Using config file: %s", config_file);
        log_message("INFO", config_msg);
    }
    write_status_file("running");
    /* Main loop */
    int iteration = 0;
    while (!should_exit) {
        iteration++;
        /* Check flag file for actions */
        char flag_content[128] = "";
        if (check_flag_file(flag_content, sizeof(flag_content))) {
            char flag_msg[256];
            snprintf(flag_msg, sizeof(flag_msg),
                     "Control flag detected: '%s'", flag_content);
            log_message("INFO", flag_msg);
            /* Perform different actions based on flag content */
            if (strcmp(flag_content, "shutdown") == 0) {
                log_message("INFO", "Shutdown flag detected - exiting gracefully");
                should_exit = 1;
            } else if (strcmp(flag_content, "status-check") == 0) {
                log_message("INFO", "Status check requested");
                write_status_file("operational");
            } else if (strcmp(flag_content, "reset") == 0) {
                iteration = 0;
                log_message("INFO", "Reset requested - iteration counter reset");
            }
        }
        /* Periodic activity log (every 5 iterations) */
        if (iteration % 5 == 0) {
            char activity_msg[256];
            snprintf(activity_msg, sizeof(activity_msg),
                     "Periodic check: iteration %d", iteration);
            log_message("DEBUG", activity_msg);
        }
        /* Sleep for a second */
        sleep(1);
    }
    /* Cleanup and exit */
    log_message("INFO", "Example daemon shutting down");
    write_status_file("stopped");
    /* Remove control flag file if it exists */
    unlink(CONTROL_FLAG_FILE);
    fprintf(stdout, "[%d] Example daemon exiting cleanly\n", getpid());
    return 0;
}
