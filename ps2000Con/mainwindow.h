
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QPointF>
#include <QString>

#include <atomic>
#include <thread>


class QPushButton;
class QLabel;
class QTableWidget;

class QChartView;
class QLineSeries;
class QValueAxis;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    explicit MainWindow(
        QWidget* parent = nullptr
    );

    ~MainWindow();


private:

    std::thread hiloCaptura;

    void finalizarCaptura();
    /* =====================================================
       INTERFAZ
       ===================================================== */

    QPushButton* botonCaptura;
    QPushButton* botonCargar;
    QPushButton* botonGuardar;

    QLabel* textoEstado;
    QTableWidget* tablaDatos;

    QChartView* graficoCanalA;
    QLineSeries* serieCanalA;

    QValueAxis* ejeX;
    QValueAxis* ejeY;


    /* =====================================================
       CONEXION Y CAPTURA
       ===================================================== */

    std::atomic<bool> capturaEnCurso{ false };

    std::atomic<bool> picoAbierto{ false };

    std::atomic<bool> detenerMonitorPico{ false };

    std::thread hiloMonitorPico;


    /* =====================================================
       DATOS
       ===================================================== */

    QVector<QPointF> datosCanalA;

    QString rutaDatosCanalA;

    QString rutaDatosCanalB;


    /* =====================================================
       FUNCIONES DE LA GUI
       ===================================================== */

    void cargarEnsayo();

    void guardarEnsayo();

    void cargarDatosCanalA(
        const QString& ruta
    );

    void actualizarGrafico();

    void actualizarTabla();

    void limpiarDatos();


    /* =====================================================
       CAPTURA
       ===================================================== */

    void iniciarCaptura();

    void capturaTerminada();


    /* =====================================================
       ESTADO
       ===================================================== */

    void actualizarEstado(
        const QString& mensaje
    );
};


#endif

