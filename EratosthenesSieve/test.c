#include <stdio.h>
#include <stdlib.h>

void sieve(int, char*);

int main() {
    int num_primes;
    scanf("%d", &num_primes);

    char* prime = malloc(num_primes);
    sieve(num_primes, prime);

    for (int num = 0; num < num_primes; ++num) {
        if (prime[num]) {
            printf("%d ", num);
        }
    }

    free(prime);
}