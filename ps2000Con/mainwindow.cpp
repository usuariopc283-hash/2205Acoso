#include "mainwindow.h"

#include "ps2000Con.h"
#include "Procesar_data.h"

#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <QAbstractItemView>
#include <QPainter>

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>

#include <QThread>
#include <QMetaObject>

#include <atomic>
#include <thread>
#include <windows.h>

#define WM_CAPTURA_TERMINADA (WM_APP + 1)

/* =========================================================
   CONSTRUCTOR
   ========================================================= */

    MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    /* =====================================================
       VENTANA
       ===================================================== */

    setWindowTitle(
        "PicoScope 2205A"
    );

    resize(
        1100,
        750
    );


    /* =====================================================
       CONTENEDOR PRINCIPAL
       ===================================================== */

    QWidget* contenedor =
        new QWidget(this);

    setCentralWidget(
        contenedor
    );


    /* =====================================================
       BOTONES
       ===================================================== */

    botonCaptura =
        new QPushButton(
            "Iniciar captura"
        );

    botonCargar =
        new QPushButton(
            "Cargar ensayo"
        );

    botonGuardar =
        new QPushButton(
            "Guardar ensayo"
        );


    botonCaptura->setMinimumHeight(
        32
    );

    botonCargar->setMinimumHeight(
        32
    );

    botonGuardar->setMinimumHeight(
        32
    );


    QHBoxLayout* layoutBotones =
        new QHBoxLayout;


    layoutBotones->addWidget(
        botonCaptura
    );

    layoutBotones->addWidget(
        botonCargar
    );

    layoutBotones->addWidget(
        botonGuardar
    );

    layoutBotones->addStretch();


    /* =====================================================
       ESTADO
       ===================================================== */

    textoEstado =
        new QLabel(
            "Iniciando..."
        );

    textoEstado->setMinimumHeight(
        28
    );


    /* =====================================================
       SERIE DEL CANAL A
       ===================================================== */

    serieCanalA =
        new QLineSeries();

    serieCanalA->setName(
        "Canal A"
    );


    /* =====================================================
       EJES
       ===================================================== */

    ejeX =
        new QValueAxis();

    ejeY =
        new QValueAxis();


    ejeX->setTitleText(
        "Tiempo (ms)"
    );

    ejeY->setTitleText(
        "Valor"
    );


    ejeX->setRange(
        0,
        1
    );

    ejeY->setRange(
        -100,
        100
    );


    /* =====================================================
       CHART
       ===================================================== */

    QChart* chart =
        new QChart();


    chart->addSeries(
        serieCanalA
    );


    chart->addAxis(
        ejeX,
        Qt::AlignBottom
    );

    chart->addAxis(
        ejeY,
        Qt::AlignLeft
    );


    serieCanalA->attachAxis(
        ejeX
    );

    serieCanalA->attachAxis(
        ejeY
    );


    chart->setTitle(
        "Canal A"
    );


    chart->legend()->setVisible(
        false
    );


    /* =====================================================
       VISTA DEL GRAFICO
       ===================================================== */

    graficoCanalA =
        new QChartView(
            chart
        );


    graficoCanalA->setRenderHint(
        QPainter::Antialiasing
    );


    graficoCanalA->setMinimumHeight(
        300
    );


    /* =====================================================
       TABLA
       ===================================================== */

    tablaDatos =
        new QTableWidget();


    tablaDatos->setColumnCount(
        2
    );


    QStringList encabezados;

    encabezados
        << "Tiempo (ms)"
        << "Valor";


    tablaDatos->setHorizontalHeaderLabels(
        encabezados
    );


    tablaDatos->horizontalHeader()
        ->setStretchLastSection(
            true
        );


    tablaDatos->horizontalHeader()
        ->setSectionResizeMode(
            0,
            QHeaderView::ResizeToContents
        );


    tablaDatos->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );


    tablaDatos->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );


    /* =====================================================
       LAYOUT PRINCIPAL
       ===================================================== */

    QVBoxLayout* layoutPrincipal =
        new QVBoxLayout(
            contenedor
        );


    layoutPrincipal->setContentsMargins(
        20,
        20,
        20,
        20
    );


    layoutPrincipal->setSpacing(
        12
    );


    layoutPrincipal->addLayout(
        layoutBotones
    );


    layoutPrincipal->addWidget(
        textoEstado
    );


    layoutPrincipal->addWidget(
        graficoCanalA,
        1
    );


    layoutPrincipal->addWidget(
        tablaDatos,
        1
    );


    /* =====================================================
       CONEXIONES DE LOS BOTONES
       ===================================================== */

    connect(
        botonCargar,
        &QPushButton::clicked,
        this,
        &MainWindow::cargarEnsayo
    );


    connect(
        botonGuardar,
        &QPushButton::clicked,
        this,
        &MainWindow::guardarEnsayo
    );


    connect(
        botonCaptura,
        &QPushButton::clicked,
        this,
        &MainWindow::iniciarCaptura
    );


    /* =====================================================
       ESTADO INICIAL
       ===================================================== */

    actualizarEstado(
        "PicoScope desconectado; intentando conectar..."
    );


    /* =====================================================
       MONITOR DEL PICOSCOPE
       ===================================================== */

    detenerMonitorPico =
        false;


    hiloMonitorPico =
        std::thread(
            [this]()
            {
                while (
                    !detenerMonitorPico.load()
                    )
                {
                    /*
                        No intentamos conectar mientras
                        hay una captura activa.
                    */

                    if (
                        !capturaEnCurso.load() &&
                        !picoAbierto.load()
                        )
                    {
                        /*
                            Intentamos abrir el PicoScope.
                        */

                        int resultado =
                            abrir_picoscope();


                        if (resultado)
                        {
                            picoAbierto =
                                true;


                            QMetaObject::invokeMethod(
                                this,
                                [this]()
                                {
                                    actualizarEstado(
                                        "PicoScope conectado"
                                    );
                                },
                                Qt::QueuedConnection
                            );
                        }
                        else
                        {
                            picoAbierto =
                                false;


                            QMetaObject::invokeMethod(
                                this,
                                [this]()
                                {
                                    actualizarEstado(
                                        "PicoScope desconectado; reintentando..."
                                    );
                                },
                                Qt::QueuedConnection
                            );
                        }
                    }


                    /*
                        Esperamos antes de volver
                        a intentar.
                    */

                    QThread::msleep(
                        2000
                    );
                }
            }
        );
}


