#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlTableModel>
#include <QListWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Metadata for one entry in the column filter combo box
struct InfoSloupce {
    QString zobrazeni;    // text shown in the combo box
    QString sloupec;      // column name in the base table (or FK column for cross-table)
    // Cross-table filter: when set, filter is  sloupec IN (SELECT id FROM refTabulka WHERE refSloupec LIKE ...)
    bool    krizovaRef = false;
    QString refTabulka;   // referenced table (e.g. "autor")
    QString refSloupec;   // column in the referenced table (e.g. "jmeno")
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *rodic = nullptr);
    ~MainWindow();

private slots:
    void onVyberTabulky(int index);
    void onPridejRadek();
    void onSmazVybranyRadek();
    void onFiltruj();
    void onAutorVybran(QListWidgetItem *polozka);

private:
    Ui::MainWindow *ui;
    QSqlDatabase mMojeDatabaze;
    QSqlTableModel *mModelAktivni;
    int mVybranyAutorId;
    QList<InfoSloupce> mSloupceFiltr;

    void inicializujDb();
    void nactiSeznamAutoru();
    void nactiSloupceProTabulku(const QString &tabulka);
    void aktualizujFiltr();
};

#endif // MAINWINDOW_H
