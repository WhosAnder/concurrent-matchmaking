# Tabla comparativa

| Version | Problema observado | Mecanismo usado | Resultado esperado | Concepto de SO demostrado |
|---|---|---|---|---|
| Sin sincronizacion (`unsync_matchmaking`) | Sobrecupo (`[RACE]`), ocupacion final inconsistente (`[ERROR]`) | Ninguno en secciones criticas | Fallo no determinista visible en varias corridas | Race condition, seccion critica, planificacion no determinista |
| Con sincronizacion (`sync_matchmaking`) | Contencion por cupo, espera cuando no hay espacio | `pthread_mutex_t` para estado compartido | Sin sobrecupo, conteos consistentes | Mutex, exclusion mutua, secciones criticas |
| Con sincronizacion (`sync_matchmaking`) | Control de capacidad por sala | `sem_t` contado por sala | Nunca excede `ROOM_CAPACITY` | Semaforos contados, administracion de recursos |
| Con sincronizacion (`sync_matchmaking`) | Espera de jugadores sin espacio inmediato | `pthread_cond_t` + `pthread_cond_wait/broadcast` | Espera bloqueante sin busy-wait | Variables de condicion, bloqueo eficiente |
| Con sincronizacion (`sync_matchmaking`) | Riesgo de interbloqueo con multiples locks | Orden fijo de locks: `rooms -> active_rooms -> queue -> players` | Sin deadlock observado | Prevencion de deadlock por orden global |
