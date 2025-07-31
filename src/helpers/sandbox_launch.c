// sandbox_launch.c
// Launch a program in a sandbox (basic placeholder)

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        return 1;
    }

    // Clone new namespaces (optional, real isolation TBD)
    if (unshare(CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNET) < 0) {
        perror("unshare");
        // Still continue for now
    }

    // TODO: enter cgroup, apply seccomp, set uid/gid

    // Replace process with target
    execvp(argv[1], &argv[1]);
    perror("execvp failed");
    return 1;
}
