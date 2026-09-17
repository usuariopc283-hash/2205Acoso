#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <gdiplus.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <string>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <system_error>
#include <fstream>
#include <vector>
#include <utility>
#include <iomanip>
#include <cstdio>

#include "ps2000Con.h"
#include "Muestreo.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;
namespace fs = std::filesystem;

// ------------------------------------------------------------
// Identificadores
// ------------------------------------------------------------

#define ID_BOTON_INICIAR 1001
#define ID_BOTON_CARGAR  1002
#define ID_ESTADO        1003
#define ID_BOTON_GUARDAR 1006

#define WM_CAPTURA_TERMINADA (WM_APP + 1)

// ------------------------------------------------------------
// Variables globales
// ------------------------------------------------------------

HWND ventanaPrincipal = NULL;
HWND textoEstado = NULL;
HWND botonCaptura = NULL;
HWND botonCargar = NULL;
HWND botonGuardar = NULL;

bool capturaEnCurso = false;
bool picoAbierto = false;

std::thread hiloCaptura;

ULONG_PTR tokenGDIPlus = 0;

std::wstring rutaCanalA;

std::vector<std::pair<double, double>> datosCanalA;

int desplazamientoTabla = 0;

const int FILAS_VISIBLES = 24;

// ------------------------------------------------------------
// Actualizar estado
// ------------------------------------------------------------

void actualizarEstado(const char* texto)
{
    if (textoEstado != NULL)
        SetWindowTextA(textoEstado, texto);
}

// ------------------------------------------------------------
// Conversión de rutas ANSI a Unicode
// ------------------------------------------------------------

std::wstring convertirRuta(const char* ruta)
{
    if (ruta == NULL || ruta[0] == '\0')
        return L"";

    int cantidad = MultiByteToWideChar(
        CP_ACP,
        0,
        ruta,
        -1,
        NULL,
        0
    );

    if (cantidad <= 0)
        return L"";

    std::wstring resultado(cantidad, L'\0');

    MultiByteToWideChar(
        CP_ACP,
        0,
        ruta,
        -1,
        &resultado[0],
        cantidad
    );

    resultado.resize(cantidad - 1);

    return resultado;
}

// ------------------------------------------------------------
// Cargar los datos del Canal A
// ------------------------------------------------------------

void cargarDatosCanalA()
{
    datosCanalA.clear();

    std::ifstream archivo("data_canal_A.txt");

    if (!archivo.is_open())
        return;

    double tiempo;
    double valor;

    while (archivo >> tiempo >> valor)
    {
        datosCanalA.emplace_back(tiempo, valor);
    }

    archivo.close();

    desplazamientoTabla = 0;
}

// ------------------------------------------------------------
// Cargar un gráfico PNG y su archivo de datos asociado
// ------------------------------------------------------------

void cargarEnsayo(HWND hwnd)
{
    char archivo[MAX_PATH] = {};

    OPENFILENAMEA ofn = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = archivo;
    ofn.nMaxFile = MAX_PATH;

    ofn.lpstrFilter =
        "Imagen PNG (*.png)\0*.png\0"
        "Todos los archivos (*.*)\0*.*\0";

    ofn.nFilterIndex = 1;

    ofn.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_HIDEREADONLY;

    ofn.lpstrTitle = "Cargar grafico PNG";

    if (!GetOpenFileNameA(&ofn))
        return;

    rutaCanalA = convertirRuta(archivo);

    // Buscar data_canal_A.txt en la misma carpeta
    // que el gráfico seleccionado.

    fs::path rutaTXT =
        fs::path(archivo).parent_path() /
        "data_canal_A.txt";

    datosCanalA.clear();

    std::ifstream entrada(rutaTXT);

    double tiempo;
    double valor;

    if (entrada.is_open())
    {
        while (entrada >> tiempo >> valor)
        {
            datosCanalA.emplace_back(tiempo, valor);
        }

        entrada.close();
    }

    desplazamientoTabla = 0;

    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);

    if (datosCanalA.empty())
    {
        actualizarEstado(
            "Grafico cargado; no se encontraron datos del Canal A"
        );
    }
    else
    {
        actualizarEstado(
            "Grafico y datos del Canal A cargados"
        );
    }
}

