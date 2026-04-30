#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>

const int N = 1000;
const int GENERACIONES = 1000;
const int REPETICIONES = 5;
const int SEMILLA = 12345;

// Probabilidad aproximada de que una celda inicial esté viva
const double PROB_VIVA = 0.30;

// Convertir coordenadas 2D a índice 1D
inline int idx(int i, int j) {
    return i * N + j;
}

void inicializarAleatorio(std::vector<int>& grid) {
    std::mt19937 gen(SEMILLA);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // Inicializamos todo a 0
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

    // Cálculo de celdas interiores
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

    for (int i = 0; i < N * N; ++i) {
        total += grid[i];
    }

    return total;
}

double ejecutarSimulacion(int& vivasFinales) {
    std::vector<int> grid(N * N, 0);
    std::vector<int> nextGrid(N * N, 0);

    inicializarAleatorio(grid);

    auto inicio = std::chrono::high_resolution_clock::now();

    for (int g = 0; g < GENERACIONES; ++g) {
        calcularSiguienteGeneracion(grid, nextGrid);
        std::swap(grid, nextGrid);
    }

    auto fin = std::chrono::high_resolution_clock::now();

    vivasFinales = contarVivas(grid);

    std::chrono::duration<double> tiempo = fin - inicio;
    return tiempo.count();
}

int main() {
    std::cout << "V2 - Secuencial optimizada con memoria contigua usando int\n";
    std::cout << "N = " << N << "\n";
    std::cout << "Generaciones = " << GENERACIONES << "\n";
    std::cout << "Repeticiones = " << REPETICIONES << "\n";
    std::cout << "Semilla = " << SEMILLA << "\n\n";

    double mejorTiempo = 1e9;
    int vivasMejor = 0;

    for (int r = 0; r < REPETICIONES; ++r) {
        int vivasFinales = 0;
        double tiempo = ejecutarSimulacion(vivasFinales);

        std::cout << "Repeticion " << r + 1 << ": "
            << tiempo << " s"
            << " | Celulas vivas finales: " << vivasFinales
            << "\n";

        if (tiempo < mejorTiempo) {
            mejorTiempo = tiempo;
            vivasMejor = vivasFinales;
        }
    }

    std::cout << "\nMejor tiempo V2: " << mejorTiempo << " s\n";
    std::cout << "Celulas vivas finales V2: " << vivasMejor << "\n";

    return 0;
}