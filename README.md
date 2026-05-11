# Simulacion de un sistema concurrente real: matchmaking de videojuegos con salas concurrentes

## 1) Descripcion del proyecto

Este proyecto simula un servidor de matchmaking de videojuegos en Linux. El servidor crea jugadores como hilos (`pthread`) que intentan entrar a salas concurrentemente. Cada sala tiene capacidad maxima. Si no hay espacio, los jugadores esperan en cola hasta que otro libere cupo.

Se incluyen dos modulos:

- `src/unsync_matchmaking.c`: version sin sincronizacion para evidenciar race conditions.
- `src/sync_matchmaking.c`: version sincronizada con mutex, semaforos y variables de condicion.

## 2) Conceptos de Sistemas Operativos demostrados

- Hilos y concurrencia real con `pthread_create` y `pthread_join`.
- Secciones criticas sobre estructuras compartidas.
- Race condition en acceso no atomico.
- Exclusion mutua con `pthread_mutex_t`.
- Control de recursos con `sem_t` por sala.
- Espera bloqueante con `pthread_cond_t` (sin busy waiting).
- No determinismo del scheduler del SO.
- Prevencion de deadlock usando orden consistente de locks.

## 3) Dependencias

- Linux (entorno oficial de evaluacion)
- GCC
- Librerias del sistema: `pthread`, `semaphore`, `unistd`, `time`, `stdio`, `stdlib`, `string`
- macOS (solo para desarrollo local con fallback)

## 4) Instrucciones de compilacion

```bash
make clean
make
```

Targets disponibles:

- `make`
- `make unsync`
- `make sync`
- `make clean`
- `make run-unsync`
- `make run-sync`
- `make stress`

## 5) Ejecucion oficial en Linux

```bash
make run-unsync
make run-sync
make stress
```

Ejecucion directa:

```bash
./bin/unsync_matchmaking 40 4
./bin/sync_matchmaking 40 2
```

Validacion oficial solicitada:

```bash
make clean
make
make run-unsync
make run-sync
make stress
```

## 5.1) Ejecucion local en macOS para desarrollo

- El proyecto es Linux-first.
- En `src/sync_matchmaking.c`, el camino principal Linux usa `sem_t` real.
- En macOS se activa automaticamente un fallback local mediante:

```c
#ifdef __APPLE__
// fallback macOS solo para desarrollo local
#else
// implementacion Linux/POSIX principal
#endif
```

- Comandos en macOS:

```bash
make clean
make
make run-unsync
make run-sync
make stress
```

Nota: la demo final y evidencia de entrega deben ejecutarse en Linux/WSL/Docker Linux/VM Linux.

## 6) Explicacion de cada modulo

### Modulo 1: `unsync_matchmaking.c`

- No protege contadores de ocupacion ni contadores globales.
- Usa patron `check-then-act` sin atomia (`if occ < cap` y luego `occ++`).
- Introduce delays aleatorios para amplificar interleavings.
- Resultado esperado: sobrecupo (`[RACE]`) y/o inconsistencia (`[ERROR]`).

### Modulo 2: `sync_matchmaking.c`

- Protege recursos compartidos con mutex:
  - lista de salas
  - contador de jugadores
  - contador de salas activas
  - cola de espera
- Usa `sem_t` por sala para limitar cupo.
- Usa `pthread_cond_t` para dormir hilos cuando no hay lugar.
- Si una sala queda vacia, se marca inactiva de forma segura.
- Resultado esperado: sin sobrecupo, terminacion limpia (`[OK]`).
- Linux usa `sem_init/sem_trywait/sem_post` como ruta oficial.
- macOS usa fallback local solo para pruebas del desarrollador.

## 7) Resultados esperados

### Unsync

Salida tipica:

```text
[PLAYER 12] buscando sala...
[ROOM 1] jugador 12 entro. Ocupacion: 6/5
[RACE] Sala 1 excedio capacidad: 6/5
[ERROR] Conteo inconsistente detectado.
```

### Sync

Salida tipica:

```text
[PLAYER 12] buscando sala...
[ROOM 2] jugador 12 entro. Ocupacion: 3/5
[PLAYER 7] esperando sala disponible...
[ROOM 2] jugador 12 salio. Ocupacion: 2/5
[SIGNAL] espacio disponible, despertando jugador en espera.
[OK] Ninguna sala excedio su capacidad.
[OK] Todos los jugadores terminaron correctamente.
```

## 8) Como interpretar los logs

- `logs/unsync.log`: cronologia completa de eventos no sincronizados.
- `results/unsync_results.txt`: resumen de lineas clave (`[RACE]`, `[ERROR]`, globales).
- `logs/sync.log`: trazas del modulo sincronizado.
- `results/sync_results.txt`: validacion final y consistencia.
- `logs/stress.log` y `results/stress_results.txt`: escenarios de mayor carga.

## 9) Diferencia entre version sin sincronizacion y version sincronizada

- Sin sincronizacion:
  - Intercalado no controlado del scheduler.
  - Actualizaciones perdidas y posibles lecturas obsoletas.
  - Comportamiento incorrecto y variable entre corridas.
- Con sincronizacion:
  - Secciones criticas protegidas.
  - Cupo garantizado por semaforo por sala.
  - Espera eficiente por condicion, sin spin.
  - Estado final consistente.

## 10) Limitaciones y trabajo futuro

- Modelo de juego simple (sin ranking ni MMR real).
- No hay afinidad de CPU ni politicas avanzadas de planificacion.
- Stress de 150 jugadores se implementa por lotes para mantener `MAX_PLAYERS=40` por corrida.

Trabajo futuro:

- Permitir tamano de pool dinamico para 150+ hilos en una sola corrida.
- Agregar metricas de latencia de emparejamiento y throughput.
- Simular prioridad de jugadores y politicas de fairness.
- Exportar trazas en CSV para analisis estadistico.
