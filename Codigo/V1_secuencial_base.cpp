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

void inicializarAleatorio(std::vector<std::vector<int>>& grid) {
    std::mt19937 gen(SEMILLA);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // Inicializamos todo a 0
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            grid[i][j] = 0;
        }
    }

    // Solo inicializamos el interior. Los bordes quedan a 0.
    for (int i = 1; i < N - 1; ++i) {
        for (int j = 1; j < N - 1; ++j) {
            grid[i][j] = (dist(gen) < PROB_VIVA) ? 1 : 0;
        }
    }
}

void calcularSiguienteGeneracion(
    const std::vector<std::vector<int>>& actual,
    std::vector<std::vector<int>>& siguiente
) {
    // Bordes fijos a 0
    for (int i = 0; i < N; ++i) {
        siguiente[i][0] = 0;
        siguiente[i][N - 1] = 0;
        siguiente[0][i] = 0;
        siguiente[N - 1][i] = 0;
    }

    // Cálculo de celdas interiores
    for (int i = 1; i < N - 1; ++i) {
        for (int j = 1; j < N - 1; ++j) {
            int vecinosVivos =
                actual[i - 1][j - 1] + actual[i - 1][j] + actual[i - 1][j + 1] +
                actual[i][j - 1] + actual[i][j + 1] +
                actual[i + 1][j - 1] + actual[i + 1][j] + actual[i + 1][j + 1];

            if (actual[i][j] == 1 && (vecinosVivos == 2 || vecinosVivos == 3)) {
                siguiente[i][j] = 1;
            }
            else if (actual[i][j] == 0 && vecinosVivos == 3) {
                siguiente[i][j] = 1;
            }
            else {
                siguiente[i][j] = 0;
            }
        }
    }
}

int contarVivas(const std::vector<std::vector<int>>& grid) {
    int total = 0;

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            total += grid[i][j];
        }
    }

    return total;
}

double ejecutarSimulacion(int& vivasFinales) {
    std::vector<std::vector<int>> grid(N, std::vector<int>(N, 0));
    std::vector<std::vector<int>> nextGrid(N, std::vector<int>(N, 0));

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
    std::cout << "V1 - Secuencial base con vector<vector<int>>\n";
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

    std::cout << "\nMejor tiempo V1: " << mejorTiempo << " s\n";
    std::cout << "Celulas vivas finales V1: " << vivasMejor << "\n";

    return 0;
}