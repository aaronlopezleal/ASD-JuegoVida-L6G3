#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <mpi.h>
#include <omp.h>

const int N = 1000;
const int GENERACIONES = 1000;
const int REPETICIONES = 5;
const int SEMILLA = 12345;
const double PROB_VIVA = 0.30;

// Hilos OpenMP usados dentro de cada proceso MPI
const int THREADS_POR_PROCESO = 2;

inline int idxGlobal(int i, int j) {
    return i * N + j;
}

inline int idxLocal(int i, int j) {
    return i * N + j;
}

void inicializarAleatorioGlobal(std::vector<int>& grid) {
    std::mt19937 gen(SEMILLA);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    std::fill(grid.begin(), grid.end(), 0);

    // Bordes globales a 0. Solo inicializamos el interior.
    for (int i = 1; i < N - 1; ++i) {
        for (int j = 1; j < N - 1; ++j) {
            grid[idxGlobal(i, j)] = (dist(gen) < PROB_VIVA) ? 1 : 0;
        }
    }
}

void intercambiarFilasFantasma(std::vector<int>& gridLocal, int filasLocales, int rank, int size) {
    if (rank > 0) {
        MPI_Sendrecv(
            &gridLocal[idxLocal(1, 0)], N, MPI_INT, rank - 1, 0,
            &gridLocal[idxLocal(0, 0)], N, MPI_INT, rank - 1, 1,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );
    }
    else {
        std::fill(
            gridLocal.begin() + idxLocal(0, 0),
            gridLocal.begin() + idxLocal(1, 0),
            0
        );
    }

    if (rank < size - 1) {
        MPI_Sendrecv(
            &gridLocal[idxLocal(filasLocales, 0)], N, MPI_INT, rank + 1, 1,
            &gridLocal[idxLocal(filasLocales + 1, 0)], N, MPI_INT, rank + 1, 0,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE
        );
    }
    else {
        std::fill(
            gridLocal.begin() + idxLocal(filasLocales + 1, 0),
            gridLocal.begin() + idxLocal(filasLocales + 2, 0),
            0
        );
    }
}

void calcularSiguienteGeneracionLocalHibrida(
    const std::vector<int>& actual,
    std::vector<int>& siguiente,
    int filasLocales,
    int rank,
    int size
) {
    // Limpiamos siguiente para mantener bordes a 0
    std::fill(siguiente.begin(), siguiente.end(), 0);

    int primeraFila = 1;
    int ultimaFila = filasLocales;

    // Primer proceso: su primera fila real corresponde al borde global superior
    if (rank == 0) {
        primeraFila = 2;
    }

    // Último proceso: su última fila real corresponde al borde global inferior
    if (rank == size - 1) {
        ultimaFila = filasLocales - 1;
    }

#pragma omp parallel for schedule(static)
    for (int i = primeraFila; i <= ultimaFila; ++i) {
        int filaAnterior = (i - 1) * N;
        int filaActual = i * N;
        int filaSiguiente = (i + 1) * N;

        for (int j = 1; j < N - 1; ++j) {
            int vecinosVivos =
                actual[filaAnterior + j - 1] + actual[filaAnterior + j] + actual[filaAnterior + j + 1] +
                actual[filaActual + j - 1] + actual[filaActual + j + 1] +
                actual[filaSiguiente + j - 1] + actual[filaSiguiente + j] + actual[filaSiguiente + j + 1];

            int celdaActual = actual[filaActual + j];

            if (celdaActual == 1 && (vecinosVivos == 2 || vecinosVivos == 3)) {
                siguiente[filaActual + j] = 1;
            }
            else if (celdaActual == 0 && vecinosVivos == 3) {
                siguiente[filaActual + j] = 1;
            }
            else {
                siguiente[filaActual + j] = 0;
            }
        }
    }
}

