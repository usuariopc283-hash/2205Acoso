#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QPushButton;
class QLabel;
class QTableWidget;
class QScrollArea;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    QPushButton* botonCaptura;
    QPushButton* botonCargar;
    QPushButton* botonGuardar;

    QLabel* textoEstado;
    QLabel* imagenCanalA;

    QTableWidget* tablaDatos;
};

#endif