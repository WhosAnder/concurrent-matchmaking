//simulacion de un sistema de salas de videojuegos online
//varios jugadores (hilos) intentan unirse a partidas con cupo limitado
//las salas se abren y cierran dinamicamente segun la demanda

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

//igual que en monitor.c, todo monitor lleva: recurso compartido + mutex + var condicion
//aqui el recurso compartido son las salas de juego
typedef struct {
    int id;
    int jugadores_actuales;
    int capacidad;
} DatosSala;

typedef struct {
    DatosSala *salas;
    int num_salas;
    pthread_mutex_t mutex;
    pthread_cond_t hay_lugar; //para despertar a los jugadores que estan esperando sala
} monitorSalas;

//igual que en estacionamiento.c, empaquetamos los datos que necesita cada hilo
typedef struct {
    int id;
    monitorSalas *monitor;
} DatosJugador;

//semaforo global para los lugares totales en todas las salas (igual que estacionamiento.c)
//si todas las salas estan llenas, los jugadores se bloquean aqui antes de siquiera buscar sala
sem_t *lugares_totales;

pthread_mutex_t *impresion; //para que los printf no se mezclen entre hilos

// ─── funciones del monitor ────────────────────────────────────────────────────

void monitor_init(monitorSalas *m, int num_salas, int capacidad){
    m->num_salas = num_salas;
    m->salas = malloc(num_salas * sizeof(DatosSala));
    pthread_mutex_init(&m->mutex, NULL);
    pthread_cond_init(&m->hay_lugar, NULL);
    for(int i = 0; i < num_salas; i++){
        m->salas[i].id = i + 1;
        m->salas[i].jugadores_actuales = 0;
        m->salas[i].capacidad = capacidad;
    }
}

void monitor_destroy(monitorSalas *m){
    pthread_mutex_destroy(&m->mutex);
    pthread_cond_destroy(&m->hay_lugar);
    free(m->salas);
}

//busca una sala con espacio disponible y regresa su indice, o -1 si no hay ninguna
int buscar_sala(monitorSalas *m){
    for(int i = 0; i < m->num_salas; i++){
        if(m->salas[i].jugadores_actuales < m->salas[i].capacidad){
            return i;
        }
    }
    return -1;
}

//el jugador entra a una sala - regresa el indice de la sala en la que entro
int entrar_sala(monitorSalas *m, int id){
    //primero bloqueamos si no hay ningun lugar en ninguna sala (semaforo como en estacionamiento.c)
    sem_wait(lugares_totales);

    //ya sabemos que existe un lugar, ahora buscamos en cual sala esta ese lugar
    pthread_mutex_lock(&m->mutex);

    //usamos while y no if, igual que en monitor.c, para volver a revisar si nos despiertan
    while(buscar_sala(m) == -1){
        pthread_mutex_lock(impresion);
        printf("Jugador %d: esperando que se libere una sala... )=\n", id);
        pthread_mutex_unlock(impresion);
        pthread_cond_wait(&m->hay_lugar, &m->mutex);
    }

    //ya encontramos sala, entramos
    int idx = buscar_sala(m);
    m->salas[idx].jugadores_actuales++;

    pthread_mutex_lock(impresion);
    printf("Jugador %d: entre a la sala %d  (%d/%d jugadores) =D\n",
           id, m->salas[idx].id,
           m->salas[idx].jugadores_actuales,
           m->salas[idx].capacidad);
    pthread_mutex_unlock(impresion);

    pthread_mutex_unlock(&m->mutex);
    return idx;
}

//el jugador sale de la sala y avisa a los que estan esperando
void salir_sala(monitorSalas *m, int id, int idx){
    pthread_mutex_lock(&m->mutex);
    m->salas[idx].jugadores_actuales--;

    pthread_mutex_lock(impresion);
    printf("Jugador %d: me fui de la sala %d  (%d/%d jugadores)\n",
           id, m->salas[idx].id,
           m->salas[idx].jugadores_actuales,
           m->salas[idx].capacidad);
    pthread_mutex_unlock(impresion);

    //si la sala quedo vacia, avisamos que se cierra
    if(m->salas[idx].jugadores_actuales == 0){
        pthread_mutex_lock(impresion);
        printf("--- Sala %d quedo vacia, cerrando sala ---\n", m->salas[idx].id);
        pthread_mutex_unlock(impresion);
    }

    //avisamos a los jugadores en espera que ya hay un lugar libre
    pthread_cond_signal(&m->hay_lugar);
    pthread_mutex_unlock(&m->mutex);

    //liberamos el lugar en el semaforo global (igual que sem_post en estacionamiento.c)
    sem_post(lugares_totales);
}

// ─── funcion que ejecuta cada hilo jugador ────────────────────────────────────

void *jugador(void *args){
    DatosJugador *dato = (DatosJugador *)args;
    int id = dato->id;
    monitorSalas *monitor = dato->monitor;

    pthread_mutex_lock(impresion);
    printf("Jugador %d: quiero unirme a una partida!\n", id);
    pthread_mutex_unlock(impresion);

    int sala = entrar_sala(monitor, id);  //busca sala y entra, se bloquea si no hay lugar
    sleep(1 + rand() % 4);               //simula el tiempo que dura la partida
    salir_sala(monitor, id, sala);        //sale de la sala y avisa a los que esperan

    pthread_mutex_lock(impresion);
    printf("Jugador %d: termine mi partida =)\n", id);
    pthread_mutex_unlock(impresion);

    return NULL;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[]){
    int num_jugadores  = atoi(argv[1]);
    int num_salas      = atoi(argv[2]);
    int capacidad      = atoi(argv[3]); //cuantos jugadores caben en cada sala

    printf("\n%d jugadores intentando entrar a %d salas (capacidad %d c/u)\n\n",
           num_jugadores, num_salas, capacidad);

    //1. inicializamos el mutex de impresion antes de crear cualquier hilo
    impresion = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(impresion, NULL);

    //2. inicializamos el semaforo global con el total de lugares disponibles
    lugares_totales = malloc(sizeof(sem_t));
    sem_init(lugares_totales, 0, num_salas * capacidad);

    //3. inicializamos el monitor con todas las salas
    monitorSalas monitor;
    monitor_init(&monitor, num_salas, capacidad);

    //4. creamos los hilos de los jugadores
    pthread_t *hilos = malloc(num_jugadores * sizeof(pthread_t));
    DatosJugador *datos = malloc(num_jugadores * sizeof(DatosJugador));

    for(int i = 0; i < num_jugadores; i++){
        datos[i].id = i + 1;
        datos[i].monitor = &monitor;
        pthread_create(&hilos[i], NULL, jugador, &datos[i]);
        usleep(300000); //0.3 seg entre jugadores para que la salida sea mas legible
    }

    //5. esperamos a que todos los jugadores terminen
    for(int i = 0; i < num_jugadores; i++){
        pthread_join(hilos[i], NULL);
    }

    //6. limpiamos todo
    monitor_destroy(&monitor);
    sem_destroy(lugares_totales);
    pthread_mutex_destroy(impresion);
    free(lugares_totales);
    free(impresion);
    free(hilos);
    free(datos);

    printf("\nTodos los jugadores terminaron. Servidor apagado.\n\n");
    return 0;
}