/* =========================================================
   DESTRUCTOR
   ========================================================= */

MainWindow::~MainWindow()
{
    detenerMonitorPico =
        true;


    if (
        hiloMonitorPico.joinable()
        )
    {
        hiloMonitorPico.join();
    }
}


/* =========================================================
   ACTUALIZAR ESTADO
   ========================================================= */

void MainWindow::actualizarEstado(
    const QString& mensaje
)
{
    if (!textoEstado)
        return;


    textoEstado->setText(
        mensaje
    );
}


/* =========================================================
   CARGAR ENSAYO
   ========================================================= */

void MainWindow::cargarEnsayo()
{
    QString archivo =
        QFileDialog::getOpenFileName(
            this,
            "Cargar ensayo",
            QDir::currentPath(),
            "Archivos de datos (*.txt)"
        );


    if (archivo.isEmpty())
        return;


    QFileInfo info(
        archivo
    );


    QString carpeta =
        info.absolutePath();


    rutaDatosCanalA =
        QDir(carpeta).filePath(
            "data_canal_A.txt"
        );


    rutaDatosCanalB =
        QDir(carpeta).filePath(
            "data_canal_B.txt"
        );


    if (
        !QFile::exists(
            rutaDatosCanalA
        )
        )
    {
        QMessageBox::warning(
            this,
            "Error",
            "No se encontró data_canal_A.txt"
        );

        return;
    }


    cargarDatosCanalA(
        rutaDatosCanalA
    );


    actualizarGrafico();

    actualizarTabla();


    actualizarEstado(
        "Ensayo cargado correctamente"
    );
}


/* =========================================================
   CARGAR DATOS DEL CANAL A
   ========================================================= */

void MainWindow::cargarDatosCanalA(const QString& ruta)
{
    datosCanalA.clear();

    QFile archivo(ruta);

    if (!archivo.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        actualizarEstado("Error al abrir data_canal_A.txt");
        return;
    }

    QTextStream entrada(&archivo);

    while (!entrada.atEnd())
    {
        QString linea = entrada.readLine().trimmed();

        if (linea.isEmpty())
            continue;

        // Acepta comas, punto y coma o espacios
        // como separadores de las dos columnas.
        QStringList campos = linea.split(
            QRegularExpression("[,;\\s]+"),
            Qt::SkipEmptyParts
        );

        if (campos.size() < 2)
            continue;

        bool tiempoValido = false;
        bool valorValido = false;

        double tiempo = campos[0].toDouble(&tiempoValido);
        double valor = campos[1].toDouble(&valorValido);

        // Ignorar cabeceras y líneas no numéricas.
        if (!tiempoValido || !valorValido)
            continue;

        datosCanalA.append(QPointF(tiempo, valor));
    }

    archivo.close();
}



