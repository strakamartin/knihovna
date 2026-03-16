#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *rodic)
    : QMainWindow(rodic)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    inicializujDb();

    // Explicitní propojení signálů se sloty začínajícími na "on"
    connect(ui->tlacitkoPridat, &QPushButton::clicked, this, &MainWindow::onPridejKnihu);
    connect(ui->tlacitkoSmazat, &QPushButton::clicked, this, &MainWindow::onSmazVybranouKnihu);
    connect(ui->listaHledani, &QLineEdit::textChanged, this, &MainWindow::onFiltrujPodleNazvu);
}

void MainWindow::inicializujDb() {
    mMojeDatabaze = QSqlDatabase::addDatabase("QSQLITE");
    mMojeDatabaze.setDatabaseName("knihovna.sqlite");

    if (!mMojeDatabaze.open()) {
        QMessageBox::critical(this, "Chyba databáze",
                             "Nepodařilo se otevřít databázi: " +
                                  mMojeDatabaze.lastError().text());
        return;
    }

    QSqlQuery dotaz;
    // Vytvoření tabulky s českými názvy sloupců
    dotaz.exec("CREATE TABLE IF NOT EXISTS knihy ("
               "id INTEGER UNIQUE PRIMARY KEY AUTOINCREMENT, "
               "nazev TEXT, "
               "autor TEXT, "
               "rok_vydani INTEGER)");

    mModelKnih = new QSqlTableModel(this, mMojeDatabaze);
    mModelKnih->setTable("knihy");
    mModelKnih->setEditStrategy(QSqlTableModel::OnFieldChange);
    mModelKnih->select();

    // Propojení s QTableView z UI souboru
    ui->tabulkaKnih->setModel(mModelKnih);
    //ui->tabulkaKnih->hideColumn(0); // Skrytí ID pro uživatele
}

void MainWindow::onPridejKnihu() {
    int radek = mModelKnih->rowCount();
    if (mModelKnih->insertRow(radek)) {
        mModelKnih->setData(mModelKnih->index(radek, 1), "Nový titul");
        mModelKnih->setData(mModelKnih->index(radek, 2), "Autor");
        mModelKnih->setData(mModelKnih->index(radek, 3), 2005);
        mModelKnih->submitAll();
    }
}

void MainWindow::onSmazVybranouKnihu() {
    QModelIndex index = ui->tabulkaKnih->currentIndex();
    if (index.isValid()) {
        mModelKnih->removeRow(index.row());
        mModelKnih->select();
    } else {
        QMessageBox::warning(this, "Varování", "Není vybrán žádný řádek ke smazání.");
    }
}

void MainWindow::onFiltrujPodleNazvu(const QString &text) {
    // Filtrace pomocí SQL podmínky LIKE
   // mModelKnih->setFilter(QString("nazev LIKE '%%1%'").arg(text));
    mModelKnih->setFilter(QString("nazev LIKE '%" + text + "%'"));
    mModelKnih->select();
}

MainWindow::~MainWindow() {
    delete ui;
}
