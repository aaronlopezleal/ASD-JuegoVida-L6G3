#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <omp.h>

const int N = 1000;
const int GENERACIONES = 1000;
const int REPETICIONES = 5;
const int SEMILLA = 12345;
const double PROB_VIVA = 0.30;

inline int idx(int i, int j) {
    return i * N + j;
}

void inicializarAleatorio(std::vector<int>& grid) {
    std::mt19937 gen(SEMILLA);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    std::fill(grid.begin(), grid.end(), 0);

    // Solo inicializamos el interior. Los bordes quedan a 0.
    for (int i = 1; i < N - 1; ++i) {
        for (int j = 1; j < N - 1; ++j) {
            grid[idx(i, j)] = (dist(gen) < PROB_VIVA) ? 1 : 0;
        }
    }
}

void calcularSiguienteGeneracion(
    const std::vector<int>& actual,
    std::vector<int>& siguiente
) {
    // Bordes fijos a 0
    for (int i = 0; i < N; ++i) {
        siguiente[idx(i, 0)] = 0;
        siguiente[idx(i, N - 1)] = 0;
        siguiente[idx(0, i)] = 0;
        siguiente[idx(N - 1, i)] = 0;
    }

#pragma omp parallel for schedule(static)
    for (int i = 1; i < N - 1; ++i) {
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

int contarVivas(const std::vector<int>& grid) {
    int total = 0;

#pragma omp parallel for reduction(+:total) schedule(static)
    for (int i = 0; i < N * N; ++i) {
        total += grid[i];
    }

    return total;
}

double ejecutarSimulacion(int numThreads, int& vivasFinales) {
    omp_set_num_threads(numThreads);

    std::vector<int> grid(N * N, 0);
    std::vector<int> nextGrid(N * N, 0);

    inicializarAleatorio(grid);

    double inicio = omp_get_wtime();

    for (int g = 0; g < GENERACIONES; ++g) {
        calcularSiguienteGeneracion(grid, nextGrid);
        std::swap(grid, nextGrid);
    }

    double fin = omp_get_wtime();

    vivasFinales = contarVivas(grid);

    return fin - inicio;
}

int main() {
    std::cout << "V3 - OpenMP sobre memoria contigua\n";
    std::cout << "N = " << N << "\n";
    std::cout << "Generaciones = " << GENERACIONES << "\n";
    std::cout << "Repeticiones = " << REPETICIONES << "\n";
    std::cout << "Semilla = " << SEMILLA << "\n\n";

    // Comprobacion inicial de que OpenMP crea realmente varios hilos
    std::cout << "Comprobacion de OpenMP:\n";

    for (int pruebaHilos : {1, 2, 4, 8}) {
        omp_set_num_threads(pruebaHilos);

#pragma omp parallel
        {
#pragma omp single
            {
                std::cout << "Solicitados " << pruebaHilos
                    << " hilos, OpenMP ha creado "
                    << omp_get_num_threads()
                    << " hilos reales.\n";
            }
        }
    }

    std::cout << "\n";

    int hilos[] = { 1, 2, 4, 8 };

    for (int h = 0; h < 4; ++h) {
        int numThreads = hilos[h];

        double mejorTiempo = 1e9;
        int vivasMejor = 0;

        std::cout << "Prueba con " << numThreads << " hilo(s)\n";

        for (int r = 0; r < REPETICIONES; ++r) {
            int vivasFinales = 0;
            double tiempo = ejecutarSimulacion(numThreads, vivasFinales);

            std::cout << "  Repeticion " << r + 1 << ": "
                << tiempo << " s"
                << " | Celulas vivas finales: " << vivasFinales
                << "\n";

            if (tiempo < mejorTiempo) {
                mejorTiempo = tiempo;
                vivasMejor = vivasFinales;
            }
        }

        std::cout << "  Mejor tiempo con " << numThreads << " hilo(s): "
            << mejorTiempo << " s\n";
        std::cout << "  Celulas vivas finales: " << vivasMejor << "\n\n";
    }

    return 0;
}