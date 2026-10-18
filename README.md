# Servidor de simulaciones eletromagneticas FDTD concurrente

CI-0117 Programacion Paralela y Concurrente, UCR.
Primer Proyecto II Ciclo 2026.

## Integrantes

- Andres Oreamuno Varela
- Andres Villegas Baltodano
- Emanuel Gonzalez Chaves

## Instrucciones

- Instalar las dependencias requeridas (`sudo apt install build-essential`).

- Para compilar el proyecto:

```bash
make clean
```
```bash
make
```

- Para ejecutar el servidor:

```bash
./bin/fdtd_server <puerto>
```

Ejemplo:

```bash
./bin/fdtd_server 8080
```

El servidor se detiene con Ctrl+C.

- Para ejecutar el cliente:

```bash
./bin/fdtd_client <ip> <puerto> <comando> [parámetros] 
```
Comandos

SUBMIT nx ny steps threads k sx sy

-nx: cantidad de filas de la matriz
-ny: cantidad de columnas de la matriz 
-steps: cantidad de iteraciones de la simulación
-threads: cantidad de hilos solucionadores
-k: cada cuantas iteraciones se guarda una instantántea de Ez 
-sx: fila de la fuente
-sy: columna de la fuente

STATUS <id>
PEEK <id> <i> <j>
RESULT <id>
SHUTDOWN

Ejemplo:

```bash
./bin/fdtd_client 127.0.0.1 8080 SUBMIT 100 100 500 4 10 50 50
```
