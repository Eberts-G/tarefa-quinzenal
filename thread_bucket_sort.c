#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * Bucket Sort paralelo com pthreads.
 *
 * Estratégia: o vetor é dividido em blocos contíguos; cada thread aplica o
 * MESMO bucket sort da versão sequencial (bucket_sort.c) sobre o seu bloco,
 * em memória exclusiva (nenhum dado compartilhado é escrito por duas threads,
 * portanto não há necessidade de mutex). Depois do join, a thread principal
 * intercala (merge k-way) os blocos já ordenados.
 *
 * Região medida (igual ao critério do sequencial: tudo que acontece depois
 * da leitura do arquivo e até o vetor final estar ordenado):
 *   criação das threads + bucket sort dos blocos + join + merge.
 * Fora da medição: leitura do arquivo, cópia de referência, verificação.
 */

typedef struct {
    int *data;
    int count;
    int capacity;
} Bucket;

typedef struct {
    int *slice;   /* bloco do vetor original, ordenado no lugar */
    int size;
    int status;   /* 0 = ok, -1 = falha de alocação */
} ThreadData;

static int compare_ints(const void *a, const void *b) {
    const int left = *(const int *)a;
    const int right = *(const int *)b;
    return (left > right) - (left < right);
}

static void free_buckets(Bucket *buckets, int bucket_count) {
    for (int i = 0; i < bucket_count; i++) {
        free(buckets[i].data);
    }
    free(buckets);
}

/* Mesmo algoritmo de bucket_sort.c. Retorna 0 em sucesso, -1 em falha. */
static int bucket_sort_sequential(int *array, int n) {
    if (n <= 1) {
        return 0;
    }

    int min_value = array[0];
    int max_value = array[0];
    for (int i = 1; i < n; i++) {
        if (array[i] < min_value) {
            min_value = array[i];
        }
        if (array[i] > max_value) {
            max_value = array[i];
        }
    }

    const int bucket_count = n;
    Bucket *buckets = calloc((size_t)bucket_count, sizeof(Bucket));
    if (buckets == NULL) {
        return -1;
    }

    long long range = (long long)max_value - (long long)min_value;

    for (int i = 0; i < n; i++) {
        int index = 0;
        if (range != 0) {
            long long scaled = ((long long)array[i] - (long long)min_value) * (long long)bucket_count / (range + 1);
            index = (int)scaled;
            if (index >= bucket_count) {
                index = bucket_count - 1;
            }
        }

        Bucket *bucket = &buckets[index];
        if (bucket->count == bucket->capacity) {
            int new_capacity = (bucket->capacity == 0) ? 8 : bucket->capacity * 2;
            int *new_data = realloc(bucket->data, (size_t)new_capacity * sizeof(int));
            if (new_data == NULL) {
                free_buckets(buckets, bucket_count);
                return -1;
            }
            bucket->data = new_data;
            bucket->capacity = new_capacity;
        }
        bucket->data[bucket->count++] = array[i];
    }

    int write_index = 0;
    for (int i = 0; i < bucket_count; i++) {
        if (buckets[i].count > 0) {
            qsort(buckets[i].data, (size_t)buckets[i].count, sizeof(int), compare_ints);
            for (int j = 0; j < buckets[i].count; j++) {
                array[write_index++] = buckets[i].data[j];
            }
        }
    }

    free_buckets(buckets, bucket_count);
    return 0;
}

