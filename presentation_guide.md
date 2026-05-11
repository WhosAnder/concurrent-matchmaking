# Guia breve de presentacion final (maximo 5 minutos)

## 1) Problema (30-45 segundos)

"Este proyecto simula un servidor de matchmaking de videojuegos donde muchos jugadores llegan al mismo tiempo. Cada jugador es un hilo y cada sala tiene cupo limitado. El reto es que, sin sincronizacion, aparecen race conditions: una sala puede sobrepasar su capacidad y los conteos finales dejan de ser confiables."

## 2) Diseno (1 minuto)

"Implementamos dos versiones: una intencionalmente insegura (`unsync`) y otra segura (`sync`). En `sync`, usamos mutex para proteger secciones criticas, semaforos por sala para controlar cupo y variables de condicion para dormir jugadores cuando no hay espacio, evitando espera activa. Cuando una sala queda vacia, se marca inactiva de forma segura."

## 3) Demo en vivo (2-3 minutos)

1. Mostrar compilacion:

```bash
make clean
make
```

2. Ejecutar unsync y resaltar evidencia:

```bash
make run-unsync
```

Buscar en pantalla/log:

- `[RACE] Sala X excedio capacidad`
- `[ERROR] Conteo inconsistente detectado`

3. Ejecutar sync y mostrar correccion:

```bash
make run-sync
```

Resaltar:

- `[OK] Ninguna sala excedio su capacidad.`
- `[OK] Todos los jugadores terminaron correctamente.`

4. Stress rapido:

```bash
make stress
```

Mencionar que se procesa carga equivalente a 150+ solicitudes en lotes, manteniendo ejecucion ligera.

## 4) Relacion con el curso (1 minuto)

"Aqui conectamos conceptos centrales de Sistemas Operativos: hilos, secciones criticas, planificacion no determinista, race condition, exclusion mutua, semaforos y variables de condicion. Tambien mostramos como prevenir deadlock con orden consistente de locks. La comparacion entre `unsync` y `sync` demuestra por que sincronizar correctamente no es opcional en sistemas concurrentes reales."
