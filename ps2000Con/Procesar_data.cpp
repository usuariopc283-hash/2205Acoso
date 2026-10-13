#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string>
#include "Procesar_data.h"

#define DATA_FILE "data.txt"
#define DESCARTAR_PRIMERAS_MUESTRAS 1   // cantidad de filas de datos a eliminar

#define SAMPLE_INTERVAL_MS 0.01
int muestras = 0;
int descartadas = 0;
double tiempo = 0.0;

void Procesar_data()
{
    FILE* data;
    FILE* canalA;
    FILE* canalB;

    double channelA;
    double channelB;

    char line[256];

    int muestras = 0;
    double tiempo = 0.0;

    data = fopen(DATA_FILE, "r");

    if (data == NULL)
    {
        printf("Error: no se pudo abrir %s\n", DATA_FILE);
        return;
    }

    canalA = fopen("data_canal_A.txt", "w");
    canalB = fopen("data_canal_B.txt", "w");

    if (canalA == NULL || canalB == NULL)
    {
        printf("Error: no se pudieron crear los archivos de los canales.\n");

        fclose(data);

        if (canalA != NULL)
            fclose(canalA);

        if (canalB != NULL)
            fclose(canalB);

        return;
    }

    /*
        Leer data.txt línea por línea.

        Formato original:

        Canal A, Canal B,
        -178, 25,
        112, 25,
        112, 25,
        ...
    */

    while (fgets(line, sizeof(line), data) != NULL)
    {
        if (sscanf(line, " %lf , %lf", &channelA, &channelB) == 2)
        {
            // Saltar las primeras filas de datos
            if (descartadas < DESCARTAR_PRIMERAS_MUESTRAS)
            {
                descartadas++;
                continue;
            }

            fprintf(canalA, "%.6f %.6f\n", tiempo, channelA);
            fprintf(canalB, "%.6f %.6f\n", tiempo, channelB);

            tiempo += SAMPLE_INTERVAL_MS;
            muestras++;
        }
    }

    fclose(data);
    fclose(canalA);
    fclose(canalB);

    printf("Muestras encontradas: %d\n", muestras);

    printf("Intervalo de muestreo: %.6f ms\n",
        SAMPLE_INTERVAL_MS);

    printf("Frecuencia de muestreo: %.2f kHz\n",
        1.0 / SAMPLE_INTERVAL_MS);

    if (muestras > 0)
    {
        printf("Tiempo de la ultima muestra: %.6f ms\n",
            (muestras - 1) * SAMPLE_INTERVAL_MS);
    }

    printf("\nArchivos generados:\n");
    printf("  data_canal_A.txt\n");
    printf("  data_canal_B.txt\n");

    return;
}