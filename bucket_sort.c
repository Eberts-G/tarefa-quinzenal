#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int *data;
    int count;
    int capacity;
} Bucket;

static int compare_ints(const void *a, const void *b) {
    const int left = *(const int *)a;
    const int right = *(const int *)b;
    return (left > right) - (left < right);
}

static void append_to_bucket(Bucket *bucket, int value) {
    if (bucket->count == bucket->capacity) {
        int new_capacity = (bucket->capacity == 0) ? 8 : bucket->capacity * 2;
        int *new_data = realloc(bucket->data, (size_t)new_capacity * sizeof(int));
        if (new_data == NULL) {
            fprintf(stderr, "Erro: falha ao alocar o balde.\n");
            exit(EXIT_FAILURE);
        }
        bucket->data = new_data;
        bucket->capacity = new_capacity;
    }

    bucket->data[bucket->count++] = value;
}

static void free_buckets(Bucket *buckets, int bucket_count) {
    for (int i = 0; i < bucket_count; i++) {
        free(buckets[i].data);
    }
    free(buckets);
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

static void bucket_sort_sequential(int *array, int n) {
    if (n <= 1) {
        return;
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
        perror("Erro ao alocar os baldes do bucket sort");
        exit(EXIT_FAILURE);
    }

    long long range = (long long)max_value - (long long)min_value;

    for (int i = 0; i < n; i++) {
        int index = 0;
        if (range == 0) {
            index = 0;
        } else {
            long long scaled = ((long long)array[i] - (long long)min_value) * (long long)bucket_count / (range + 1);
            index = (int)scaled;
            if (index >= bucket_count) {
                index = bucket_count - 1;
            }
        }
        append_to_bucket(&buckets[index], array[i]);
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
}

static int is_sorted_ascending(const int *array, int n) {
    for (int i = 1; i < n; i++) {
        if (array[i - 1] > array[i]) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    const char *file_name = (argc > 1) ? argv[1] : "entradas/pequena.txt";

    int n = 0;
    int *array = read_input(file_name, &n);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    bucket_sort_sequential(array, n);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (double)(end.tv_sec - start.tv_sec)
                   + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;

    if (!is_sorted_ascending(array, n)) {
        fprintf(stderr, "Resultado inválido: o vetor não está ordenado de forma crescente.\n");
        free(array);
        return EXIT_FAILURE;
    }

    printf("MODALIDADE: SEQUENCIAL\n");
    printf("ARQUIVO: %s\n", file_name);
    printf("N: %d\n", n);
    printf("VALIDO: SIM\n");
    printf("TEMPO_SEGUNDOS: %.9f\n", elapsed);
    printf("PRIMEIRO: %d\n", array[0]);
    printf("ULTIMO: %d\n", array[n - 1]);

    free(array);
    return EXIT_SUCCESS;
}

