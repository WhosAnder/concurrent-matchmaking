#include "common.h"

#ifdef __APPLE__
#define USE_MACOS_SEM_FALLBACK 1
#else
#define USE_MACOS_SEM_FALLBACK 0
#endif

static Room rooms[MAX_ROOMS];
static Player players[MAX_PLAYERS];
static WaitingQueue waiting_queue;

static pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t active_rooms_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t room_available_cond = PTHREAD_COND_INITIALIZER;

static int active_rooms = 0;
static int players_completed = 0;
static int players_inside = 0;
static int cli_players = MAX_PLAYERS;
static int cli_sleep_max = 2;

static int created_total = 0;
static int closed_total = 0;
static int over_capacity_detected = 0;

static void init_data(void) {
    int i;
    for (i = 0; i < MAX_ROOMS; i++) {
        rooms[i].id = i + 1;
        rooms[i].capacity = ROOM_CAPACITY;
        rooms[i].current_players = 0;
        rooms[i].is_active = 0;
        rooms[i].total_entries = 0;
        rooms[i].total_exits = 0;
#if !USE_MACOS_SEM_FALLBACK
        if (sem_init(&rooms[i].slots, 0, ROOM_CAPACITY) != 0) {
            perror("sem_init");
            exit(1);
        }
#endif
    }
    for (i = 0; i < cli_players; i++) {
        players[i].id = i + 1;
        players[i].play_time_sec = 0;
        players[i].assigned_room = -1;
        players[i].completed = 0;
    }
    queue_init(&waiting_queue);
}

static int room_slot_try_acquire(int idx) {
#if USE_MACOS_SEM_FALLBACK
    if (rooms[idx].current_players < ROOM_CAPACITY) {
        return 0;
    }
    errno = EAGAIN;
    return -1;
#else
    {
        return sem_trywait(&rooms[idx].slots);
    }
#endif
}

static void room_slot_release(int idx) {
#if USE_MACOS_SEM_FALLBACK
    (void)idx;
#else
    {
        sem_post(&rooms[idx].slots);
    }
#endif
}

static int activate_room_if_possible(void) {
    int i;
    for (i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].is_active) {
            rooms[i].is_active = 1;
            rooms[i].current_players = 0;
#if !USE_MACOS_SEM_FALLBACK
            sem_destroy(&rooms[i].slots);
            if (sem_init(&rooms[i].slots, 0, ROOM_CAPACITY) != 0) {
                perror("sem_init");
                exit(1);
            }
#endif
            pthread_mutex_lock(&active_rooms_mutex);
            active_rooms++;
            created_total++;
            pthread_mutex_unlock(&active_rooms_mutex);
            printf("[SERVER] Sala %d activada. Salas activas: %d\n", rooms[i].id, active_rooms);
            return i;
        }
    }
    return -1;
}

static int find_active_room_with_slot(int *room_idx) {
    int i;
    for (i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].is_active) {
            if (room_slot_try_acquire(i) == 0) {
                *room_idx = i;
                return 0;
            }
        }
    }
    return -1;
}

static void maybe_close_room_if_empty(int idx) {
    if (rooms[idx].is_active && rooms[idx].current_players == 0) {
        rooms[idx].is_active = 0;
        pthread_mutex_lock(&active_rooms_mutex);
        if (active_rooms > 0) {
            active_rooms--;
        }
        closed_total++;
        pthread_mutex_unlock(&active_rooms_mutex);
        printf("[SERVER] Sala %d marcada inactiva. Salas activas: %d\n", rooms[idx].id, active_rooms);
    }
}

