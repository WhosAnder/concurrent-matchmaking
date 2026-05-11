# Reporte tecnico

## Simulacion de un sistema concurrente real: matchmaking de videojuegos con salas concurrentes

## 1. Descripcion del proyecto

Este proyecto implementa una simulacion de matchmaking en C para Linux, donde cada jugador es un hilo y cada sala es un recurso compartido con capacidad limitada. El objetivo academico es observar y explicar, con evidencia reproducible, la diferencia entre un diseno concurrente sin sincronizacion y uno sincronizado.

La arquitectura es **Linux-first**: el comportamiento oficial, validacion y demo final se ejecutan en Linux (nativo, WSL, Docker Linux o VM Linux).

La motivacion es representar un problema real de sistemas: un servidor que recibe solicitudes concurrentes para asignar jugadores a partidas. Este problema contiene de manera natural secciones criticas, contencion, interleavings no deterministas y necesidad de politicas de espera eficientes.

Se desarrollaron dos modulos:

- `src/unsync_matchmaking.c`: diseno intencionalmente inseguro para provocar race conditions.
- `src/sync_matchmaking.c`: diseno robusto con mutex, semaforos y variables de condicion.

Para permitir desarrollo local en macOS sin cambiar el diseno principal, se incluyo un fallback encapsulado con `#ifdef __APPLE__`. Ese fallback no redefine la arquitectura del proyecto y no sustituye la validacion oficial en Linux.

Constantes de configuracion usadas:

- `MAX_PLAYERS=40`
- `MAX_ROOMS=8`
- `ROOM_CAPACITY=5`
- `MAX_WAITING_QUEUE=200`

## 2. Diseno y arquitectura

### 2.1 Componentes

1. **Servidor de matchmaking (proceso principal)**
   - Inicializa salas, jugadores y estructuras compartidas.
   - Crea y sincroniza vida de hilos.
   - Consolida resultados y reportes.

2. **Jugadores (hilos pthread)**
   - Intentan entrar a sala.
   - Esperan si no hay espacio.
   - Simulan juego con `sleep()`.
   - Liberan cupo y notifican al sistema.

3. **Salas (recursos compartidos)**
   - Poseen capacidad maxima.
   - Registran entradas/salidas.
   - En version sync: se activan/inactivan con control seguro.

4. **Cola de espera**
   - Buffer circular para jugadores que no encuentran cupo.
   - En sync se protege con mutex dedicado.

### 2.2 Estructuras y sincronizacion

En `src/common.h` se definen:

- `Player`
- `Room`
- `WaitingQueue`

En version sincronizada:

- `rooms_mutex`: protege lista de salas y ocupacion.
- `players_mutex`: protege contadores globales de jugadores.
- `active_rooms_mutex`: protege contador de salas activas.
- `queue_mutex`: protege cola de espera.
- `sem_t slots` por sala: impone cupo.
- `pthread_cond_t room_available_cond`: bloquea jugadores sin espacio.

Nota de portabilidad:

- Linux/POSIX: ruta principal con `sem_init/sem_trywait/sem_post`.
- macOS local: fallback protegido por compilacion condicional para pruebas del desarrollador.

### 2.3 Politica para evitar deadlock

Se define un **orden global de adquisicion de locks**:

`rooms_mutex -> active_rooms_mutex -> queue_mutex -> players_mutex`

Evitar cambios arbitrarios de orden corta la posibilidad de espera circular (condicion necesaria para deadlock). Ademas, se mantienen secciones criticas cortas y nunca se deja un mutex bloqueado al salir por error en flujo normal.

## 3. Modulos implementados

## 3.1 Modulo 1: version sin sincronizacion (`unsync_matchmaking.c`)

Este modulo usa un patron peligroso:

1. Leer ocupacion (`seen = room.occupancy`).
2. Validar `seen < ROOM_CAPACITY`.
3. Introducir delay aleatorio.
4. Escribir `room.occupancy = seen + 1`.

Como varios hilos pueden ejecutar ese bloque al mismo tiempo, dos o mas jugadores pueden “ver” el mismo valor y luego sobrescribir resultados entre si. Esto provoca:

- sobrecupo de sala,
- perdidas de actualizacion,
- conteos finales inconsistentes.

Se implementaron rondas por jugador para aumentar probabilidad de fallo visible en la misma corrida. El modulo imprime lineas de evidencia como:

- `[RACE] Sala X excedio capacidad`
- `[ERROR] Conteo inconsistente detectado`

## 3.2 Modulo 2: version sincronizada (`sync_matchmaking.c`)

El flujo de cada jugador:

1. Buscar sala activa con cupo (`sem_trywait`).
2. Si no hay cupo, intentar activar nueva sala (si hay margen).
3. Si tampoco hay opcion, entrar a cola y esperar en `pthread_cond_wait`.
4. Al entrar, incrementar ocupacion y contadores bajo mutex.
5. Simular juego (`sleep`).
6. Salir, decrementar ocupacion, `sem_post` para liberar cupo.
7. Notificar disponibilidad con `pthread_cond_broadcast`.

Cuando una sala llega a cero jugadores, se marca inactiva de forma segura (sin liberar memoria dinamica), reduciendo complejidad y riesgo de errores de punteros.