/* =========================================================
   ACTUALIZAR GRAFICO
   ========================================================= */

void MainWindow::actualizarGrafico()
{
    serieCanalA->clear();


    if (
        datosCanalA.isEmpty()
        )
    {
        ejeX->setRange(
            0,
            1
        );

        ejeY->setRange(
            -100,
            100
        );

        return;
    }


    serieCanalA->append(
        datosCanalA
    );


    double minX =
        datosCanalA.first().x();

    double maxX =
        datosCanalA.first().x();

    double minY =
        datosCanalA.first().y();

    double maxY =
        datosCanalA.first().y();


    for (
        const QPointF& punto
        : datosCanalA
        )
    {
        if (
            punto.x() < minX
            )
        {
            minX =
                punto.x();
        }


        if (
            punto.x() > maxX
            )
        {
            maxX =
                punto.x();
        }


        if (
            punto.y() < minY
            )
        {
            minY =
                punto.y();
        }


        if (
            punto.y() > maxY
            )
        {
            maxY =
                punto.y();
        }
    }


    double margenX =
        (maxX - minX) * 0.02;


    double margenY =
        (maxY - minY) * 0.05;


    if (
        margenX <= 0
        )
    {
        margenX = 1;
    }


    if (
        margenY <= 0
        )
    {
        margenY = 1;
    }


    ejeX->setRange(
        minX - margenX,
        maxX + margenX
    );


    ejeY->setRange(
        minY - margenY,
        maxY + margenY
    );
}


/* =========================================================
   ACTUALIZAR TABLA
   ========================================================= */

void MainWindow::actualizarTabla()
{
    tablaDatos->setRowCount(
        0
    );


    for (
        const QPointF& punto
        : datosCanalA
        )
    {
        int fila =
            tablaDatos->rowCount();


        tablaDatos->insertRow(
            fila
        );


        QTableWidgetItem* itemTiempo =
            new QTableWidgetItem(
                QString::number(
                    punto.x(),
                    'f',
                    6
                )
            );


        QTableWidgetItem* itemValor =
            new QTableWidgetItem(
                QString::number(
                    punto.y(),
                    'f',
                    6
                )
            );


        tablaDatos->setItem(
            fila,
            0,
            itemTiempo
        );


        tablaDatos->setItem(
            fila,
            1,
            itemValor
        );
    }
}


/* =========================================================
   LIMPIAR DATOS
   ========================================================= */

void MainWindow::limpiarDatos()
{ 
    datosCanalA.clear();


    serieCanalA->clear();


    tablaDatos->setRowCount(
        0
    );


    ejeX->setRange(
        0,
        1
    );


    ejeY->setRange(
        -100,
        100
    );
}


/* =========================================================
   INICIAR CAPTURA
   ========================================================= */


void MainWindow::iniciarCaptura()
{
    // Si ya se está capturando, el botón solicita detener.
    if (capturaEnCurso.load())
    {
        finalizarCaptura();
        return;
    }

    // No iniciar si el dispositivo no está conectado.
    if (!picoAbierto.load())
    {
        actualizarEstado("PicoScope desconectado");
        return;
    }

    // Evitar reutilizar un hilo que todavía sea joinable.
    if (hiloCaptura.joinable())
        hiloCaptura.join();

    capturaEnCurso = true;

    botonCaptura->setText("Detener captura");
    botonCargar->setEnabled(false);
    botonGuardar->setEnabled(false);

    limpiarDatos();

    actualizarEstado("Capturando datos...");

    // La adquisición se ejecuta en segundo plano.
    hiloCaptura = std::thread([this]()
        {
            establecer_estado_captura(1);

            collect_streaming();

            // collect_streaming() ya terminó y cerró data.txt.
            QMetaObject::invokeMethod(
                this,
                [this]()
                {
                    actualizarEstado("Procesando datos...");
                },
                Qt::QueuedConnection
            );

            Procesar_data();

            // Volver a la interfaz para cargar y graficar.
            QMetaObject::invokeMethod(
                this,
                [this]()
                {
                    rutaDatosCanalA =
                        QDir::current().filePath(
                            "data_canal_A.txt"
                        );

                    rutaDatosCanalB =
                        QDir::current().filePath(
                            "data_canal_B.txt"
                        );

                    if (QFile::exists(rutaDatosCanalA))
                    {
                        cargarDatosCanalA(rutaDatosCanalA);
                        actualizarGrafico();
                        actualizarTabla();

                        actualizarEstado(
                            "Captura y procesamiento finalizados"
                        );
                    }
                    else
                    {
                        actualizarEstado(
                            "Error: no se generó data_canal_A.txt"
                        );
                    }

                    capturaEnCurso = false;

                    botonCaptura->setText("Iniciar captura");
                    botonCargar->setEnabled(true);
                    botonGuardar->setEnabled(true);
                },
                Qt::QueuedConnection
            );
        });
}


