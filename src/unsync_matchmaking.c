#include "common.h"

typedef struct {
    int id;
    int occupancy;
    int entries;
    int exits;
} UnsyncRoom;

static UnsyncRoom rooms[MAX_ROOMS];
static Player players[MAX_PLAYERS];

static int g_completed = 0;
static int g_race_detected = 0;
static int g_inconsistency_detected = 0;
static int g_total_entries = 0;
static int g_total_exits = 0;

static int cli_players = MAX_PLAYERS;
static int cli_rounds = 3;

static void random_delay(unsigned int *seed, int min_us, int max_us) {
    int span = max_us - min_us + 1;
    int d = min_us + (int)(rand_r(seed) % span);
    usleep((useconds_t)d);
}

static void try_join_room_unsafely(int player_id, unsigned int *seed, int *joined_room) {
    int start = (int)(rand_r(seed) % MAX_ROOMS);
    int k;

    for (k = 0; k < MAX_ROOMS; k++) {
        int idx = (start + k) % MAX_ROOMS;
        int seen = rooms[idx].occupancy;

        if (seen < ROOM_CAPACITY) {
            random_delay(seed, 500, 4000);

            rooms[idx].occupancy = seen + 1;
            rooms[idx].entries++;
            g_total_entries++;

            printf("[ROOM %d] jugador %d entro. Ocupacion: %d/%d\n",
                   rooms[idx].id, player_id, rooms[idx].occupancy, ROOM_CAPACITY);

            if (rooms[idx].occupancy > ROOM_CAPACITY) {
                printf("[RACE] Sala %d excedio capacidad: %d/%d\n",
                       rooms[idx].id, rooms[idx].occupancy, ROOM_CAPACITY);
                g_race_detected = 1;
            }

            *joined_room = idx;
            return;
        }
    }

    *joined_room = -1;
}

static void *player_thread_unsync(void *arg) {
    Player *p = (Player *)arg;
    unsigned int seed = rng_seed() ^ (unsigned int)p->id;
    int round;

    for (round = 0; round < cli_rounds; round++) {
        int joined = -1;
        int retry = 0;

        printf("[PLAYER %d] buscando sala... (ronda %d)\n", p->id, round + 1);

        while (joined < 0) {
            try_join_room_unsafely(p->id, &seed, &joined);
            if (joined < 0) {
                retry++;
                printf("[PLAYER %d] sin sala inmediata, reintentando (%d)...\n", p->id, retry);
                random_delay(&seed, 700, 3000);
            }
        }

        p->assigned_room = joined;
        p->play_time_sec = 1 + (int)(rand_r(&seed) % 2);
        sleep((unsigned int)p->play_time_sec);

        random_delay(&seed, 500, 3500);
        rooms[joined].occupancy--;
        rooms[joined].exits++;
        g_total_exits++;

        printf("[ROOM %d] jugador %d salio. Ocupacion: %d/%d\n",
               rooms[joined].id, p->id, rooms[joined].occupancy, ROOM_CAPACITY);

        if (rooms[joined].occupancy < 0) {
            printf("[ERROR] Ocupacion negativa detectada en sala %d\n", rooms[joined].id);
            g_inconsistency_detected = 1;
        }

        random_delay(&seed, 500, 2500);
    }

    p->completed = 1;
    g_completed++;
    return NULL;
}

static void init_data(void) {
    int i;
    for (i = 0; i < MAX_ROOMS; i++) {
        rooms[i].id = i + 1;
        rooms[i].occupancy = 0;
        rooms[i].entries = 0;
        rooms[i].exits = 0;
    }
    for (i = 0; i < cli_players; i++) {
        players[i].id = i + 1;
        players[i].play_time_sec = 0;
        players[i].assigned_room = -1;
        players[i].completed = 0;
    }
}

static void print_summary(void) {
    int i;
    int final_occ = 0;
    int expected_ops = cli_players * cli_rounds;

    printf("\n========== RESUMEN UNSYNC ==========" "\n");
    for (i = 0; i < MAX_ROOMS; i++) {
        printf("[ROOM %d] entries=%d exits=%d final_occ=%d\n",
               rooms[i].id, rooms[i].entries, rooms[i].exits, rooms[i].occupancy);
        final_occ += rooms[i].occupancy;
        if (rooms[i].occupancy != 0) {
            g_inconsistency_detected = 1;
        }
    }

    printf("[GLOBAL] completados=%d/%d\n", g_completed, cli_players);
    printf("[GLOBAL] entradas=%d salidas=%d esperadas=%d\n",
           g_total_entries, g_total_exits, expected_ops);

    if (g_total_entries != g_total_exits || g_total_entries != expected_ops || final_occ != 0) {
        g_inconsistency_detected = 1;
    }

    if (g_race_detected) {
        printf("[RACE] Se detectaron carreras de datos durante el acceso a cupos.\n");
    }
    if (g_inconsistency_detected) {
        printf("[ERROR] Conteo inconsistente detectado.\n");
    }
    if (!g_race_detected && !g_inconsistency_detected) {
        printf("[WARN] Esta corrida no mostro fallo visible. Repite para observar no determinismo.\n");
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
        int r = atoi(argv[2]);
        if (r > 0 && r <= 20) {
            cli_rounds = r;
        }
    }

    printf("=== Matchmaking UNSYNC | players=%d rooms=%d cap=%d rounds=%d ===\n",
           cli_players, MAX_ROOMS, ROOM_CAPACITY, cli_rounds);

    init_data();

    for (i = 0; i < cli_players; i++) {
        if (pthread_create(&tids[i], NULL, player_thread_unsync, &players[i]) != 0) {
            fprintf(stderr, "Error creando hilo %d\n", i + 1);
            return 1;
        }
    }

    for (i = 0; i < cli_players; i++) {
        pthread_join(tids[i], NULL);
    }

    print_summary();
    return 0;
}
