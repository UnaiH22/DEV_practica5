#include <stdio.h>
#include <stdint.h>
#include <emmintrin.h>  // Para _mm_mfence
#include <x86intrin.h>  // Para rdtsc

#define ITER 10000

// Mide el contador de ciclos del CPU
static inline uint64_t rdtsc(){
    unsigned int lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Vacía la línea de caché que contiene la dirección dada
static inline void clflush(void *p){
    __asm__ volatile("clflush (%0)" :: "r"(p));
}

int main() {
    uint8_t data = 42;
    uint64_t t1, t2;
    uint64_t sum_hit = 0, sum_miss = 0;
    int i;

    // Calcular tiempos de acceso cuando el dato está en caché ("hit")
    for (i = 0; i < ITER; i++) {
        // Asegurarse de que 'data' ya esté en caché (acceso repetido)
        volatile uint8_t dummy = data;
        t1 = rdtsc();
        dummy = data;
        t2 = rdtsc();
        sum_hit += (t2 - t1);
    }
    double avg_hit = (double)sum_hit / ITER;

    // Calcular tiempos de acceso cuando el dato ha sido eliminado de la caché ("miss")
    for (i = 0; i < ITER; i++) {
        clflush(&data);
        _mm_mfence(); // Barrera para asegurar el orden
        t1 = rdtsc();
        volatile uint8_t dummy = data;
        t2 = rdtsc();
        sum_miss += (t2 - t1);
    }
    double avg_miss = (double)sum_miss / ITER;

    printf("Tiempo promedio (hit):  %f ciclos\n", avg_hit);
    printf("Tiempo promedio (miss): %f ciclos\n", avg_miss);

    // El umbral sugerido suele estar en algún punto intermedio entre los dos promedios.
    double threshold = (avg_hit + avg_miss) / 2;
    printf("Umbral sugerido (CACHE_HIT_THRESHOLD): %f ciclos\n", threshold);

    return 0;
}
