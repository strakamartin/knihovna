#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *rodic)
    : QMainWindow(rodic)
    , ui(new Ui::MainWindow)
    , mVybranyAutorId(-1)
{
    ui->setupUi(this);
    inicializujDb();

    // Explicitní propojení signálů se sloty začínajícími na "on"
    connect(ui->tlacitkoPridat, &QPushButton::clicked, this, &MainWindow::onPridejKnihu);
    connect(ui->tlacitkoSmazat, &QPushButton::clicked, this, &MainWindow::onSmazVybranouKnihu);
    connect(ui->listaHledani, &QLineEdit::textChanged, this, &MainWindow::onFiltrujPodleNazvu);
    connect(ui->tlacitkoPridatAutora, &QPushButton::clicked, this, &MainWindow::onPridejAutora);
    connect(ui->tlacitkoSmazatAutora, &QPushButton::clicked, this, &MainWindow::onSmazVybranehAutora);
    connect(ui->listAutoru, &QListWidget::itemClicked, this, &MainWindow::onAutorVybran);
}

void MainWindow::inicializujDb() {
    mMojeDatabaze = QSqlDatabase::addDatabase("QSQLITE");
    mMojeDatabaze.setDatabaseName("knihovna.sqlite");

    if (!mMojeDatabaze.open()) {
        QMessageBox::critical(this, "Chyba databáze",
                             "Nepodařilo se otevřít databázi: " + mMojeDatabaze.lastError().text());
        return;
    }

    QSqlQuery dotaz;

    // Vytvoření tabulky autorů
    dotaz.exec("CREATE TABLE IF NOT EXISTS autor ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "jmeno TEXT, "
               "prijmeni TEXT, "
               "rok_narozeni INTEGER)");

    // Vytvoření tabulky knih s cizím klíčem na autora
    dotaz.exec("CREATE TABLE IF NOT EXISTS knihy ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "nazev TEXT, "
               "rok_vydani INTEGER, "
               "autor_id INTEGER REFERENCES autor(id))");

    // Migrace: přidání sloupce autor_id pokud chybí (pro existující databáze)
    QSqlQuery checkSloupce;
    checkSloupce.exec("PRAGMA table_info(knihy)");
    bool maAutorId = false;
    while (checkSloupce.next()) {
        if (checkSloupce.value(1).toString() == "autor_id") {
            maAutorId = true;
            break;
        }
    }
    if (!maAutorId) {
        dotaz.exec("ALTER TABLE knihy ADD COLUMN autor_id INTEGER REFERENCES autor(id)");
    }

    // Nastavení modelu pro tabulku knih
    mModelKnih = new QSqlTableModel(this, mMojeDatabaze);
    mModelKnih->setTable("knihy");
    mModelKnih->setEditStrategy(QSqlTableModel::OnFieldChange);
    mModelKnih->select();

    // Propojení s QTableView z UI souboru
    ui->tabulkaKnih->setModel(mModelKnih);
    ui->tabulkaKnih->hideColumn(0); // Skrytí ID pro uživatele

    // Nastavení modelu pro tabulku autorů
    mModelAutoru = new QSqlTableModel(this, mMojeDatabaze);
    mModelAutoru->setTable("autor");
    mModelAutoru->setEditStrategy(QSqlTableModel::OnFieldChange);
    mModelAutoru->select();

    ui->tabulkaAutoru->setModel(mModelAutoru);
    ui->tabulkaAutoru->hideColumn(0); // Skrytí ID pro uživatele

    // Aktualizace seznamu autorů po změně dat v tabulce
    connect(mModelAutoru, &QSqlTableModel::dataChanged, this, &MainWindow::nactiSeznamAutoru);

    nactiSeznamAutoru();
}

