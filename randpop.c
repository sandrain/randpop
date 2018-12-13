#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <getopt.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static int continue_on_fail;

static const char *root = "./randpop";

static uint64_t total_count = 1000;
static uint64_t n_threads = 4;
static uint64_t files_per_dir = 10;
static uint64_t dirs_per_dir = 10;
static uint64_t tree_depth;

static inline int randpop_mkdir(const char *pathname)
{
    int ret = 0;

    ret = mkdir(pathname, 0755);
    if (ret < 0) {
        fprintf(stderr, "mkdir failed on %s: %s (%d)",
                        pathname, strerror(errno), errno);
        if (!continue_on_fail)
            assert(0);
    }

    return ret;
}

static inline int randpop_create(const char *pathname)
{
    int ret = 0;

    ret = creat(pathname, 0644);
    if (ret < 0) {
        fprintf(stderr, "creat failed on %s: %s (%d)",
                        pathname, strerror(errno), errno);
        if (!continue_on_fail)
            assert(0);
    }
    else {
        close(ret);
        ret = 0;
    }

    return ret;
}

static void do_directory(char *name, int depth)
{
    uint64_t i = 0;
    char *pos = &name[strlen(name)];

    for (i = 0; i < files_per_dir; i++) {
        sprintf(pos, "/file.%lu", i);
        randpop_create(name);
    }

    for (i = 0; i < dirs_per_dir; i++) {
        sprintf(pos, "/dir.%lu", i);
        randpop_mkdir(name);

        if (depth)
            do_directory(name, depth - 1);
    }
}

static void *worker_func(void *data)
{
    char path[PATH_MAX] = { 0, };
    uint64_t tid = (uint64_t) pthread_self();

    umask(0);

    sprintf(path, "%s/t.%lu", root, tid);
    randpop_mkdir(path);

    do_directory(path, tree_depth);

    return (void *) NULL;
}

static inline uint64_t calculate_depth(uint64_t goal)
{
    uint64_t d = 0;
    uint64_t sum = 1;
    uint64_t count = 0;

    for (d = 1; count < goal; d++) {
        sum += dirs_per_dir^(d-1);
        count = (files_per_dir+dirs_per_dir)*sum;
    }

    return d;
}

static struct option const long_opts[] = {
    { "count", 1, 0, 'c' },
    { "ndirs", 1, 0, 'd' },
    { "nfiles", 1, 0, 'f' },
    { "help", 0, 0, 'h' },
    { "ignore-error", 0, 0, 'i' },
    { "root", 1, 0, 'r' },
    { "threads", 1, 0, 't' },
    { 0, 0, 0, 0},
};

static const char *short_opts = "cd:f:hir:t:";

int main(int argc, char **argv)
{
    int ret = 0;
    int ch = 0;
    int optidx = 0;
    uint64_t i = 0;
    pthread_t *threads = NULL;

    while ((ch = getopt_long(argc, argv,
                              short_opts, long_opts, &optidx)) >= 0) {
        switch (ch) {
        case 'c':
            total_count = strtoull(optarg, NULL, 0);
            break;

        case 'd':
            dirs_per_dir = strtoull(optarg, NULL, 0);
            break;

        case 'f':
            files_per_dir = strtoull(optarg, NULL, 0);
            break;

        case 'i':
            continue_on_fail = 1;
            break;

        case 'r':
            root = strdup(optarg);
            break;

        case 't':
            n_threads = strtoull(optarg, NULL, 0);
            break;

        case 'h':
        default:
            fprintf(stderr, "check usage\n");
            goto out;
        }
    }

    printf("## total_count = %lu\n"
           "## subdirs = %lu\n"
           "## files = %lu\n"
           "## threads = %lu\n"
           "## test directory = %s\n",
           total_count,
           dirs_per_dir,
           files_per_dir,
           n_threads,
           root);

    tree_depth = calculate_depth(total_count/n_threads + 1);
    if (tree_depth < 1) {
        fprintf(stderr, "Tree depth is 0, try again.\n");
        goto out;
    }

    printf("## tree depth = %lu\n", tree_depth);

    ret = mkdir(root, 0755);
    if (ret < 0 || errno != EEXIST) {
        fprintf(stderr, "failed to create the directory %s (%s)",
                        root, strerror(errno));
        goto out;
    }

    threads = calloc(sizeof(*threads), n_threads);
    if (!threads) {
        perror("cannot allocate memory");
        goto out;
    }

    for (i = 0; i < n_threads; i++) {
        ret = pthread_create(&threads[i], NULL, &worker_func, NULL);
        if (ret < 0) {
            perror("cannot create threads.\n");
            goto out_free;
        }
    }

    for (i = 0; i < n_threads; i++) {
        ret = pthread_join(threads[i], NULL);
        if (ret)
            perror("pthread_join");
    }

out_free:
    free(threads);
out:
    return ret;
}