static int *read_input(const char *file_name, int *size_out) {
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    int n = 0;
    if (fscanf(file, "%d", &n) != 1 || n <= 0) {
        fprintf(stderr, "Formato inválido: a primeira linha deve conter N > 0.\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    int *array = malloc((size_t)n * sizeof(int));
    if (array == NULL) {
        perror("Erro ao alocar memória");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n; i++) {
        if (fscanf(file, "%d", &array[i]) != 1) {
            fprintf(stderr, "Formato inválido: faltou o valor %d de %d.\n", i + 1, n);
            free(array);
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }

    fclose(file);
    *size_out = n;
    return array;
}

static int is_sorted_ascending(const int *array, int n) {
    for (int i = 1; i < n; i++) {
        if (array[i - 1] > array[i]) {
            return 0;
        }
    }
    return 1;
}

/* Intercala 'count' blocos ordenados em 'output'. Retorna 0 ou -1. */
static int merge_sorted_runs(const ThreadData *runs, int count, int *output) {
    int *indices = calloc((size_t)count, sizeof(int));
    if (indices == NULL) {
        return -1;
    }

    int remaining = 0;
    for (int i = 0; i < count; i++) {
        remaining += runs[i].size;
    }

    int write_index = 0;
    while (remaining > 0) {
        int best_index = -1;
        for (int i = 0; i < count; i++) {
            if (indices[i] >= runs[i].size) {
                continue;
            }
            /* Compara pelo índice, e não por um valor sentinela, para que
               INT_MAX na entrada também seja tratado corretamente. */
            if (best_index == -1 ||
                runs[i].slice[indices[i]] < runs[best_index].slice[indices[best_index]]) {
                best_index = i;
            }
        }

        output[write_index++] = runs[best_index].slice[indices[best_index]];
        indices[best_index]++;
        remaining--;
    }

    free(indices);
    return 0;
}

static void *worker_sort(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    data->status = bucket_sort_sequential(data->slice, data->size);
    return NULL;
}

static int parse_thread_count(const char *text, int *value_out) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (text == end || *end != '\0' || value <= 0 || value > 4096) {
        return -1;
    }
    *value_out = (int)value;
    return 0;
}

static void print_usage(const char *program) {
    fprintf(stderr,
            "Uso: %s [arquivo] [N_THREADS | --threads N_THREADS | --max]\n"
            "  Sem número de threads (ou com --max) usa o total de CPUs lógicas online.\n",
            program);
}

int main(int argc, char *argv[]) {
    const char *file_name = (argc > 1) ? argv[1] : "entradas/pequena.txt";

    int max_cpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (max_cpus <= 0) {
        max_cpus = 1;
    }

    int requested_threads = max_cpus;
    if (argc > 2) {
        int ok = 0;
        if (strcmp(argv[2], "--max") == 0 && argc == 3) {
            ok = 1;
        } else if (strcmp(argv[2], "--threads") == 0 && argc == 4) {
            ok = (parse_thread_count(argv[3], &requested_threads) == 0);
        } else if (argc == 3) {
            ok = (parse_thread_count(argv[2], &requested_threads) == 0);
        }
        if (!ok) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    int exit_code = EXIT_FAILURE;
    int n = 0;
    int *base = read_input(file_name, &n);
    int *expected = NULL;
    int *result = NULL;
    ThreadData *threads = NULL;
    pthread_t *handles = NULL;
    int created = 0;
    int failed = 0;

    if (requested_threads > n) {
        requested_threads = n;
    }

    expected = malloc((size_t)n * sizeof(int));
    result = malloc((size_t)n * sizeof(int));
    threads = calloc((size_t)requested_threads, sizeof(ThreadData));
    handles = calloc((size_t)requested_threads, sizeof(pthread_t));
    if (expected == NULL || result == NULL || threads == NULL || handles == NULL) {
        fprintf(stderr, "Erro ao alocar memória.\n");
        goto cleanup;
    }

    /* Referência para a verificação (fora da medição de tempo). */
    memcpy(expected, base, (size_t)n * sizeof(int));
    if (bucket_sort_sequential(expected, n) != 0) {
        fprintf(stderr, "Erro ao gerar a sequência de referência.\n");
        goto cleanup;
    }

    int block_size = n / requested_threads;
    int remainder = n % requested_threads;
    int offset = 0;
    for (int i = 0; i < requested_threads; i++) {
        int chunk_size = block_size + (i < remainder ? 1 : 0);
        threads[i].slice = base + offset;
        threads[i].size = chunk_size;
        offset += chunk_size;
    }

    /* ---- início da região medida ---- */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < requested_threads; i++) {
        if (pthread_create(&handles[i], NULL, worker_sort, &threads[i]) != 0) {
            fprintf(stderr, "Erro ao criar a thread %d.\n", i);
            failed = 1;
            break;
        }
        created++;
    }

    for (int i = 0; i < created; i++) {
        pthread_join(handles[i], NULL);
    }

    if (!failed) {
        for (int i = 0; i < requested_threads; i++) {
            if (threads[i].status != 0) {
                fprintf(stderr, "Erro: a thread %d falhou ao alocar memória.\n", i);
                failed = 1;
            }
        }
    }

    if (!failed && merge_sorted_runs(threads, requested_threads, result) != 0) {
        fprintf(stderr, "Erro ao alocar índices para a mesclagem.\n");
        failed = 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    /* ---- fim da região medida ---- */

    if (failed) {
        goto cleanup;
    }

    if (!is_sorted_ascending(result, n) || memcmp(result, expected, (size_t)n * sizeof(int)) != 0) {
        fprintf(stderr, "Resultado inválido: a ordem paralela não corresponde à versão sequencial.\n");
        goto cleanup;
    }

    double elapsed = (double)(end.tv_sec - start.tv_sec)
                   + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;

    printf("MODALIDADE: PTHREADS\n");
    printf("ARQUIVO: %s\n", file_name);
    printf("THREADS: %d\n", requested_threads);
    printf("MAX_CPUS_ONLINE: %d\n", max_cpus);
    printf("N: %d\n", n);
    printf("VALIDO: SIM\n");
    printf("TEMPO_SEGUNDOS: %.9f\n", elapsed);
    printf("PRIMEIRO: %d\n", result[0]);
    printf("ULTIMO: %d\n", result[n - 1]);
    exit_code = EXIT_SUCCESS;

cleanup:
    free(threads);
    free(handles);
    free(base);
    free(expected);
    free(result);
    return exit_code;
}
