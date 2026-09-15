#include <windows.h>
#include <stdio.h>

#include "ps2000Con.h"
#include "Muestreo.h"
#include <thread>

// ------------------------------------------------------------
// Identificadores de controles
// ------------------------------------------------------------

#define ID_BOTON_INICIAR   1001
#define ID_BOTON_DETENER   1002
#define ID_ESTADO          1003
#define ID_IMAGEN_A        1004
#define ID_IMAGEN_B        1005

// ------------------------------------------------------------
// Variables globales de la interfaz
// ------------------------------------------------------------

HWND ventanaPrincipal;
HWND textoEstado;

HBITMAP imagenA = NULL;
HBITMAP imagenB = NULL;

bool picoAbierto = false;
std::thread hiloCaptura;

// ------------------------------------------------------------
// Función para cargar una imagen BMP
// ------------------------------------------------------------

HBITMAP cargarImagen(const char* archivo)
{
    return (HBITMAP)LoadImageA(
        NULL,
        archivo,
        IMAGE_BITMAP,
        0,
        0,
        LR_LOADFROMFILE
    );
}

// ------------------------------------------------------------
// Mostrar imágenes
// ------------------------------------------------------------

void mostrarGraficos()
{
    imagenA = cargarImagen("canal_A.bmp");
    imagenB = cargarImagen("canal_B.bmp");

    if (imagenA != NULL)
    {
        SendDlgItemMessage(
            ventanaPrincipal,
            ID_IMAGEN_A,
            STM_SETIMAGE,
            IMAGE_BITMAP,
            (LPARAM)imagenA
        );
    }

    if (imagenB != NULL)
    {
        SendDlgItemMessage(
            ventanaPrincipal,
            ID_IMAGEN_B,
            STM_SETIMAGE,
            IMAGE_BITMAP,
            (LPARAM)imagenB
        );
    }
}

// ------------------------------------------------------------
// Actualizar texto de estado
// ------------------------------------------------------------

void actualizarEstado(const char* texto)
{
    SetWindowTextA(textoEstado, texto);
}

// ------------------------------------------------------------
// Crear controles
// ------------------------------------------------------------

void crearControles(HWND hwnd)
{
    // Botón iniciar
    CreateWindowA(
        "BUTTON",
        "Iniciar captura",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        30,
        30,
        180,
        40,
        hwnd,
        (HMENU)ID_BOTON_INICIAR,
        NULL,
        NULL
    );

    // Botón detener
    CreateWindowA(
        "BUTTON",
        "Detener",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        230,
        30,
        180,
        40,
        hwnd,
        (HMENU)ID_BOTON_DETENER,
        NULL,
        NULL
    );

    // Estado
    textoEstado = CreateWindowA(
        "STATIC",
        "Estado: esperando...",
        WS_VISIBLE | WS_CHILD,
        440,
        40,
        400,
        25,
        hwnd,
        (HMENU)ID_ESTADO,
        NULL,
        NULL
    );

    // Título Canal A
    CreateWindowA(
        "STATIC",
        "Canal A",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        30,
        100,
        540,
        30,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    // Área imagen Canal A
    CreateWindowA(
        "STATIC",
        "",
        WS_VISIBLE | WS_CHILD | SS_BITMAP,
        30,
        135,
        540,
        300,
        hwnd,
        (HMENU)ID_IMAGEN_A,
        NULL,
        NULL
    );

    // Título Canal B
    CreateWindowA(
        "STATIC",
        "Canal B",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        600,
        100,
        540,
        30,
        hwnd,
        NULL,
        NULL,
        NULL
    );

    // Área imagen Canal B
    CreateWindowA(
        "STATIC",
        "",
        WS_VISIBLE | WS_CHILD | SS_BITMAP,
        600,
        135,
        540,
        300,
        hwnd,
        (HMENU)ID_IMAGEN_B,
        NULL,
        NULL
    );
}

// ------------------------------------------------------------
// Procedimiento de la ventana
// ------------------------------------------------------------

LRESULT CALLBACK procedimientoVentana(
    HWND hwnd,
    UINT mensaje,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (mensaje)
    {
    case WM_CREATE:
    {
        crearControles(hwnd);

        // Intentamos abrir el PicoScope
        if (abrir_picoscope())
        {
            picoAbierto = true;
            actualizarEstado("Estado: PicoScope conectado");
        }
        else
        {
            actualizarEstado("Estado: error al abrir PicoScope");
        }

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
            // ------------------------------------------------
            // INICIAR CAPTURA
            // ------------------------------------------------

        case ID_BOTON_INICIAR:
        {
            if (!picoAbierto)
            {
                actualizarEstado(
                    "Estado: PicoScope no conectado"
                );

                return 0;
            }

            actualizarEstado(
                "Estado: capturando datos..."
            );

            // Captura
            captura_activa = 1;

            hiloCaptura = std::thread([]()
                {
                    collect_fast_streaming();
                });

            actualizarEstado(
                "Estado: generando graficos..."
            );

            // Procesamiento + Gnuplot
            generar_Grafico();

            // Mostrar PNG
            mostrarGraficos();

            actualizarEstado(
                "Estado: captura terminada"
            );

            return 0;
        }

        // ------------------------------------------------
        // DETENER
        // ------------------------------------------------

        case ID_BOTON_DETENER:
        {
            if (picoAbierto)
            {

                captura_activa = 0;
            }

            actualizarEstado(
                "Estado: PicoScope detenido"
            );

            return 0;
        }
        }

        break;
    }

    case WM_DESTROY:
    {
        if (picoAbierto)
        {
            cerrar_picoscope();
            picoAbierto = false;
        }

        if (imagenA != NULL)
            DeleteObject(imagenA);

        if (imagenB != NULL)
            DeleteObject(imagenB);

        PostQuitMessage(0);

        return 0;
    }
    }

    return DefWindowProc(
        hwnd,
        mensaje,
        wParam,
        lParam
    );
}

// ------------------------------------------------------------
// Programa principal
// ------------------------------------------------------------

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    const char nombreClase[] = "PicoScope2205A";

    // --------------------------------------------------------
    // Registrar clase de ventana
    // --------------------------------------------------------

    WNDCLASSA wc = {};

    wc.lpfnWndProc = procedimientoVentana;
    wc.hInstance = hInstance;
    wc.lpszClassName = nombreClase;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc))
    {
        MessageBoxA(
            NULL,
            "No se pudo registrar la ventana.",
            "Error",
            MB_ICONERROR
        );

        return 1;
    }

    // --------------------------------------------------------
    // Crear ventana
    // --------------------------------------------------------

    ventanaPrincipal = CreateWindowA(
        nombreClase,
        "PicoScope 2205A - data",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1200,
        550,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (ventanaPrincipal == NULL)
    {
        MessageBoxA(
            NULL,
            "No se pudo crear la ventana.",
            "Error",
            MB_ICONERROR
        );

        return 1;
    }

    // --------------------------------------------------------
    // Mostrar ventana
    // --------------------------------------------------------

    ShowWindow(ventanaPrincipal, nCmdShow);
    UpdateWindow(ventanaPrincipal);

    // --------------------------------------------------------
    // Bucle de mensajes
    // --------------------------------------------------------

    MSG mensaje;

    while (GetMessage(&mensaje, NULL, 0, 0) > 0)
    {
        TranslateMessage(&mensaje);
        DispatchMessage(&mensaje);
    }

    return (int)mensaje.wParam;
}