static void *player_thread_sync(void *arg) {
    Player *p = (Player *)arg;
    unsigned int seed = rng_seed() ^ (unsigned int)p->id;
    int my_room = -1;

    printf("[PLAYER %d] buscando sala...\n", p->id);

    while (my_room < 0) {
        pthread_mutex_lock(&rooms_mutex);

        if (find_active_room_with_slot(&my_room) == 0) {
            rooms[my_room].current_players++;
            rooms[my_room].total_entries++;
            if (rooms[my_room].current_players > ROOM_CAPACITY) {
                over_capacity_detected = 1;
            }
            pthread_mutex_lock(&players_mutex);
            players_inside++;
            pthread_mutex_unlock(&players_mutex);

            printf("[ROOM %d] jugador %d entro. Ocupacion: %d/%d\n",
                   rooms[my_room].id, p->id, rooms[my_room].current_players, ROOM_CAPACITY);
            pthread_mutex_unlock(&rooms_mutex);
            break;
        }

        if (active_rooms < MAX_ROOMS) {
            int created_idx = activate_room_if_possible();
            if (created_idx >= 0 && room_slot_try_acquire(created_idx) == 0) {
                my_room = created_idx;
                rooms[my_room].current_players++;
                rooms[my_room].total_entries++;
                pthread_mutex_lock(&players_mutex);
                players_inside++;
                pthread_mutex_unlock(&players_mutex);

                printf("[ROOM %d] jugador %d entro. Ocupacion: %d/%d\n",
                       rooms[my_room].id, p->id, rooms[my_room].current_players, ROOM_CAPACITY);
                pthread_mutex_unlock(&rooms_mutex);
                break;
            }
        }

        pthread_mutex_lock(&queue_mutex);
        if (queue_push(&waiting_queue, p->id) == 0) {
            printf("[PLAYER %d] esperando sala disponible...\n", p->id);
        } else {
            printf("[PLAYER %d] cola llena, reintentando...\n", p->id);
        }
        pthread_mutex_unlock(&queue_mutex);

        pthread_cond_wait(&room_available_cond, &rooms_mutex);
        pthread_mutex_unlock(&rooms_mutex);
    }

    p->assigned_room = my_room;
    p->play_time_sec = 1 + (int)(rand_r(&seed) % cli_sleep_max);
    sleep((unsigned int)p->play_time_sec);

    pthread_mutex_lock(&rooms_mutex);
    rooms[my_room].current_players--;
    rooms[my_room].total_exits++;
    if (rooms[my_room].current_players < 0) {
        rooms[my_room].current_players = 0;
    }

    pthread_mutex_lock(&players_mutex);
    players_inside--;
    players_completed++;
    pthread_mutex_unlock(&players_mutex);

    printf("[ROOM %d] jugador %d salio. Ocupacion: %d/%d\n",
           rooms[my_room].id, p->id, rooms[my_room].current_players, ROOM_CAPACITY);

    room_slot_release(my_room);
    maybe_close_room_if_empty(my_room);

    pthread_mutex_lock(&queue_mutex);
    if (waiting_queue.count > 0) {
        int woke_id;
        queue_pop(&waiting_queue, &woke_id);
        printf("[SIGNAL] espacio disponible, despertando jugador en espera (id=%d).\n", woke_id);
    } else {
        printf("[SIGNAL] espacio disponible, no hay cola activa.\n");
    }
    pthread_mutex_unlock(&queue_mutex);

    pthread_cond_broadcast(&room_available_cond);
    pthread_mutex_unlock(&rooms_mutex);

    p->completed = 1;
    return NULL;
}

static void destroy_resources(void) {
    int i;
    for (i = 0; i < MAX_ROOMS; i++) {
#if !USE_MACOS_SEM_FALLBACK
        {
            sem_destroy(&rooms[i].slots);
        }
#endif
    }
    pthread_mutex_destroy(&rooms_mutex);
    pthread_mutex_destroy(&players_mutex);
    pthread_mutex_destroy(&active_rooms_mutex);
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&room_available_cond);
}

static void print_summary(void) {
    int i;
    int final_occ = 0;
    int local_over = 0;

    printf("\n========== RESUMEN SYNC ==========" "\n");
    for (i = 0; i < MAX_ROOMS; i++) {
        printf("[ROOM %d] activa=%d entries=%d exits=%d occ=%d/%d\n",
               rooms[i].id,
               rooms[i].is_active,
               rooms[i].total_entries,
               rooms[i].total_exits,
               rooms[i].current_players,
               ROOM_CAPACITY);
        if (rooms[i].current_players > ROOM_CAPACITY) {
            local_over = 1;
        }
        final_occ += rooms[i].current_players;
    }

    printf("[GLOBAL] completados=%d/%d dentro=%d\n", players_completed, cli_players, players_inside);
    printf("[GLOBAL] salas creadas=%d salas cerradas=%d\n", created_total, closed_total);

    if (!over_capacity_detected && !local_over) {
        printf("[OK] Ninguna sala excedio su capacidad.\n");
    } else {
        printf("[ERROR] Se detecto sobrecupo, revisar sincronizacion.\n");
    }

    if (players_completed == cli_players && final_occ == 0 && players_inside == 0) {
        printf("[OK] Todos los jugadores terminaron correctamente.\n");
    } else {
        printf("[ERROR] Conteo final inconsistente.\n");
    }
}

int main(int argc, char **argv) {
    pthread_t tids[MAX_PLAYERS];
    int i;

    if (argc > 1) {
        int n = atoi(argv[1]);
        if (n > 0 && n <= MAX_PLAYERS) {
            cli_players = n;
        }
    }
    if (argc > 2) {
        int s = atoi(argv[2]);
        if (s > 0 && s <= 5) {
            cli_sleep_max = s;
        }
    }

    printf("=== Matchmaking SYNC | players=%d rooms=%d cap=%d ===\n",
           cli_players, MAX_ROOMS, ROOM_CAPACITY);

    init_data();

#if USE_MACOS_SEM_FALLBACK
    {
        printf("[WARN] sem_init no disponible en este sistema; usando fallback local para ejecutar demo.\n");
    }
#endif

    pthread_mutex_lock(&rooms_mutex);
    activate_room_if_possible();
    pthread_mutex_unlock(&rooms_mutex);

    for (i = 0; i < cli_players; i++) {
        if (pthread_create(&tids[i], NULL, player_thread_sync, &players[i]) != 0) {
            fprintf(stderr, "Error creando hilo %d\n", i + 1);
            return 1;
        }
    }

    for (i = 0; i < cli_players; i++) {
        pthread_join(tids[i], NULL);
    }

    print_summary();
    destroy_resources();
    return 0;
}