void MainWindow::nactiSeznamAutoru() {
    ui->listAutoru->clear();

    // Položka pro zobrazení všech knih
    QListWidgetItem *vsichni = new QListWidgetItem("-- Všichni autoři --");
    vsichni->setData(Qt::UserRole, -1);
    ui->listAutoru->addItem(vsichni);

    QSqlQuery dotaz("SELECT id, jmeno, prijmeni FROM autor ORDER BY prijmeni, jmeno");
    while (dotaz.next()) {
        int id = dotaz.value(0).toInt();
        QString jmeno = dotaz.value(1).toString();
        QString prijmeni = dotaz.value(2).toString();
        QListWidgetItem *polozka = new QListWidgetItem(jmeno + " " + prijmeni);
        polozka->setData(Qt::UserRole, id);
        ui->listAutoru->addItem(polozka);
    }
}

void MainWindow::aktualizujFiltrKnih() {
    QString filtr;
    QString hledani = ui->listaHledani->text();
    // Escapování jednoduchých uvozovek pro zamezení SQL injection
    QString hledaniEsc = hledani;
    hledaniEsc.replace("'", "''");

    if (mVybranyAutorId >= 0) {
        filtr = QString("autor_id = %1").arg(mVybranyAutorId);
        if (!hledaniEsc.isEmpty()) {
            filtr += QString(" AND nazev LIKE '%%1%'").arg(hledaniEsc);
        }
    } else if (!hledaniEsc.isEmpty()) {
        filtr = QString("nazev LIKE '%%1%'").arg(hledaniEsc);
    }

    mModelKnih->setFilter(filtr);
    mModelKnih->select();
}

void MainWindow::onPridejKnihu() {
    int radek = mModelKnih->rowCount();
    if (mModelKnih->insertRow(radek)) {
        int colNazev = mModelKnih->fieldIndex("nazev");
        int colRok = mModelKnih->fieldIndex("rok_vydani");
        int colAutorId = mModelKnih->fieldIndex("autor_id");
        mModelKnih->setData(mModelKnih->index(radek, colNazev), "Nový titul");
        mModelKnih->setData(mModelKnih->index(radek, colRok), 2024);
        if (mVybranyAutorId >= 0 && colAutorId >= 0) {
            mModelKnih->setData(mModelKnih->index(radek, colAutorId), mVybranyAutorId);
        }
        mModelKnih->submitAll();
    }
}

void MainWindow::onSmazVybranouKnihu() {
    QModelIndex index = ui->tabulkaKnih->currentIndex();
    if (index.isValid()) {
        mModelKnih->removeRow(index.row());
        mModelKnih->submitAll();
        mModelKnih->select();
    } else {
        QMessageBox::warning(this, "Varování", "Není vybrán žádný řádek ke smazání.");
    }
}

void MainWindow::onFiltrujPodleNazvu(const QString &text) {
    Q_UNUSED(text)
    // Filtrace pomocí SQL podmínek LIKE a autor_id
    aktualizujFiltrKnih();
}

void MainWindow::onPridejAutora() {
    int radek = mModelAutoru->rowCount();
    if (mModelAutoru->insertRow(radek)) {
        mModelAutoru->setData(mModelAutoru->index(radek, mModelAutoru->fieldIndex("jmeno")), "Jméno");
        mModelAutoru->setData(mModelAutoru->index(radek, mModelAutoru->fieldIndex("prijmeni")), "Příjmení");
        mModelAutoru->setData(mModelAutoru->index(radek, mModelAutoru->fieldIndex("rok_narozeni")), 1970);
        mModelAutoru->submitAll();
    }
    nactiSeznamAutoru();
}

void MainWindow::onSmazVybranehAutora() {
    QModelIndex index = ui->tabulkaAutoru->currentIndex();
    if (index.isValid()) {
        mModelAutoru->removeRow(index.row());
        mModelAutoru->submitAll();
        mModelAutoru->select();
        nactiSeznamAutoru();
    } else {
        QMessageBox::warning(this, "Varování", "Není vybrán žádný autor ke smazání.");
    }
}

void MainWindow::onAutorVybran(QListWidgetItem *polozka) {
    mVybranyAutorId = polozka->data(Qt::UserRole).toInt();
    aktualizujFiltrKnih();
}

MainWindow::~MainWindow() {
    delete ui;
}