// ------------------------------------------------------------
// Mostrar gráfico de la captura
// ------------------------------------------------------------

void mostrarGraficos()
{
    rutaCanalA = convertirRuta("canal_A.png");

    cargarDatosCanalA();

    InvalidateRect(
        ventanaPrincipal,
        NULL,
        TRUE
    );

    UpdateWindow(ventanaPrincipal);
}

// ------------------------------------------------------------
// Copiar archivo si existe
// ------------------------------------------------------------

bool copiarArchivoSiExiste(
    const fs::path& origen,
    const fs::path& destino)
{
    std::error_code ec;

    if (!fs::exists(origen, ec) || ec)
        return false;

    if (!fs::is_regular_file(origen, ec) || ec)
        return false;

    fs::copy_file(
        origen,
        destino,
        fs::copy_options::overwrite_existing,
        ec
    );

    return !ec;
}

// ------------------------------------------------------------
// Guardar ensayo
// ------------------------------------------------------------

void guardarEnsayo(HWND hwnd)
{
    if (rutaCanalA.empty() && datosCanalA.empty())
    {
        MessageBoxA(
            hwnd,
            "No hay grafico ni datos cargados para guardar.",
            "Guardar ensayo",
            MB_OK | MB_ICONINFORMATION
        );

        return;
    }

    // Seleccionar carpeta de destino

    BROWSEINFOA bi = {};

    bi.hwndOwner = hwnd;

    bi.lpszTitle =
        "Selecciona la carpeta donde copiar los archivos";

    bi.ulFlags =
        BIF_RETURNONLYFSDIRS |
        BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pid =
        SHBrowseForFolderA(&bi);

    if (pid == NULL)
        return;

    char carpeta[MAX_PATH] = {};

    BOOL obtenida =
        SHGetPathFromIDListA(pid, carpeta);

    CoTaskMemFree(pid);

    if (!obtenida)
    {
        MessageBoxA(
            hwnd,
            "No se pudo obtener la carpeta seleccionada.",
            "Error",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    fs::path destino(carpeta);

    bool seCopioAlgo = false;
    bool huboError = false;

    // Copiar gráfico del Canal A

    if (!rutaCanalA.empty())
    {
        bool copiado = copiarArchivoSiExiste(
            fs::path(rutaCanalA),
            destino / "canal_A.png"
        );

        seCopioAlgo = seCopioAlgo || copiado;
        huboError = huboError || !copiado;
    }

    // Copiar archivo de datos del Canal A

    bool datosCopiados = copiarArchivoSiExiste(
        fs::path("data_canal_A.txt"),
        destino / "data_canal_A.txt"
    );

    seCopioAlgo = seCopioAlgo || datosCopiados;

    // Si el archivo original no está disponible,
    // guardar los datos que están cargados en memoria.

    if (!datosCopiados && !datosCanalA.empty())
    {
        std::ofstream salida(
            destino / "data_canal_A.txt"
        );

        if (salida.is_open())
        {
            salida << std::fixed << std::setprecision(6);

            for (const auto& dato : datosCanalA)
            {
                salida
                    << dato.first
                    << ' '
                    << dato.second
                    << '\n';
            }

            salida.close();

            seCopioAlgo = true;
        }
        else
        {
            huboError = true;
        }
    }

    if (!seCopioAlgo)
    {
        MessageBoxA(
            hwnd,
            "No se pudo copiar ningun archivo.\n"
            "Verifica que los archivos de origen existan.",
            "Error al guardar",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    if (huboError)
    {
        actualizarEstado("Guardado parcial");

        MessageBoxA(
            hwnd,
            "Algunos archivos no pudieron copiarse.",
            "Guardado parcial",
            MB_OK | MB_ICONWARNING
        );
    }
    else
    {
        actualizarEstado(
            "Archivos copiados correctamente"
        );

        MessageBoxA(
            hwnd,
            "Los archivos se copiaron directamente "
            "en la carpeta seleccionada.",
            "Guardar ensayo",
            MB_OK | MB_ICONINFORMATION
        );
    }
}

// ------------------------------------------------------------
// Dibujar imagen PNG
// ------------------------------------------------------------

void dibujarImagen(
    Graphics& graphics,
    const std::wstring& ruta,
    int x,
    int y,
    int ancho,
    int alto)
{
    if (ruta.empty())
        return;

    if (ancho <= 0 || alto <= 0)
        return;

    Image imagen(ruta.c_str());

    if (imagen.GetLastStatus() != Ok)
        return;

    UINT anchoOriginal = imagen.GetWidth();
    UINT altoOriginal = imagen.GetHeight();

    if (anchoOriginal == 0 || altoOriginal == 0)
        return;

    double escalaX =
        static_cast<double>(ancho) / anchoOriginal;

    double escalaY =
        static_cast<double>(alto) / altoOriginal;

    double escala =
        (std::min)(escalaX, escalaY);

    int anchoFinal =
        static_cast<int>(anchoOriginal * escala);

    int altoFinal =
        static_cast<int>(altoOriginal * escala);

    int posX =
        x + (ancho - anchoFinal) / 2;

    int posY =
        y + (alto - altoFinal) / 2;

    graphics.DrawImage(
        &imagen,
        posX,
        posY,
        anchoFinal,
        altoFinal
    );
}

// ------------------------------------------------------------
// Dibujar tabla de datos del Canal A
// ------------------------------------------------------------

void dibujarTabla(
    Graphics& graphics,
    int x,
    int y,
    int ancho,
    int alto)
{
    if (ancho <= 0 || alto <= 0)
        return;

    SolidBrush fondo(Color(255, 255, 255, 255));

    SolidBrush tinta(Color(255, 35, 40, 46));

    SolidBrush textoSecundario(
        Color(255, 105, 112, 120)
    );

    Pen linea(
        Color(255, 220, 224, 229),
        1
    );

    graphics.FillRectangle(
        &fondo,
        x,
        y,
        ancho,
        alto
    );

    graphics.DrawRectangle(
        &linea,
        x,
        y,
        ancho,
        alto
    );

    FontFamily familia(L"Segoe UI");

    Font encabezado(
        &familia,
        10,
        FontStyleBold,
        UnitPixel
    );

    Font cuerpo(
        &familia,
        10,
        FontStyleRegular,
        UnitPixel
    );

    Font aviso(
        &familia,
        11,
        FontStyleRegular,
        UnitPixel
    );

    StringFormat formato;

    formato.SetLineAlignment(
        StringAlignmentCenter
    );

    const int altoFila = 24;
    const int margen = 10;

    int anchoTiempo = ancho * 45 / 100;

    // Encabezados

    RectF rectTiempo(
        static_cast<REAL>(x + margen),
        static_cast<REAL>(y),
        static_cast<REAL>(anchoTiempo - margen),
        static_cast<REAL>(altoFila)
    );

    RectF rectValor(
        static_cast<REAL>(x + anchoTiempo),
        static_cast<REAL>(y),
        static_cast<REAL>(ancho - anchoTiempo - margen),
        static_cast<REAL>(altoFila)
    );

    graphics.DrawString(
        L"Tiempo (ms)",
        -1,
        &encabezado,
        rectTiempo,
        &formato,
        &tinta
    );

    graphics.DrawString(
        L"Valor",
        -1,
        &encabezado,
        rectValor,
        &formato,
        &tinta
    );

    graphics.DrawLine(
        &linea,
        x,
        y + altoFila,
        x + ancho,
        y + altoFila
    );

    // Filas disponibles

    int filasDisponibles =
        (alto - altoFila) / altoFila;

    filasDisponibles =
        (std::max)(0, filasDisponibles);

    int inicio =
        (std::max)(0, desplazamientoTabla);

    for (int i = 0; i < filasDisponibles; ++i)
    {
        size_t indice =
            static_cast<size_t>(inicio + i);

        if (indice >= datosCanalA.size())
            break;

        int yy =
            y + altoFila * (i + 1);

        // Fondo alternado

        if (i % 2 == 1)
        {
            SolidBrush alterno(
                Color(255, 247, 248, 250)
            );

            graphics.FillRectangle(
                &alterno,
                x + 1,
                yy,
                ancho - 2,
                altoFila
            );
        }

        graphics.DrawLine(
            &linea,
            x,
            yy + altoFila,
            x + ancho,
            yy + altoFila
        );

        wchar_t tiempo[64];
        wchar_t valor[64];

        swprintf_s(
            tiempo,
            L"%.6f",
            datosCanalA[indice].first
        );

        swprintf_s(
            valor,
            L"%.6f",
            datosCanalA[indice].second
        );

        RectF rectT(
            static_cast<REAL>(x + margen),
            static_cast<REAL>(yy),
            static_cast<REAL>(anchoTiempo - margen),
            static_cast<REAL>(altoFila)
        );

        RectF rectV(
            static_cast<REAL>(x + anchoTiempo),
            static_cast<REAL>(yy),
            static_cast<REAL>(ancho - anchoTiempo - margen),
            static_cast<REAL>(altoFila)
        );

        graphics.DrawString(
            tiempo,
            -1,
            &cuerpo,
            rectT,
            &formato,
            &tinta
        );

        graphics.DrawString(
            valor,
            -1,
            &cuerpo,
            rectV,
            &formato,
            &tinta
        );
    }

    // Mensaje cuando todavía no hay datos

    if (datosCanalA.empty())
    {
        RectF rectAviso(
            static_cast<REAL>(x + 12),
            static_cast<REAL>(y + altoFila + 8),
            static_cast<REAL>(ancho - 24),
            70
        );

        graphics.DrawString(
            L"No hay datos cargados.\n"
            L"Realiza una captura o carga un ensayo "
            L"con data_canal_A.txt.",
            -1,
            &aviso,
            rectAviso,
            NULL,
            &textoSecundario
        );
    }
}

// ------------------------------------------------------------
// Crear controles
// ------------------------------------------------------------

void crearControles(HWND hwnd)
{
    botonCaptura = CreateWindowA(
        "BUTTON",
        "Iniciar captura",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        20, 18, 135, 28,
        hwnd,
        (HMENU)(INT_PTR)ID_BOTON_INICIAR,
        NULL,
        NULL
    );

    botonCargar = CreateWindowA(
        "BUTTON",
        "Cargar ensayo",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        165, 18, 115, 28,
        hwnd,
        (HMENU)(INT_PTR)ID_BOTON_CARGAR,
        NULL,
        NULL
    );

    botonGuardar = CreateWindowA(
        "BUTTON",
        "Guardar ensayo",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        290, 18, 125, 28,
        hwnd,
        (HMENU)(INT_PTR)ID_BOTON_GUARDAR,
        NULL,
        NULL
    );

    textoEstado = CreateWindowA(
        "STATIC",
        "Estado: esperando...",
        WS_VISIBLE | WS_CHILD,
        435, 23, 650, 22,
        hwnd,
        (HMENU)(INT_PTR)ID_ESTADO,
        NULL,
        NULL
    );

    CreateWindowA(
        "STATIC",
        "",
        WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
        20, 58, 1140, 2,
        hwnd,
        NULL,
        NULL,
        NULL
    );
}

// ------------------------------------------------------------
// Procedimiento de ventana
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

        if (abrir_picoscope())
        {
            picoAbierto = true;

            actualizarEstado(
                "PicoScope conectado"
            );
        }
        else
        {
            actualizarEstado(
                "Error al abrir PicoScope"
            );
        }

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_BOTON_INICIAR:
        {
            if (!capturaEnCurso)
            {
                if (!picoAbierto)
                {
                    MessageBoxA(
                        hwnd,
                        "No se pudo abrir el PicoScope.",
                        "Aviso",
                        MB_OK | MB_ICONWARNING
                    );

                    break;
                }

                capturaEnCurso = true;
                captura_activa = 1;

                SetWindowTextA(
                    botonCaptura,
                    "Detener captura"
                );

                actualizarEstado(
                    "Capturando..."
                );

                HWND hwndDestino =
                    ventanaPrincipal;

                hiloCaptura = std::thread(
                    [hwndDestino]()
                    {
                        collect_fast_streaming();

                        generar_Grafico();

                        PostMessage(
                            hwndDestino,
                            WM_CAPTURA_TERMINADA,
                            0,
                            0
                        );
                    }
                );
            }
            else
            {
                captura_activa = 0;

                SetWindowTextA(
                    botonCaptura,
                    "Finalizando..."
                );

                actualizarEstado(
                    "Deteniendo captura..."
                );
            }

            break;
        }

        case ID_BOTON_CARGAR:
        {
            if (!capturaEnCurso)
                cargarEnsayo(hwnd);

            break;
        }

        case ID_BOTON_GUARDAR:
        {
            if (!capturaEnCurso)
                guardarEnsayo(hwnd);

            break;
        }
        }

        return 0;
    }

    case WM_CAPTURA_TERMINADA:
    {
        if (hiloCaptura.joinable())
            hiloCaptura.join();

        capturaEnCurso = false;

        SetWindowTextA(
            botonCaptura,
            "Iniciar captura"
        );

        actualizarEstado(
            "Captura finalizada"
        );

        mostrarGraficos();

        return 0;
    }

    // --------------------------------------------------------
    // Desplazamiento de la tabla con la rueda del mouse
    // --------------------------------------------------------

    case WM_MOUSEWHEEL:
    {
        if (!datosCanalA.empty())
        {
            int delta =
                GET_WHEEL_DELTA_WPARAM(wParam);

            int maxInicio =
                (std::max)(
                    0,
                    static_cast<int>(datosCanalA.size())
                    - FILAS_VISIBLES
                    );

            desplazamientoTabla -=
                delta / WHEEL_DELTA * 3;

            desplazamientoTabla =
                (std::max)(
                    0,
                    (std::min)(
                        desplazamientoTabla,
                        maxInicio
                        )
                    );

            InvalidateRect(
                hwnd,
                NULL,
                FALSE
            );
        }

        return 0;
    }

    case WM_SIZE:
    {
        InvalidateRect(
            hwnd,
            NULL,
            TRUE
        );

        return 0;
    }

    // --------------------------------------------------------
    // Dibujar interfaz
    // --------------------------------------------------------

    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        HDC hdc = BeginPaint(
            hwnd,
            &ps
        );

        RECT rect;

        GetClientRect(
            hwnd,
            &rect
        );

        Graphics graphics(hdc);

        graphics.SetSmoothingMode(
            SmoothingModeHighQuality
        );

        graphics.SetInterpolationMode(
            InterpolationModeHighQualityBicubic
        );

        graphics.Clear(
            Color(255, 245, 246, 248)
        );

        const int margen = 20;
        const int inicioY = 78;

        int anchoTotal =
            rect.right - margen * 2;

        int alto =
            rect.bottom - inicioY - 25;

        if (alto < 100)
            alto = 100;

        // Distribución horizontal:
        // tabla a la izquierda, gráfico a la derecha.

        int anchoTabla =
            (std::max)(
                250,
                anchoTotal * 34 / 100
                );

        int separacion = 14;

        int anchoGrafico =
            anchoTotal - anchoTabla - separacion;

        if (anchoGrafico < 100)
            anchoGrafico = 100;

        // Fuente y pincel de títulos

        FontFamily familia(L"Segoe UI");

        Font titulo(
            &familia,
            13,
            FontStyleBold,
            UnitPixel
        );

        SolidBrush tinta(
            Color(255, 35, 40, 46)
        );

        StringFormat centrado;

        centrado.SetAlignment(
            StringAlignmentCenter
        );

        // Título de la tabla

        RectF rectTituloTabla(
            static_cast<REAL>(margen),
            62,
            static_cast<REAL>(anchoTabla),
            24
        );

        graphics.DrawString(
            L"",
            -1,
            &titulo,
            rectTituloTabla,
            &centrado,
            &tinta
        );

        // Título del gráfico

        int xGrafico =
            margen + anchoTabla + separacion;

        RectF rectTituloGrafico(
            static_cast<REAL>(xGrafico),
            62,
            static_cast<REAL>(anchoGrafico),
            24
        );

        graphics.DrawString(
            L"",
            -1,
            &titulo,
            rectTituloGrafico,
            &centrado,
            &tinta
        );

        // Dibujar tabla

        dibujarTabla(
            graphics,
            margen,
            inicioY,
            anchoTabla,
            alto
        );

        // Fondo del gráfico

        SolidBrush fondoGrafico(
            Color(255, 255, 255, 255)
        );

        Pen bordeGrafico(
            Color(255, 210, 214, 220),
            1
        );

        graphics.FillRectangle(
            &fondoGrafico,
            xGrafico,
            inicioY,
            anchoGrafico,
            alto
        );

        graphics.DrawRectangle(
            &bordeGrafico,
            xGrafico,
            inicioY,
            anchoGrafico,
            alto
        );

        // Mostrar solamente el gráfico del Canal A

        dibujarImagen(
            graphics,
            rutaCanalA,
            xGrafico + 8,
            inicioY + 8,
            anchoGrafico - 16,
            alto - 16
        );

        // Cantidad de muestras

        Font pie(
            &familia,
            9,
            FontStyleRegular,
            UnitPixel
        );

        wchar_t resumen[128];

        swprintf_s(
            resumen,
            L"%zu muestras | Rueda del mouse para desplazarse",
            datosCanalA.size()
        );

        RectF rectResumen(
            static_cast<REAL>(margen),
            static_cast<REAL>(inicioY + alto + 3),
            static_cast<REAL>(anchoTabla),
            18
        );

        graphics.DrawString(
            resumen,
            -1,
            &pie,
            rectResumen,
            NULL,
            &tinta
        );

        EndPaint(
            hwnd,
            &ps
        );

        return 0;
    }

    // --------------------------------------------------------
    // Cerrar aplicación
    // --------------------------------------------------------

    case WM_DESTROY:
    {
        if (capturaEnCurso)
        {
            captura_activa = 0;

            if (hiloCaptura.joinable())
                hiloCaptura.join();
        }

        if (picoAbierto)
        {
            cerrar_picoscope();

            picoAbierto = false;
        }

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
    (void)hPrevInstance;
    (void)lpCmdLine;

    GdiplusStartupInput gdiplusStartupInput;

    if (GdiplusStartup(
        &tokenGDIPlus,
        &gdiplusStartupInput,
        NULL
    ) != Ok)
    {
        MessageBoxA(
            NULL,
            "No se pudo inicializar GDI+.",
            "Error",
            MB_ICONERROR
        );

        return 1;
    }

    const char nombreClase[] =
        "PicoScope2205A";

    WNDCLASSA wc = {};

    wc.lpfnWndProc =
        procedimientoVentana;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        nombreClase;

    wc.hCursor =
        LoadCursor(NULL, IDC_ARROW);

    wc.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc))
    {
        GdiplusShutdown(
            tokenGDIPlus
        );

        return 1;
    }

    ventanaPrincipal = CreateWindowA(
        nombreClase,
        "",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1200,
        700,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (ventanaPrincipal == NULL)
    {
        GdiplusShutdown(
            tokenGDIPlus
        );

        return 1;
    }

    ShowWindow(
        ventanaPrincipal,
        nCmdShow
    );

    UpdateWindow(
        ventanaPrincipal
    );

    MSG mensaje;

    while (GetMessage(
        &mensaje,
        NULL,
        0,
        0
    ) > 0)
    {
        TranslateMessage(
            &mensaje
        );

        DispatchMessage(
            &mensaje
        );
    }

    GdiplusShutdown(
        tokenGDIPlus
    );

    return (int)mensaje.wParam;
}