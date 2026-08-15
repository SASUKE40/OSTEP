#define _GNU_SOURCE

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sched.h>
#endif

enum {
    DEFAULT_REPEATS = 9,
    WARMUP_ROUNDS = 1000
};

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [-n round_trips] [-r repeats] [-c cpu]\n"
            "  -n  Parent/child ping-pongs per sample (default: 100000)\n"
            "  -r  Number of samples (default: %d)\n"
            "  -c  Logical CPU for both processes "
            "(default: first allowed CPU on Linux)\n",
            program, DEFAULT_REPEATS);
}

static uint64_t parse_positive(const char *text, const char *option)
{
    char *end = NULL;
    uintmax_t value;

    errno = 0;
    value = strtoumax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 ||
        value > UINT64_MAX) {
        fprintf(stderr, "Invalid value for %s: %s\n", option, text);
        exit(EXIT_FAILURE);
    }
    return (uint64_t)value;
}

static int parse_cpu(const char *text)
{
    char *end = NULL;
    uintmax_t value;

    errno = 0;
    value = strtoumax(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        fprintf(stderr, "Invalid value for -c: %s\n", text);
        exit(EXIT_FAILURE);
    }

#ifdef __linux__
    if (value >= CPU_SETSIZE) {
        fprintf(stderr, "CPU must be less than %d\n", CPU_SETSIZE);
        exit(EXIT_FAILURE);
    }
#else
    if (value > INT32_MAX) {
        fprintf(stderr, "CPU number is too large\n");
        exit(EXIT_FAILURE);
    }
#endif
    return (int)value;
}

static uint64_t now_ns(void)
{
    struct timespec time;

    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)time.tv_sec * UINT64_C(1000000000) +
           (uint64_t)time.tv_nsec;
}

#ifdef __linux__
static int first_allowed_cpu(void)
{
    cpu_set_t allowed;
    int cpu;

    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) == -1) {
        perror("sched_getaffinity");
        exit(EXIT_FAILURE);
    }
    for (cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (CPU_ISSET(cpu, &allowed)) {
            return cpu;
        }
    }

    fprintf(stderr, "The process has no allowed CPUs\n");
    exit(EXIT_FAILURE);
}

static void pin_to_cpu(int cpu)
{
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        fprintf(stderr, "Cannot pin process %ld to CPU %d: %s\n",
                (long)getpid(), cpu, strerror(errno));
        exit(EXIT_FAILURE);
    }
}
#else
static int first_allowed_cpu(void)
{
    return 0;
}

static void pin_to_cpu(int cpu)
{
    (void)cpu;
    fprintf(stderr,
            "warning: strict CPU affinity is not available on this platform; "
            "results may include CPU migration\n");
}
#endif

static int compare_u64(const void *left, const void *right)
{
    const uint64_t a = *(const uint64_t *)left;
    const uint64_t b = *(const uint64_t *)right;

    return (a > b) - (a < b);
}

static uint64_t median(uint64_t *values, size_t count)
{
    qsort(values, count, sizeof(*values), compare_u64);
    if ((count & 1U) != 0) {
        return values[count / 2];
    }
    return values[count / 2 - 1] / 2 + values[count / 2] / 2 +
           ((values[count / 2 - 1] & 1U) && (values[count / 2] & 1U));
}

static int write_byte(int fd, unsigned char byte)
{
    ssize_t result;

    do {
        result = write(fd, &byte, 1);
    } while (result == -1 && errno == EINTR);
    if (result == 1) {
        return 0;
    }
    if (result == -1) {
        perror("write");
    } else {
        fprintf(stderr, "Short write to pipe\n");
    }
    return -1;
}

/* Returns 1 for a byte, 0 for end-of-file, and -1 for an error. */
static int read_byte(int fd, unsigned char *byte)
{
    ssize_t result;

    do {
        result = read(fd, byte, 1);
    } while (result == -1 && errno == EINTR);
    if (result == 1) {
        return 1;
    }
    if (result == 0) {
        return 0;
    }
    perror("read");
    return -1;
}

