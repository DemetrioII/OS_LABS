#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <string.h>

int MAX_THREADS = 4;

int min(int x, int y) {
    if (x < y) return x;
    else return y;
}

pthread_t* threads;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t barrier;

typedef struct {
    double x, y;
} Point;

typedef struct {
    double x, y;
    int count;
} Centroid;

Point* points;
int num_points;
Centroid* centroids;

typedef struct {
    int start;
    int end;
    int *changed;
    pthread_mutex_t* changed_mutex;
} Thread_data;

int* centroid_ids, *new_centroid_ids;
int k, n;

double distance(Point a, Point b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

double _distance(Point a, Centroid b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

void *compute_clusters(void *arg) {
    int i = *((int*)arg);
    free(arg);
    double sum_x = 0.0, sum_y = 0.0;
    int count = 0;
    for (int j = 0; j < n; ++j) {
        if (centroid_ids[j] == i) {
            sum_x += points[j].x;
            sum_y += points[j].y;
            ++count;
        }
    }

    if (count > 0) {
        centroids[i].x = sum_x / count;
        centroids[i].y = sum_y / count;
    }

    return NULL;
}

void* recalculate_centroids(void *arg) {
    Thread_data* td = (Thread_data*)arg;
    for (int j = td->start; j < td->end; ++j) {
        new_centroid_ids[j] = centroid_ids[j];
        for (int i = 0; i < k; ++i) {
            if (_distance(points[j], centroids[i]) < _distance(points[j], centroids[new_centroid_ids[j]])) {
                new_centroid_ids[j] = i;
            }
        }
        if (centroid_ids[j] != new_centroid_ids[j]) {
            pthread_mutex_lock(td->changed_mutex);
            *(td->changed) = 1;
            pthread_mutex_unlock(td->changed_mutex);
        }
    }

    return NULL;
}


int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            MAX_THREADS = atoi(argv[i + 1]);
            if (MAX_THREADS <= 0) {
                fprintf(stderr, "Ошибка: количество потоков должно быть положительным числом\n");
                return 1;
            }
            i++;
        }
        else {
            fprintf(stderr, "Неизвестный аргумент: %s\n", argv[i]);
            return 1;
        }
    }

    scanf("%d\n%d", &k, &n);
    centroids = (Centroid*)malloc(sizeof(Centroid) * k);
    points = (Point*)malloc(sizeof(Point) * n);
    centroid_ids = (int*)malloc(sizeof(int) * n);

    threads = (pthread_t*)malloc(sizeof(pthread_t) * k);

    pthread_mutex_t changed_mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < n; ++i) {
        scanf("%lf %lf", &points[i].x, &points[i].y);
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < k; ++i) {
        centroids[i].x = points[i].x;
        centroids[i].y = points[i].y;
        centroids[i].count = 1;
        centroid_ids[i] = i;
    }

    for (int i = k; i < n; ++i) {
        int idx = 0;
        for (int j = 0; j < k; ++j) {
            if (_distance(points[i], centroids[j]) < _distance(points[i], centroids[idx])) {
                idx = j;
            }
        }
        centroid_ids[i] = idx;
        centroids[idx].count++;
    }

    new_centroid_ids = (int*)malloc(sizeof(int) * n);

    while (1) {
        int t = 0;
        
        printf("Создано потоков для пересчета центроидов: %d\n", k);
        for (int i = 0; i < k; ++i) {
            int *arg = (int*)malloc(sizeof(int));
            *arg = i;
            pthread_create(&threads[i], NULL, compute_clusters, arg);
        }
        for (int i = 0; i < k; ++i) {
            pthread_join(threads[i], NULL);
        }
        
        int num_threads = min(MAX_THREADS, n); // Не больше потоков, чем точек
        printf("Создано потоков для перераспределения точек: %d (ограничение: %d)\n", num_threads, MAX_THREADS);
        
        pthread_t* assign_threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
        Thread_data* args = (Thread_data*) malloc(num_threads * sizeof(Thread_data));

        int chunk = n / num_threads;
        for (int th = 0; th < num_threads; ++th) {
            args[th].start = th * chunk;
            args[th].end = (th == num_threads - 1) ? n : (th + 1) * chunk;
            args[th].changed = &t;
            args[th].changed_mutex = &changed_mutex;

            pthread_create(&assign_threads[th], NULL, recalculate_centroids, &args[th]);
        }

        for (int th = 0; th < num_threads; ++th) {
            pthread_join(assign_threads[th], NULL);
        }

        for (int i = 0; i < n; ++i) {
            centroid_ids[i] = new_centroid_ids[i];
        }
        for (int i = 0; i < k; ++i) {
            centroids[i].count = 0;
        }
        for (int i = 0; i < n; ++i) {
            centroids[centroid_ids[i]].count++;
        }

        free(assign_threads);
        free(args);

        if (!t)
            break;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    for (int i = 0; i < k; ++i) {
        printf("%f %f\n", centroids[i].x, centroids[i].y);
    }

    printf("Время выполнения: %lf секунд\n", time_sec);
    printf("Максимальное количество потоков: %d\n", MAX_THREADS);

    free(centroid_ids);
    free(centroids);
    free(points);
    free(new_centroid_ids);
    free(threads);
    return 0;
}