void MainWindow::finalizarCaptura()
{
    actualizarEstado("Deteniendo captura...");

    // collect_streaming() saldrá de su bucle y
    // cerrará el archivo cuando vea el estado 0.
    establecer_estado_captura(0);

    // El botón permanece deshabilitado hasta que
    // termine el procesamiento y se actualice la gráfica.
    botonCaptura->setEnabled(true);
    connect(
        botonCaptura,
        &QPushButton::clicked,
        this,
        &MainWindow::iniciarCaptura
    );
}




/* =========================================================
   CAPTURA TERMINADA
   ========================================================= */

void MainWindow::capturaTerminada()
{
    actualizarEstado(
        "Captura terminada"
    );
}


/* =========================================================
   GUARDAR ENSAYO
   ========================================================= */

void MainWindow::guardarEnsayo()
{
    if (
        datosCanalA.isEmpty()
        )
    {
        QMessageBox::warning(
            this,
            "Guardar ensayo",
            "No hay datos del Canal A para guardar."
        );

        return;
    }


    QString carpeta =
        QFileDialog::getExistingDirectory(
            this,
            "Selecciona la carpeta donde guardar los archivos",
            QDir::currentPath()
        );


    if (
        carpeta.isEmpty()
        )
    {
        return;
    }


    bool canalAGuardado =
        false;

    bool canalBGuardado =
        false;


    /* =====================================================
       CANAL A
       ===================================================== */

    if (
        !rutaDatosCanalA.isEmpty() &&
        QFile::exists(
            rutaDatosCanalA
        )
        )
    {
        QString destinoA =
            QDir(carpeta).filePath(
                "data_canal_A.txt"
            );


        if (
            QFile::exists(
                destinoA
            )
            )
        {
            QFile::remove(
                destinoA
            );
        }


        canalAGuardado =
            QFile::copy(
                rutaDatosCanalA,
                destinoA
            );
    }
    else
    {
        QString destinoA =
            QDir(carpeta).filePath(
                "data_canal_A.txt"
            );


        QFile archivoA(
            destinoA
        );


        if (
            archivoA.open(
                QIODevice::WriteOnly |
                QIODevice::Text
            )
            )
        {
            QTextStream salida(
                &archivoA
            );


            for (
                const QPointF& punto
                : datosCanalA
                )
            {
                salida
                    << QString::number(
                        punto.x(),
                        'f',
                        6
                    )
                    << " "
                    << QString::number(
                        punto.y(),
                        'f',
                        6
                    )
                    << "\n";
            }


            archivoA.close();

            canalAGuardado =
                true;
        }
    }


    /* =====================================================
       CANAL B
       ===================================================== */

    if (
        !rutaDatosCanalB.isEmpty() &&
        QFile::exists(
            rutaDatosCanalB
        )
        )
    {
        QString destinoB =
            QDir(carpeta).filePath(
                "data_canal_B.txt"
            );


        if (
            QFile::exists(
                destinoB
            )
            )
        {
            QFile::remove(
                destinoB
            );
        }


        canalBGuardado =
            QFile::copy(
                rutaDatosCanalB,
                destinoB
            );
    }


    /* =====================================================
       ESTADO FINAL
       ===================================================== */

    if (
        canalAGuardado &&
        canalBGuardado
        )
    {
        actualizarEstado(
            "Ensayo guardado correctamente"
        );
    }
    else if (
        canalAGuardado
        )
    {
        actualizarEstado(
            "Canal A guardado; Canal B no disponible"
        );
    }
    else
    {
        QMessageBox::warning(
            this,
            "Guardar ensayo",
            "No se pudieron guardar los datos."
        );
    }
}