static void close_fd(int fd)
{
    if (close(fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }
}

static void child_ping_pong(int receive_fd, int send_fd, int cpu)
{
    unsigned char byte;
    int status;

#ifdef __linux__
    pin_to_cpu(cpu);
#else
    (void)cpu;
#endif
    for (;;) {
        status = read_byte(receive_fd, &byte);
        if (status == 0) {
            break;
        }
        if (status == -1 || write_byte(send_fd, byte) == -1) {
            _exit(EXIT_FAILURE);
        }
    }
    close(receive_fd);
    close(send_fd);
    _exit(EXIT_SUCCESS);
}

static uint64_t measure_process_round_trips(uint64_t iterations, int cpu)
{
    int to_child[2];
    int to_parent[2];
    pid_t child;
    uint64_t round;
    uint64_t start;
    uint64_t end;
    unsigned char byte = 0x5a;
    int child_status;

    if (pipe(to_child) == -1 || pipe(to_parent) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    child = fork();
    if (child == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (child == 0) {
        close(to_child[1]);
        close(to_parent[0]);
        child_ping_pong(to_child[0], to_parent[1], cpu);
    }

    close_fd(to_child[0]);
    close_fd(to_parent[1]);

    for (round = 0; round < WARMUP_ROUNDS; ++round) {
        if (write_byte(to_child[1], byte) == -1 ||
            read_byte(to_parent[0], &byte) != 1) {
            exit(EXIT_FAILURE);
        }
    }

    start = now_ns();
    for (round = 0; round < iterations; ++round) {
        if (write_byte(to_child[1], byte) == -1 ||
            read_byte(to_parent[0], &byte) != 1) {
            exit(EXIT_FAILURE);
        }
    }
    end = now_ns();

    close_fd(to_child[1]);
    close_fd(to_parent[0]);
    if (waitpid(child, &child_status, 0) == -1) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }
    if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS) {
        fprintf(stderr, "Child process failed\n");
        exit(EXIT_FAILURE);
    }
    return end - start;
}

static uint64_t measure_local_pipe_round_trips(uint64_t iterations)
{
    int first[2];
    int second[2];
    uint64_t round;
    uint64_t start;
    uint64_t end;
    unsigned char byte = 0x5a;

    if (pipe(first) == -1 || pipe(second) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    start = now_ns();
    for (round = 0; round < iterations; ++round) {
        if (write_byte(first[1], byte) == -1 ||
            read_byte(first[0], &byte) != 1 ||
            write_byte(second[1], byte) == -1 ||
            read_byte(second[0], &byte) != 1) {
            exit(EXIT_FAILURE);
        }
    }
    end = now_ns();

    close_fd(first[0]);
    close_fd(first[1]);
    close_fd(second[0]);
    close_fd(second[1]);
    return end - start;
}

int main(int argc, char **argv)
{
    uint64_t iterations = UINT64_C(100000);
    uint64_t repeats = DEFAULT_REPEATS;
    uint64_t *process_times;
    uint64_t *local_times;
    uint64_t *switch_times;
    uint64_t process_median;
    uint64_t local_median;
    uint64_t switch_median;
    uint64_t sample;
    int cpu = -1;
    int option;

    while ((option = getopt(argc, argv, "n:r:c:h")) != -1) {
        switch (option) {
        case 'n':
            iterations = parse_positive(optarg, "-n");
            break;
        case 'r':
            repeats = parse_positive(optarg, "-r");
            if (repeats > SIZE_MAX / sizeof(*process_times)) {
                fprintf(stderr, "Too many samples\n");
                return EXIT_FAILURE;
            }
            break;
        case 'c':
            cpu = parse_cpu(optarg);
            break;
        case 'h':
            usage(argv[0]);
            return EXIT_SUCCESS;
        default:
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (optind != argc) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (cpu < 0) {
        cpu = first_allowed_cpu();
    }
    pin_to_cpu(cpu);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return EXIT_FAILURE;
    }

    process_times = malloc((size_t)repeats * sizeof(*process_times));
    local_times = malloc((size_t)repeats * sizeof(*local_times));
    switch_times = malloc((size_t)repeats * sizeof(*switch_times));
    if (process_times == NULL || local_times == NULL || switch_times == NULL) {
        perror("malloc");
        free(process_times);
        free(local_times);
        free(switch_times);
        return EXIT_FAILURE;
    }

    for (sample = 0; sample < repeats; ++sample) {
        local_times[sample] = measure_local_pipe_round_trips(iterations);
        process_times[sample] = measure_process_round_trips(iterations, cpu);
        switch_times[sample] = process_times[sample] > local_times[sample]
                                   ? (process_times[sample] - local_times[sample]) / 2
                                   : 0;
    }

    process_median = median(process_times, (size_t)repeats);
    local_median = median(local_times, (size_t)repeats);
    switch_median = median(switch_times, (size_t)repeats);

    printf("Process context-switch benchmark (pipe ping-pong)\n");
#ifdef __linux__
    printf("CPU:                       %d (both processes pinned)\n", cpu);
#else
    printf("Requested CPU:             %d (not strictly pinned)\n", cpu);
#endif
    printf("Round trips/sample:        %" PRIu64 "\n", iterations);
    printf("Samples:                   %" PRIu64 "\n", repeats);
    printf("Median process round trip: %.2f ns\n",
           (double)process_median / (double)iterations);
    printf("Median local pipe baseline: %.2f ns/round trip\n",
           (double)local_median / (double)iterations);
    printf("Estimated context switch:  %.2f ns/switch\n",
           (double)switch_median / (double)iterations);
    printf("Note: each process round trip causes two context switches; the\n"
           "      estimate subtracts four same-process pipe operations.\n");

    free(switch_times);
    free(local_times);
    free(process_times);
    return EXIT_SUCCESS;
}
