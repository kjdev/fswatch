#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#include <inotifytools/inotifytools.h>
#include <inotifytools/inotify.h>

static int interrupted = 0;

static void
signal_handler(int sig)
{
    interrupted = 1;
}

static void
signals(__sighandler_t handler)
{
    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void
usage(char *arg)
{
    char *command = basename(arg);

    printf("Usage: %s <DIRECTORY> <COMMAND> [ARGS ...]\n\n", command);

    printf("  DIRECTORY watch direcotry\n");
    printf("  COMMAND   execute events in commnad\n");
    printf("  ARGS ...  command arguments\n");
}

int
main(int argc, char **argv)
{
    char *sh_command[4] = {
        "/bin/bash",
        "-c",
        NULL,
        NULL
    };
    int events = IN_CREATE | IN_MODIFY | IN_DELETE| IN_MOVE;
    int self_events = IN_DELETE_SELF | IN_MOVE_SELF;
    int i, timeout = -1, n = 0, command_len = 0;
    char *command, *dirs, *tmp;
    struct stat st;
    struct inotify_event *event;
    extern char **environ;

    /* command */
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    for (i = 2; i < argc; i++) {
        command_len += strlen(argv[i]) + 1;
    }

    command = malloc(command_len * sizeof(char));
    if (!command) {
        fprintf(stderr, "ERR: Allocate memory.\n");
        return 1;
    }
    for (i = 2; i < argc; i++) {
        memcpy(&command[n], argv[i], strlen(argv[i]) * sizeof(char));
        n += strlen(argv[i]);
        command[n++] = ' ';
    }
    command[n-1] = '\0';

    sh_command[2] = command;

    /* inotifytools: initialize */
    if (!inotifytools_initialize()) {
        fprintf(stderr, "ERR: Couldn't initialize inotify.\n");
        free(command);
        return 1;
    }

    /* directory */
    dirs = strdup(argv[1]);
    tmp = strtok(dirs, ":");
    if (tmp != NULL) {
        do {
            if (stat(tmp, &st) != 0 || (st.st_mode & S_IFMT) != S_IFDIR) {
                inotifytools_cleanup();
                usage(argv[0]);
                free(command);
                free(dirs);
                return 1;
            }

            if (!inotifytools_watch_recursively(tmp, events | self_events)) {
                fprintf(stderr, "ERR: %s\n", strerror(inotifytools_error()));
                inotifytools_cleanup();
                free(command);
                free(dirs);
                return 1;
            }
            tmp = strtok(NULL, ":");
        } while (tmp != NULL);
    } else {
        if (stat(argv[1], &st) != 0 || (st.st_mode & S_IFMT) != S_IFDIR) {
            inotifytools_cleanup();
            usage(argv[0]);
            free(command);
            free(dirs);
            return 1;
        }

        if (!inotifytools_watch_recursively(argv[1], events | self_events)) {
            fprintf(stderr, "ERR: %s\n", strerror(inotifytools_error()));
            inotifytools_cleanup();
            free(command);
            free(dirs);
            return 1;
        }
    }

    /* inotifytools: event */
    signals(signal_handler);
    do {
        event = inotifytools_next_event(timeout);
        if (!event) {
            if (inotifytools_error() && !interrupted) {
                fprintf(stderr, "ERR: %s\n", strerror(inotifytools_error()));
            }
            break;
        }

        if (event->mask & events) {
            pid_t pid;
            int status;
            if ((pid = fork()) < 0) {
                fprintf(stderr, "ERR: Couldn't fork\n");
                exit(1);
            } else if (pid == 0) {
                if (execve(sh_command[0], sh_command, environ) < 0) {
                    fprintf(stderr, "ERR: %s executing\n", sh_command[0]);
                    exit(1);
                }
            } else {
                while (wait(&status) != pid);
            }
        } else if (event->mask & self_events) {
            fprintf(stderr, "ERR: Events of Self deleted or moved\n");
            break;
        }
    } while (!interrupted);

    inotifytools_cleanup();
    free(command);
    free(dirs);

    return 0;
}