int contarVivasLocal(const std::vector<int>& gridLocal, int filasLocales) {
    int total = 0;

#pragma omp parallel for reduction(+:total) schedule(static)
    for (int i = 1; i <= filasLocales; ++i) {
        int sumaFila = 0;
        for (int j = 0; j < N; ++j) {
            sumaFila += gridLocal[idxLocal(i, j)];
        }
        total += sumaFila;
    }

    return total;
}

double ejecutarSimulacionHibrida(int rank, int size, int& vivasTotales) {
    int filasLocales = N / size;

    std::vector<int> gridGlobal;

    if (rank == 0) {
        gridGlobal.resize(N * N, 0);
        inicializarAleatorioGlobal(gridGlobal);
    }

    std::vector<int> recvBuffer(filasLocales * N, 0);

    MPI_Scatter(
        rank == 0 ? gridGlobal.data() : nullptr,
        filasLocales * N,
        MPI_INT,
        recvBuffer.data(),
        filasLocales * N,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    std::vector<int> gridLocal((filasLocales + 2) * N, 0);
    std::vector<int> nextLocal((filasLocales + 2) * N, 0);

    // Copiamos las filas recibidas dejando fila 0 y fila filasLocales+1 como fantasmas
    for (int i = 0; i < filasLocales; ++i) {
        for (int j = 0; j < N; ++j) {
            gridLocal[idxLocal(i + 1, j)] = recvBuffer[i * N + j];
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio = MPI_Wtime();

    for (int g = 0; g < GENERACIONES; ++g) {
        intercambiarFilasFantasma(gridLocal, filasLocales, rank, size);
        calcularSiguienteGeneracionLocalHibrida(gridLocal, nextLocal, filasLocales, rank, size);
        std::swap(gridLocal, nextLocal);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double fin = MPI_Wtime();

    int vivasLocales = contarVivasLocal(gridLocal, filasLocales);
    vivasTotales = 0;

    MPI_Reduce(
        &vivasLocales,
        &vivasTotales,
        1,
        MPI_INT,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );

    double tiempoLocal = fin - inicio;
    double tiempoMaximo = 0.0;

    MPI_Reduce(
        &tiempoLocal,
        &tiempoMaximo,
        1,
        MPI_DOUBLE,
        MPI_MAX,
        0,
        MPI_COMM_WORLD
    );

    return tiempoMaximo;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    omp_set_num_threads(THREADS_POR_PROCESO);

    if (N % size != 0) {
        if (rank == 0) {
            std::cout << "Error: N debe ser divisible entre el numero de procesos.\n";
            std::cout << "N = " << N << ", procesos = " << size << "\n";
        }

        MPI_Finalize();
        return 1;
    }

    if (rank == 0) {
        std::cout << "V5 - Hibrida MPI + OpenMP\n";
        std::cout << "N = " << N << "\n";
        std::cout << "Generaciones = " << GENERACIONES << "\n";
        std::cout << "Repeticiones = " << REPETICIONES << "\n";
        std::cout << "Semilla = " << SEMILLA << "\n";
        std::cout << "Procesos MPI = " << size << "\n";
        std::cout << "Hilos OpenMP por proceso = " << THREADS_POR_PROCESO << "\n";
        std::cout << "Unidades teoricas totales = " << size * THREADS_POR_PROCESO << "\n\n";
    }

    double mejorTiempo = 1e9;
    int vivasMejor = 0;

    for (int r = 0; r < REPETICIONES; ++r) {
        int vivasTotales = 0;
        double tiempo = ejecutarSimulacionHibrida(rank, size, vivasTotales);

        if (rank == 0) {
            std::cout << "Repeticion " << r + 1 << ": "
                << tiempo << " s"
                << " | Celulas vivas finales: " << vivasTotales
                << "\n";

            if (tiempo < mejorTiempo) {
                mejorTiempo = tiempo;
                vivasMejor = vivasTotales;
            }
        }
    }

    if (rank == 0) {
        std::cout << "\nMejor tiempo V5 hibrida: " << mejorTiempo << " s\n";
        std::cout << "Celulas vivas finales V5: " << vivasMejor << "\n";
    }

    MPI_Finalize();
    return 0;
}