<div align="center">

# Arquitectura de Sistemas Distribuidos  
## Optimización y paralelización del Juego de la Vida de Conway

<img src="https://commons.wikimedia.org/wiki/Special:Redirect/file/Game%20of%20life%20animated%20glider.gif" alt="Animated Conway's Game of Life glider" width="120"/>

**Proyecto final - Arquitectura de Sistemas Distribuidos**  
**Grupo L6G3**

**Autores:**  
Aaron López Leal  
Gonzalo Benítez Barquero

</div>

---

## Índice

- [1. Descripción del proyecto](#1-descripción-del-proyecto)
- [2. Objetivos](#2-objetivos)
- [3. Plataforma experimental](#3-plataforma-experimental)
- [4. Problema elegido: Juego de la Vida](#4-problema-elegido-juego-de-la-vida)
- [5. Análisis del código secuencial](#5-análisis-del-código-secuencial)
- [6. Análisis de dependencias](#6-análisis-de-dependencias)
- [7. Ley de Amdahl](#7-ley-de-amdahl)
- [8. Modelo del tejado e intensidad aritmética](#8-modelo-del-tejado-e-intensidad-aritmética)
- [9. Versiones implementadas](#9-versiones-implementadas)
- [10. Resultados locales](#10-resultados-locales)
- [11. Resultados en clúster](#11-resultados-en-clúster)
- [12. Compilación y ejecución](#12-compilación-y-ejecución)
- [13. Estructura del repositorio](#13-estructura-del-repositorio)
- [14. Conclusiones](#14-conclusiones)

---

## 1. Descripción del proyecto

Este repositorio contiene el proyecto final de la asignatura **Arquitectura de Sistemas Distribuidos**.

El trabajo consiste en analizar, optimizar y paralelizar una simulación del **Juego de la Vida de Conway**, comparando distintas implementaciones y evaluando su rendimiento experimentalmente.

El proyecto parte de una versión secuencial sencilla y evoluciona progresivamente hacia versiones optimizadas y paralelas usando distintas técnicas vistas en la asignatura:

- optimización de memoria;
- análisis del paralelismo disponible;
- OpenMP;
- MPI;
- combinación MPI + OpenMP;
- CUDA sobre GPU NVIDIA;
- ejecución en clúster.

El objetivo principal no es únicamente obtener una versión rápida, sino justificar qué tipo de paralelismo se explota en cada caso, medir correctamente el rendimiento y analizar los cuellos de botella de cada enfoque.

---

## 2. Objetivos

Los objetivos principales del proyecto son:

- partir de una implementación secuencial correcta;
- estudiar el comportamiento del código base;
- identificar dependencias y paralelismo disponible;
- mejorar la representación de memoria;
- aplicar paralelismo en memoria compartida mediante OpenMP;
- aplicar paralelismo distribuido mediante MPI;
- combinar MPI y OpenMP en una versión híbrida;
- implementar una versión GPU mediante CUDA;
- validar que todas las versiones producen el mismo resultado;
- comparar tiempos, speedup, eficiencia y escalabilidad;
- ejecutar versiones distribuidas en un clúster real;
- extraer conclusiones sobre los límites de rendimiento de cada técnica.

---

## 3. Plataforma experimental

Para que los experimentos sean replicables, se documenta la plataforma local utilizada para la toma principal de resultados.

### Plataforma local

| Elemento | Valor |
|---|---|
| Sistema operativo | Windows 11 Home 10.0.26200 |
| CPU | Intel Core i5-14600KF |
| Frecuencia reportada | ~3.5 GHz |
| RAM | 32 GB Corsair DDR5 |
| Velocidad RAM | 4800 MHz |
| GPU | NVIDIA GeForce RTX 4060 |
| Compilación C++ | Visual Studio, configuración Release x64 |
| OpenMP | Activado mediante `/openmp` |
| MPI local | Microsoft MPI |
| CUDA | CUDA Toolkit 13.2.1 |

### Plataforma de clúster

También se realizaron pruebas en el clúster de la asignatura.

| Elemento | Valor |
|---|---|
| Entorno | Clúster de la asignatura |
| Grupo | L6G3 |
| Nodos | 3 máquinas virtuales Ubuntu 24.04 |
| Compilador MPI | `mpicxx` |
| Ejecución MPI | `mpirun` con archivo `hostfile` |
| OpenMP en clúster | `-fopenmp`, `OMP_NUM_THREADS=2` |

No se incluyen capturas ni direcciones internas del clúster en el repositorio por privacidad y limpieza del proyecto.

---

## 4. Problema elegido: Juego de la Vida

El **Juego de la Vida de Conway** es un autómata celular bidimensional. Cada celda de una matriz puede estar en uno de dos estados:

- viva, representada como `1`;
- muerta, representada como `0`.

En cada generación, el estado de una celda se actualiza en función del número de vecinas vivas entre sus ocho celdas adyacentes.

Las reglas usadas son las reglas clásicas:

1. Una célula viva con 2 o 3 vecinas vivas sobrevive.
2. Una célula viva con menos de 2 vecinas vivas muere por soledad.
3. Una célula viva con más de 3 vecinas vivas muere por sobrepoblación.
4. Una célula muerta con exactamente 3 vecinas vivas nace.

La simulación se realiza sobre una matriz `N x N` durante un número fijo de generaciones.

---

## 5. Análisis del código secuencial

La versión secuencial base implementa directamente las reglas del Juego de la Vida.

La estructura general del algoritmo es:

```text
Inicializar matriz inicial
Para cada generación:
    Para cada celda interior:
        Contar vecinas vivas
        Aplicar reglas del Juego de la Vida
    Intercambiar matriz actual y matriz siguiente
Contar células vivas finales
```

Para evitar que la actualización de una celda afecte al cálculo de otras celdas dentro de la misma generación, se usa doble buffer:

```text
grid      -> matriz de la generación actual
nextGrid  -> matriz de la generación siguiente
```

Cada celda lee únicamente de `grid` y escribe su nuevo estado en `nextGrid`.

El intercambio entre generaciones se realiza con:

```cpp
std::swap(grid, nextGrid);
```

Esto evita copiar toda la matriz al final de cada generación.

Además, se define explícitamente el tratamiento de bordes:

```text
Los bordes de la matriz permanecen siempre muertos.
```

Esto simplifica el cálculo y evita accesos fuera de los límites de la matriz.

---

## 6. Análisis de dependencias

El Juego de la Vida tiene una dependencia temporal clara:

```text
Generación g  ->  Generación g + 1
```

Esto significa que no se puede calcular la generación `g + 1` sin haber completado previamente la generación `g`.

Sin embargo, dentro de una misma generación, las celdas son independientes entre sí si se usa doble buffer.

El cálculo de una celda puede representarse así:

```text
Generación g:

(i-1,j-1)   (i-1,j)   (i-1,j+1)
(i,  j-1)   (i,  j)   (i,  j+1)
(i+1,j-1)   (i+1,j)   (i+1,j+1)

        ↓

Generación g+1:

          (i,j)
```

La nueva celda `(i,j)` en la generación siguiente depende de las ocho vecinas y del estado anterior de la celda en la generación actual.

Por tanto:

- no se pueden paralelizar libremente las generaciones;
- sí se puede paralelizar el cálculo de las celdas dentro de una misma generación;
- el uso de doble buffer elimina condiciones de carrera;
- el patrón de cómputo es regular y adecuado para paralelismo de datos.

### Tipos de paralelismo explotados

| Tipo | Aplicación en el proyecto |
|---|---|
| ILP | Operaciones independientes dentro del cálculo de vecinos. |
| DLP | Mismo cálculo aplicado a muchas celdas de la matriz. |
| TLP | Reparto de filas entre hilos mediante OpenMP. |
| Paralelismo distribuido | Reparto de bloques de filas entre procesos MPI. |
| Híbrido | MPI entre procesos y OpenMP dentro de cada proceso. |
| GPU | Un hilo CUDA calcula una celda de la matriz. |

---

## 7. Ley de Amdahl

La Ley de Amdahl permite razonar sobre el límite teórico de aceleración cuando solo una parte del programa puede paralelizarse.

La fórmula general es:

```text
Speedup máximo = 1 / ((1 - P) + P / n)
```

donde:

- `P` es la fracción paralelizable del programa;
- `n` es el número de unidades de ejecución;
- `1 - P` es la parte secuencial que no puede paralelizarse.

En este proyecto, la parte más costosa es el cálculo de las celdas en cada generación, que es altamente paralelizable. Sin embargo, existen partes que limitan la aceleración máxima:

- inicialización de la matriz;
- conteo final de células vivas;
- sincronización entre generaciones;
- reducción de resultados;
- comunicación entre procesos MPI;
- intercambio de filas fantasma;
- lanzamiento de kernels CUDA;
- copias entre CPU y GPU.

Esto explica por qué la eficiencia disminuye al aumentar el número de hilos o procesos. Aunque haya mucho paralelismo disponible, siempre existen costes no paralelizables o de coordinación.

---

## 8. Modelo del tejado e intensidad aritmética

El modelo del tejado, o *Roofline Model*, relaciona el rendimiento de un programa con dos límites principales:

- capacidad de cálculo del procesador;
- ancho de banda de memoria.

Una magnitud importante es la **intensidad aritmética**:

```text
IA = operaciones aritméticas / bytes transferidos
```

En el Juego de la Vida, para calcular una celda se realizan pocas operaciones aritméticas, pero se accede a varios datos de memoria:

- lectura de 8 vecinas;
- lectura de la celda actual;
- escritura de la nueva celda.

Usando enteros de 4 bytes, una estimación simple es:

```text
Lecturas: 9 enteros  ->  36 bytes
Escrituras: 1 entero ->   4 bytes
Total aproximado     ->  40 bytes por celda
```

El número de operaciones por celda es reducido: sumas para contar vecinos y comparaciones para aplicar las reglas.

Por tanto, el algoritmo tiene una intensidad aritmética relativamente baja. Esto significa que su rendimiento puede estar muy condicionado por:

- localidad de memoria;
- eficiencia de caché;
- ancho de banda de memoria;
- coste de comunicación en MPI;
- transferencias CPU-GPU en CUDA.

Esta observación justifica la mejora de V2 al usar memoria contigua y también explica por qué añadir más hilos o procesos no siempre produce una aceleración lineal.

---

## 9. Versiones implementadas

| Versión | Técnica | Descripción |
|---|---|---|
| **V1** | Secuencial base | Implementación inicial con `std::vector<std::vector<int>>`. |
| **V2** | Optimización secuencial | Uso de memoria contigua mediante `std::vector<int>`. |
| **V3** | OpenMP | Paralelización del cálculo de filas con hilos. |
| **V4** | MPI | División de la matriz por filas entre procesos. |
| **V5** | MPI + OpenMP | Versión híbrida con procesos MPI e hilos OpenMP. |
| **V6** | CUDA | Versión GPU usando una NVIDIA GeForce RTX 4060. |

### V1 - Secuencial base

La primera versión implementa el algoritmo de forma secuencial usando:

```cpp
std::vector<std::vector<int>>
```

Se emplean dos matrices:

- una para la generación actual;
- otra para la generación siguiente.

El intercambio se realiza mediante:

```cpp
std::swap(grid, nextGrid);
```

Esto evita copiar toda la matriz en cada generación.

### V2 - Memoria contigua

En la segunda versión se cambia la representación de la matriz a un único vector lineal:

```cpp
std::vector<int> grid(N * N);
```

El acceso a la celda `(i, j)` se realiza mediante:

```cpp
grid[i * N + j]
```

Esta modificación mejora:

- la localidad espacial;
- el acceso secuencial por filas;
- la eficiencia de caché;
- la base sobre la que paralelizar posteriormente.

### V3 - OpenMP

La versión OpenMP paraleliza el bucle exterior que recorre las filas interiores:

```cpp
#pragma omp parallel for schedule(static)
```

Cada hilo calcula un subconjunto de filas. Como todos los hilos leen de `grid` y escriben en posiciones distintas de `nextGrid`, la paralelización es segura.

Se probaron ejecuciones con:

```text
1, 2, 4 y 8 hilos
```

### V4 - MPI

La versión MPI reparte la matriz por bloques de filas entre procesos.

Cada proceso mantiene:

- sus filas locales;
- una fila fantasma superior;
- una fila fantasma inferior.

En cada generación, los procesos vecinos intercambian sus filas frontera mediante:

```cpp
MPI_Sendrecv
```

Al finalizar la simulación, el número total de células vivas se obtiene mediante:

```cpp
MPI_Reduce
```

El tiempo medido en MPI corresponde al máximo tiempo entre procesos, ya que el tiempo total de la simulación distribuida queda determinado por el proceso más lento.

### V5 - MPI + OpenMP

La versión híbrida combina las dos estrategias anteriores:

- MPI divide la matriz entre procesos.
- OpenMP paraleliza el cálculo local dentro de cada proceso.

En las pruebas locales se usaron 2 hilos OpenMP por proceso MPI.

Esta versión permite estudiar la combinación de paralelismo distribuido y paralelismo en memoria compartida.

### V6 - CUDA

La versión CUDA ejecuta la simulación en GPU. Cada hilo CUDA calcula una celda de la matriz.

La configuración usada fue:

```text
GPU: NVIDIA GeForce RTX 4060
Bloques CUDA: 16 x 16 hilos
```

La dependencia entre generaciones se mantiene, pero dentro de cada generación se explota paralelismo masivo de datos.

---

## 10. Resultados locales

Para comparar las versiones de forma justa, se usaron los mismos parámetros principales:

```cpp
const int N = 1000;
const int GENERACIONES = 1000;
const int REPETICIONES = 5;
const int SEMILLA = 12345;
const double PROB_VIVA = 0.30;
```

Todas las versiones se validaron comprobando el mismo resultado final:

```text
Células vivas finales = 43306
```

Resultados obtenidos en ejecución local:

| Versión | Configuración | Mejor tiempo | Células vivas | Speedup vs V1 |
|---|---:|---:|---:|---:|
| **V1** | Secuencial base | 1.92701 s | 43306 | 1.00x |
| **V2** | Secuencial optimizada | 1.56022 s | 43306 | 1.24x |
| **V3** | OpenMP, 8 hilos | 0.307451 s | 43306 | 6.27x |
| **V4** | MPI, 8 procesos | 0.311311 s | 43306 | 6.19x |
| **V5** | MPI + OpenMP | 0.186780 s | 43306 | 10.32x |
| **V6** | CUDA, RTX 4060 | 0.0181502 s | 43306 | 106.17x |

### Evolución del rendimiento

```text
V1  Secuencial base        1.92701 s
V2  Memoria contigua       1.56022 s
V3  OpenMP                 0.307451 s
V4  MPI                    0.311311 s
V5  MPI + OpenMP           0.186780 s
V6  CUDA                   0.0181502 s
```

La versión más rápida en local fue V6 CUDA, con una aceleración aproximada de:

```text
Speedup V6 = 1.92701 / 0.0181502 = 106.17x
```

---

## 11. Resultados en clúster

También se realizaron pruebas en el clúster de la asignatura usando tres máquinas virtuales Ubuntu asignadas al grupo.

No se incluyen capturas ni direcciones internas en el repositorio por privacidad y limpieza del proyecto.

Resultados obtenidos:

| Versión | Configuración | Mejor tiempo | Células vivas |
|---|---:|---:|---:|
| **V4 MPI** | 4 procesos MPI | 1.01538 s | 43306 |
| **V5 MPI + OpenMP** | 4 procesos MPI x 2 hilos | 4.02544 s | 43306 |

La versión híbrida no mejoró a la versión MPI pura en el clúster. Esto se explica por:

- recursos limitados en máquinas virtuales;
- sobrecarga de planificación de hilos;
- competencia por CPU y memoria;
- coste de coordinación entre procesos e hilos.

Aun así, ambas versiones se ejecutaron correctamente en un entorno distribuido real y produjeron el mismo resultado final que las versiones locales.

---

## 12. Compilación y ejecución

### V1 y V2

En Linux:

```bash
g++ V1_secuencial_base.cpp -O2 -o v1
./v1

g++ V2_memoria_contigua.cpp -O2 -o v2
./v2
```

En Windows se compilaron como proyectos C++ en Visual Studio en configuración:

```text
Release | x64
```

### V3 - OpenMP

En Linux:

```bash
g++ V3_openmp.cpp -O2 -fopenmp -o v3_openmp
./v3_openmp
```

En Visual Studio es necesario activar:

```text
Propiedades del proyecto
C/C++
Lenguaje
Compatibilidad con OpenMP: Sí (/openmp)
```

### V4 - MPI

En Linux:

```bash
mpicxx V4_mpi.cpp -O2 -o v4_mpi
mpirun -np 4 ./v4_mpi
```

En Windows con MS-MPI:

```cmd
mpiexec -n 4 ASD_JuegoVida_V4_MPI.exe
```

Para ejecutar con 8 procesos:

```cmd
mpiexec -n 8 ASD_JuegoVida_V4_MPI.exe
```

### V5 - MPI + OpenMP

En Linux:

```bash
mpicxx V5_hibrida_mpi_openmp.cpp -O2 -fopenmp -o v5_hibrida
OMP_NUM_THREADS=2 mpirun -np 4 ./v5_hibrida
```

En Windows con MS-MPI y OpenMP activado:

```cmd
mpiexec -n 4 ASD_JuegoVida_V5_Hibrida.exe
```

### V6 - CUDA

Compilar desde **x64 Native Tools Command Prompt for Visual Studio**:

```cmd
nvcc V6_cuda.cu -O2 -o v6_cuda.exe
v6_cuda.exe
```

---

## 13. Estructura del repositorio

```text
ASD-JuegoVida-L6G3/
├── README.md
├── .gitignore
└── Codigo/
    ├── V1_secuencial_base.cpp
    ├── V2_memoria_contigua.cpp
    ├── V3_openmp.cpp
    ├── V4_mpi.cpp
    ├── V5_hibrida_mpi_openmp.cpp
    └── V6_cuda.cu
```

---

## 14. Conclusiones

La evolución del proyecto permite observar el impacto de distintas técnicas de optimización y paralelización sobre un mismo problema computacional.

Conclusiones principales:

- La optimización de memoria de V2 reduce el tiempo respecto a la versión secuencial base.
- OpenMP consigue una mejora importante al repartir filas entre hilos.
- MPI permite distribuir el cálculo entre procesos independientes.
- La versión híbrida MPI + OpenMP obtiene el mejor rendimiento CPU local.
- CUDA proporciona la mayor aceleración gracias al paralelismo masivo de la GPU.
- La ejecución en clúster valida el funcionamiento distribuido real.
- Más paralelismo no siempre implica menor tiempo: el rendimiento depende de la plataforma, la memoria, la comunicación y la sobrecarga de coordinación.

El proyecto muestra cómo un problema sencillo puede servir para estudiar de forma clara los principales niveles de paralelismo vistos en la asignatura: memoria, hilos, procesos, ejecución híbrida, clúster y GPU.