## 4. Experimentos realizados

## 4.1 Escenario base

- Unsync: `./bin/unsync_matchmaking 40 4`
- Sync: `./bin/sync_matchmaking 40 2`

Objetivo:

- observar comportamientos distintos bajo misma carga logica;
- capturar evidencia de no determinismo y consistencia.

## 4.2 Escenario de stress

Para cumplir “150 jugadores rapido en laptop” manteniendo `MAX_PLAYERS=40`, se corre stress en lotes de 40 en la version sincronizada (4 lotes = 160, equivalente o superior al objetivo de 150 solicitudes efectivas).

Esto conserva tiempos de ejecucion razonables y evita aumentar consumo de recursos por encima de hardware modesto.

## 4.3 Instrumentacion y evidencia

Los scripts generan:

- `logs/unsync.log`, `logs/sync.log`, `logs/stress.log`
- `results/unsync_results.txt`, `results/sync_results.txt`, `results/stress_results.txt`

Los resultados filtran mensajes clave (`[RACE]`, `[ERROR]`, `[OK]`, globales).

## 5. Resultados

## 5.1 Resultado esperado en unsync

En ejecuciones tipicas se observan una o ambas condiciones:

1. Sobrecupo temporal en una sala (`6/5`, `7/5`, etc.).
2. Inconsistencia final en conteo global (entradas/salidas/ocupacion).

El orden exacto de eventos cambia entre corridas por el scheduler.

## 5.2 Resultado esperado en sync

La salida valida:

- `[OK] Ninguna sala excedio su capacidad.`
- `[OK] Todos los jugadores terminaron correctamente.`

No deberia haber bloqueos indefinidos ni fuga de hilos en flujo normal.

## 5.3 Comparacion operativa

- Unsync: rapido de escribir, incorrecto en concurrencia real.
- Sync: mayor complejidad, comportamiento correcto y reproducible.

## 6. Interpretacion tecnica

### 6.1 Por que ocurre race condition

Una race condition aparece cuando dos o mas hilos acceden y modifican el mismo estado compartido sin exclusion mutua, y al menos uno escribe. El resultado depende del interleaving exacto de instrucciones. En este proyecto, el contador de ocupacion de sala es el ejemplo directo.

### 6.2 Por que cambia el resultado entre ejecuciones

El sistema operativo usa planificacion no determinista: asigna CPU a hilos segun eventos de tiempo, interrupciones, disponibilidad de nucleos y politicas internas. Como los delays y preemptions varian, cambia el orden de lectura/escritura de datos compartidos.

### 6.3 Que hace el scheduler

El scheduler decide que hilo corre, cuanto tiempo y cuando puede ser interrumpido. En cargas concurrentes, esto crea intercalados de instrucciones imposibles de predecir completamente en user space. Por eso un programa sin sincronizacion puede “parecer bien” una vez y fallar en la siguiente.

### 6.4 Por que mutex protege secciones criticas

Un mutex garantiza exclusion mutua: solo un hilo entra a la seccion critica por vez. Si el estado compartido se lee y se actualiza dentro de ese bloque, se evita la condicion de carrera sobre ese recurso.

### 6.5 Como semaforos controlan cupo

El semaforo contado modela tokens de capacidad. `sem_trywait/sem_wait` consume un token para entrar; `sem_post` lo devuelve al salir. Si no hay tokens, nadie mas entra. Eso implementa cupo maximo con semantica natural de recurso.

### 6.6 Como variables de condicion evitan espera activa

Sin condicion, un hilo puede hacer polling y gastar CPU revisando repetidamente estado. Con `pthread_cond_wait`, el hilo se duerme y libera mutex; se despierta cuando otro hilo hace `signal/broadcast`. Se reduce uso de CPU y mejora comportamiento bajo contencion.

### 6.7 Como se evita deadlock

El diseno aplica orden consistente de locks y evita adquirirlos en secuencia contradictoria. Esta estrategia elimina la condicion de espera circular entre hilos, lo que reduce drasticamente el riesgo de deadlock.

## 7. Limitaciones y trabajo futuro

## 7.1 Limitaciones actuales

- `MAX_PLAYERS=40` por corrida para mantener compatibilidad y ejecucion ligera.
- Simulacion simplificada: no modela latencia de red real, match por ranking o regiones.
- Politicas de fairness basicas (no se mide starvation formalmente).

## 7.2 Trabajo futuro

1. Permitir configuracion runtime de jugadores maximos por corrida (e.g., 200+).
2. Agregar telemetria (tiempo promedio de espera por jugador/sala).
3. Implementar politicas de prioridad y fairness verificables.
4. Integrar exportacion de datos para graficas automatizadas.
5. Evaluar impacto de politicas de scheduler en Linux (SCHED_OTHER/SCHED_FIFO, cuando aplique).

## Conclusiones

El proyecto evidencia de forma practica la diferencia entre concurrencia “funcional” y concurrencia “correcta”. En la version unsync, la ausencia de sincronizacion produce resultados no confiables por race conditions y no determinismo del scheduler. En la version sync, el uso combinado de mutex, semaforos y variables de condicion permite controlar secciones criticas, cupo y espera de manera segura, evitando sobrecupo y terminando consistentemente.
