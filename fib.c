#include <stdio.h>
#include <time.h>

double fib(double n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double result = fib(35);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    
    printf("Fibonacci(35) = %f\n", result);
    printf("Time taken: %f seconds\n", elapsed);
    return 0;
}
