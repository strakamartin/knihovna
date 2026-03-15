#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlTableModel>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *rodic = nullptr);
    ~MainWindow();

private slots:
    void onPridejKnihu();
    void onSmazVybranouKnihu();
    void onFiltrujPodleNazvu(const QString &text);

private:
    Ui::MainWindow *ui;
    QSqlDatabase mMojeDatabaze;
    QSqlTableModel *mModelKnih;

    void inicializujDb();
};

#endif // MAINWINDOW_H
