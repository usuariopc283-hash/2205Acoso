#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>

#define DATA_FILE "..\\..\\..\\ps2000Con\\data.txt"

#define SAMPLE_INTERVAL_MS 0.01

int main()
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
        return 1;
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

        return 1;
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
            /*
                Escribir:

                tiempo    valor

                Ejemplo:

                0.00      -178
                0.01       112
                0.02       112
            */

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

    printf("\nGenerando graficos con Gnuplot...\n");

    int resultado = system("gnuplot plot_commands.gp");

    printf("Gnuplot termino con codigo: %d\n", resultado);

    return 0;
}