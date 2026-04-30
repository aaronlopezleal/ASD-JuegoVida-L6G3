#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cuda_runtime.h>

const int N = 1000;
const int GENERACIONES = 1000;
const int REPETICIONES = 5;
const int SEMILLA = 12345;
const double PROB_VIVA = 0.30;

inline int idx(int i, int j) {
    return i * N + j;
}

void comprobarCuda(cudaError_t error, const char* mensaje) {
    if (error != cudaSuccess) {
        std::cerr << "Error CUDA en " << mensaje << ": "
                  << cudaGetErrorString(error) << std::endl;
        exit(EXIT_FAILURE);
    }
}

void inicializarAleatorio(std::vector<int>& grid) {
    std::mt19937 gen(SEMILLA);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    std::fill(grid.begin(), grid.end(), 0);

    for (int i = 1; i < N - 1; ++i) {
        for (int j = 1; j < N - 1; ++j) {
            grid[idx(i, j)] = (dist(gen) < PROB_VIVA) ? 1 : 0;
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

__global__ void calcularSiguienteGeneracionCUDA(const int* actual, int* siguiente) {
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    int i = blockIdx.y * blockDim.y + threadIdx.y;

    if (i >= N || j >= N) {
        return;
    }

    int posicion = i * N + j;

    // Bordes globales siempre muertos
    if (i == 0 || i == N - 1 || j == 0 || j == N - 1) {
        siguiente[posicion] = 0;
        return;
    }

    int vecinosVivos =
        actual[(i - 1) * N + (j - 1)] + actual[(i - 1) * N + j] + actual[(i - 1) * N + (j + 1)] +
        actual[i * N + (j - 1)]                                      + actual[i * N + (j + 1)] +
        actual[(i + 1) * N + (j - 1)] + actual[(i + 1) * N + j] + actual[(i + 1) * N + (j + 1)];

    int celdaActual = actual[posicion];

    if (celdaActual == 1 && (vecinosVivos == 2 || vecinosVivos == 3)) {
        siguiente[posicion] = 1;
    }
    else if (celdaActual == 0 && vecinosVivos == 3) {
        siguiente[posicion] = 1;
    }
    else {
        siguiente[posicion] = 0;
    }
}

double ejecutarSimulacionCUDA(int& vivasFinales) {
    std::vector<int> grid(N * N, 0);
    inicializarAleatorio(grid);

    int* d_actual = nullptr;
    int* d_siguiente = nullptr;

    size_t bytes = N * N * sizeof(int);

    comprobarCuda(cudaMalloc(&d_actual, bytes), "cudaMalloc d_actual");
    comprobarCuda(cudaMalloc(&d_siguiente, bytes), "cudaMalloc d_siguiente");

    comprobarCuda(cudaMemcpy(d_actual, grid.data(), bytes, cudaMemcpyHostToDevice),
                  "cudaMemcpy host -> device");

    comprobarCuda(cudaMemset(d_siguiente, 0, bytes), "cudaMemset d_siguiente");

    dim3 bloque(16, 16);
    dim3 gridCuda((N + bloque.x - 1) / bloque.x,
                  (N + bloque.y - 1) / bloque.y);

    cudaEvent_t inicio, fin;
    comprobarCuda(cudaEventCreate(&inicio), "cudaEventCreate inicio");
    comprobarCuda(cudaEventCreate(&fin), "cudaEventCreate fin");

    comprobarCuda(cudaEventRecord(inicio), "cudaEventRecord inicio");

    for (int g = 0; g < GENERACIONES; ++g) {
        calcularSiguienteGeneracionCUDA<<<gridCuda, bloque>>>(d_actual, d_siguiente);

        comprobarCuda(cudaGetLastError(), "kernel calcularSiguienteGeneracionCUDA");

        int* temp = d_actual;
        d_actual = d_siguiente;
        d_siguiente = temp;
    }

    comprobarCuda(cudaEventRecord(fin), "cudaEventRecord fin");
    comprobarCuda(cudaEventSynchronize(fin), "cudaEventSynchronize fin");

    float tiempoMs = 0.0f;
    comprobarCuda(cudaEventElapsedTime(&tiempoMs, inicio, fin), "cudaEventElapsedTime");

    comprobarCuda(cudaMemcpy(grid.data(), d_actual, bytes, cudaMemcpyDeviceToHost),
                  "cudaMemcpy device -> host");

    vivasFinales = contarVivas(grid);

    cudaEventDestroy(inicio);
    cudaEventDestroy(fin);
    cudaFree(d_actual);
    cudaFree(d_siguiente);

    return tiempoMs / 1000.0;
}

int main() {
    std::cout << "V6 - CUDA GPU\n";
    std::cout << "N = " << N << "\n";
    std::cout << "Generaciones = " << GENERACIONES << "\n";
    std::cout << "Repeticiones = " << REPETICIONES << "\n";
    std::cout << "Semilla = " << SEMILLA << "\n\n";

    int device = 0;
    cudaDeviceProp prop;
    comprobarCuda(cudaGetDeviceProperties(&prop, device), "cudaGetDeviceProperties");

    std::cout << "GPU usada: " << prop.name << "\n\n";

    double mejorTiempo = 1e9;
    int vivasMejor = 0;

    for (int r = 0; r < REPETICIONES; ++r) {
        int vivasFinales = 0;
        double tiempo = ejecutarSimulacionCUDA(vivasFinales);

        std::cout << "Repeticion " << r + 1 << ": "
                  << tiempo << " s"
                  << " | Celulas vivas finales: " << vivasFinales
                  << "\n";

        if (tiempo < mejorTiempo) {
            mejorTiempo = tiempo;
            vivasMejor = vivasFinales;
        }
    }

    std::cout << "\nMejor tiempo V6 CUDA: " << mejorTiempo << " s\n";
    std::cout << "Celulas vivas finales V6: " << vivasMejor << "\n";

    return 0;
}
