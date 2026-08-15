#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sched.h>
#endif

enum {
    DEFAULT_REPEATS = 9,
    WARMUP_CALLS = 10000
};

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [-n iterations] [-r repeats] [-c cpu]\n"
            "  -n  Zero-byte read calls per sample (default: 1000000)\n"
            "  -r  Number of samples (default: %d)\n"
            "  -c  Logical CPU to use (default: first allowed CPU on Linux)\n",
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
        fprintf(stderr, "Cannot pin the process to CPU %d: %s\n", cpu,
                strerror(errno));
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

static uint64_t measure_reads(int fd, uint64_t iterations)
{
    char byte = 0;
    uint64_t start;
    uint64_t end;
    uint64_t i;

    start = now_ns();
    for (i = 0; i < iterations; ++i) {
        if (read(fd, &byte, 0) != 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }
    }
    end = now_ns();
    return end - start;
}

static uint64_t measure_loop(uint64_t iterations)
{
    uint64_t start;
    uint64_t end;
    uint64_t i;

    start = now_ns();
    for (i = 0; i < iterations; ++i) {
#if defined(__GNUC__) || defined(__clang__)
        __asm__ volatile("" ::: "memory");
#else
        volatile uint64_t keep_loop = i;
        (void)keep_loop;
#endif
    }
    end = now_ns();
    return end - start;
}

int main(int argc, char **argv)
{
    uint64_t iterations = UINT64_C(1000000);
    uint64_t repeats = DEFAULT_REPEATS;
    uint64_t *read_times;
    uint64_t *loop_times;
    uint64_t read_median;
    uint64_t loop_median;
    uint64_t adjusted;
    int cpu = -1;
    int fd;
    int option;
    uint64_t sample;

    while ((option = getopt(argc, argv, "n:r:c:h")) != -1) {
        switch (option) {
        case 'n':
            iterations = parse_positive(optarg, "-n");
            break;
        case 'r':
            repeats = parse_positive(optarg, "-r");
            if (repeats > SIZE_MAX / sizeof(*read_times)) {
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

    fd = open("/dev/null", O_RDONLY);
    if (fd == -1) {
        perror("open /dev/null");
        return EXIT_FAILURE;
    }

    read_times = malloc((size_t)repeats * sizeof(*read_times));
    loop_times = malloc((size_t)repeats * sizeof(*loop_times));
    if (read_times == NULL || loop_times == NULL) {
        perror("malloc");
        close(fd);
        free(read_times);
        free(loop_times);
        return EXIT_FAILURE;
    }

    (void)measure_reads(fd, WARMUP_CALLS);
    for (sample = 0; sample < repeats; ++sample) {
        loop_times[sample] = measure_loop(iterations);
        read_times[sample] = measure_reads(fd, iterations);
    }

    read_median = median(read_times, (size_t)repeats);
    loop_median = median(loop_times, (size_t)repeats);
    adjusted = read_median > loop_median ? read_median - loop_median : 0;

    printf("Zero-byte read system-call benchmark\n");
#ifdef __linux__
    printf("CPU:                 %d (pinned)\n", cpu);
#else
    printf("Requested CPU:       %d (not strictly pinned)\n", cpu);
#endif
    printf("Iterations/sample:   %" PRIu64 "\n", iterations);
    printf("Samples:             %" PRIu64 "\n", repeats);
    printf("Median raw time:     %.2f ns/call\n",
           (double)read_median / (double)iterations);
    printf("Median loop time:    %.2f ns/iteration\n",
           (double)loop_median / (double)iterations);
    printf("Estimated syscall:   %.2f ns/call\n",
           (double)adjusted / (double)iterations);

    free(loop_times);
    free(read_times);
    if (close(fd) == -1) {
        perror("close");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
