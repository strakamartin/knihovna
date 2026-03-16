#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *rodic)
    : QMainWindow(rodic)
    , ui(new Ui::MainWindow)
    , mModelAktivni(nullptr)
    , mVybranyAutorId(-1)
{
    ui->setupUi(this);
    inicializujDb();

    connect(ui->tlacitkoPridat, &QPushButton::clicked, this, &MainWindow::onPridejRadek);
    connect(ui->tlacitkoSmazat, &QPushButton::clicked, this, &MainWindow::onSmazVybranyRadek);
    connect(ui->listaHledani,   &QLineEdit::textChanged, this, &MainWindow::onFiltruj);
    connect(ui->comboSloupce,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFiltruj);
    connect(ui->comboTabulky,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onVyberTabulky);
    connect(ui->listAutoru,     &QListWidget::itemClicked,
            this, &MainWindow::onAutorVybran);
}

// ---------------------------------------------------------------------------
// Database initialisation
// ---------------------------------------------------------------------------
void MainWindow::inicializujDb() {
    mMojeDatabaze = QSqlDatabase::addDatabase("QSQLITE");
    mMojeDatabaze.setDatabaseName("knihovna.sqlite");

    if (!mMojeDatabaze.open()) {
        QMessageBox::critical(this, "Chyba databáze",
                              "Nepodařilo se otevřít databázi: " + mMojeDatabaze.lastError().text());
        return;
    }

    QSqlQuery dotaz;

    // Create author table
    dotaz.exec("CREATE TABLE IF NOT EXISTS autor ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "jmeno TEXT, "
               "prijmeni TEXT, "
               "rok_narozeni INTEGER)");

    // Create books table with FK to autor
    dotaz.exec("CREATE TABLE IF NOT EXISTS knihy ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "nazev TEXT, "
               "rok_vydani INTEGER, "
               "autor_id INTEGER REFERENCES autor(id))");

    // Migration: add autor_id if an older database is opened
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

    // Populate table selector combo box from the actual database tables
    QSqlQuery tabulky("SELECT name FROM sqlite_master "
                      "WHERE type='table' AND name NOT LIKE 'sqlite_%' "
                      "ORDER BY name");
    ui->comboTabulky->blockSignals(true);
    while (tabulky.next()) {
        ui->comboTabulky->addItem(tabulky.value(0).toString());
    }
    ui->comboTabulky->blockSignals(false);

    nactiSeznamAutoru();

    // Load the first table (triggers onVyberTabulky via the signal we just unblocked)
    if (ui->comboTabulky->count() > 0) {
        onVyberTabulky(0);
    }
}

// ---------------------------------------------------------------------------
// Populate author list widget
// ---------------------------------------------------------------------------
void MainWindow::nactiSeznamAutoru() {
    ui->listAutoru->clear();

    QListWidgetItem *vsichni = new QListWidgetItem("-- Všichni autoři --");
    vsichni->setData(Qt::UserRole, -1);
    ui->listAutoru->addItem(vsichni);

    QSqlQuery dotaz("SELECT id, jmeno, prijmeni FROM autor ORDER BY prijmeni, jmeno");
    while (dotaz.next()) {
        int id          = dotaz.value(0).toInt();
        QString jmeno   = dotaz.value(1).toString();
        QString prijmeni = dotaz.value(2).toString();
        QListWidgetItem *polozka = new QListWidgetItem(jmeno + " " + prijmeni);
        polozka->setData(Qt::UserRole, id);
        ui->listAutoru->addItem(polozka);
    }
}

// ---------------------------------------------------------------------------
// Build column filter combo for the active table (including related columns)
// ---------------------------------------------------------------------------
void MainWindow::nactiSloupceProTabulku(const QString &tabulka) {
    mSloupceFiltr.clear();
    ui->comboSloupce->blockSignals(true);
    ui->comboSloupce->clear();

    QSqlQuery q;
    q.exec(QString("PRAGMA table_info(%1)").arg(tabulka));
    while (q.next()) {
        QString colName = q.value(1).toString();
        if (colName == "id") continue;  // hide internal ID

        InfoSloupce info;
        info.zobrazeni = colName;
        info.sloupec   = colName;
        mSloupceFiltr.append(info);
        ui->comboSloupce->addItem(colName);
    }

    // For the 'knihy' table also expose searchable autor columns via FK
    if (tabulka == "knihy") {
        InfoSloupce infoJmeno;
        infoJmeno.zobrazeni  = "autor.jmeno";
        infoJmeno.sloupec    = "autor_id";
        infoJmeno.krizovaRef = true;
        infoJmeno.refTabulka = "autor";
        infoJmeno.refSloupec = "jmeno";
        mSloupceFiltr.append(infoJmeno);
        ui->comboSloupce->addItem("autor.jmeno");

        InfoSloupce infoPrijmeni;
        infoPrijmeni.zobrazeni  = "autor.prijmeni";
        infoPrijmeni.sloupec    = "autor_id";
        infoPrijmeni.krizovaRef = true;
        infoPrijmeni.refTabulka = "autor";
        infoPrijmeni.refSloupec = "prijmeni";
        mSloupceFiltr.append(infoPrijmeni);
        ui->comboSloupce->addItem("autor.prijmeni");
    }

    ui->comboSloupce->blockSignals(false);
}

// ---------------------------------------------------------------------------
// Build and apply the combined SQL filter
// ---------------------------------------------------------------------------
void MainWindow::aktualizujFiltr() {
    if (!mModelAktivni) return;

    QStringList podminkySql;
    const QString tabulka = ui->comboTabulky->currentText();

    // Author filter: narrow to the selected author's rows
    if (mVybranyAutorId >= 0) {
        if (tabulka == "knihy") {
            podminkySql << QString("autor_id = %1").arg(mVybranyAutorId);
        } else if (tabulka == "autor") {
            podminkySql << QString("id = %1").arg(mVybranyAutorId);
        }
    }

    // Column text filter
    // Note: QSqlTableModel::setFilter() takes a plain SQL string without
    // parameter binding, so we escape single quotes manually. In SQLite the
    // LIKE operator has no other special metacharacters that need escaping
    // for correctness (% and _ affect pattern matching but are intentional
    // wildcards here).
    const QString hledani = ui->listaHledani->text();
    if (!hledani.isEmpty()) {
        const int idx = ui->comboSloupce->currentIndex();
        if (idx >= 0 && idx < mSloupceFiltr.size()) {
            QString val = hledani;
            val.replace("'", "''");  // escape single quotes

            const InfoSloupce &info = mSloupceFiltr[idx];
            if (info.krizovaRef) {
                // e.g. autor_id IN (SELECT id FROM autor WHERE jmeno LIKE '%val%')
                podminkySql << QString("%1 IN (SELECT id FROM %2 WHERE %3 LIKE '%%4%')")
                               .arg(info.sloupec, info.refTabulka, info.refSloupec, val);
            } else {
                // e.g. nazev LIKE '%val%'
                podminkySql << QString("%1 LIKE '%%2%'").arg(info.sloupec, val);
            }
        }
    }

    mModelAktivni->setFilter(podminkySql.join(" AND "));
    mModelAktivni->select();

    // QSqlTableModel::select() emits modelReset which resets the header view's
    // section visibility back to the default (all visible). Re-hide the ID
    // column so it stays hidden after every data refresh.
    ui->tabulkaHlavni->hideColumn(0);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void MainWindow::onVyberTabulky(int /*index*/) {
    const QString tabulka = ui->comboTabulky->currentText();
    if (tabulka.isEmpty()) return;

    // Replace the active model
    delete mModelAktivni;
    mModelAktivni = new QSqlTableModel(this, mMojeDatabaze);
    mModelAktivni->setTable(tabulka);
    mModelAktivni->setEditStrategy(QSqlTableModel::OnFieldChange);

    // Connect the view BEFORE calling select() so the initial modelReset is
    // handled by the view with the correct model already in place.
    ui->tabulkaHlavni->setModel(mModelAktivni);

    // When editing the author table inline, refresh the author list
    if (tabulka == "autor") {
        connect(mModelAktivni, &QSqlTableModel::dataChanged,
                this, &MainWindow::nactiSeznamAutoru);
    }

    // Reset filters and rebuild column combo
    mVybranyAutorId = -1;
    ui->listaHledani->clear();
    ui->listAutoru->clearSelection();
    nactiSloupceProTabulku(tabulka);

    // Load data with no filter; hide the internal ID column.
    // aktualizujFiltr() builds an empty filter (= no restriction) and calls
    // select(), then re-hides column 0 so the header reset does not expose it.
    aktualizujFiltr();
}

void MainWindow::onPridejRadek() {
    if (!mModelAktivni) return;

    const QString tabulka = ui->comboTabulky->currentText();
    const int radek = mModelAktivni->rowCount();

    if (!mModelAktivni->insertRow(radek)) return;

    if (tabulka == "knihy") {
        mModelAktivni->setData(mModelAktivni->index(radek, mModelAktivni->fieldIndex("nazev")),      "Nový titul");
        mModelAktivni->setData(mModelAktivni->index(radek, mModelAktivni->fieldIndex("rok_vydani")), 2024);
        if (mVybranyAutorId >= 0) {
            int col = mModelAktivni->fieldIndex("autor_id");
            if (col >= 0)
                mModelAktivni->setData(mModelAktivni->index(radek, col), mVybranyAutorId);
        }
    } else if (tabulka == "autor") {
        mModelAktivni->setData(mModelAktivni->index(radek, mModelAktivni->fieldIndex("jmeno")),        "Jméno");
        mModelAktivni->setData(mModelAktivni->index(radek, mModelAktivni->fieldIndex("prijmeni")),     "Příjmení");
        mModelAktivni->setData(mModelAktivni->index(radek, mModelAktivni->fieldIndex("rok_narozeni")), 1970);
    }

    mModelAktivni->submitAll();
    aktualizujFiltr();  // re-applies current filter and re-hides column 0

    if (tabulka == "autor") {
        nactiSeznamAutoru();
    }
}

void MainWindow::onSmazVybranyRadek() {
    if (!mModelAktivni) return;

    QModelIndex index = ui->tabulkaHlavni->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "Varování", "Není vybrán žádný řádek ke smazání.");
        return;
    }

    mModelAktivni->removeRow(index.row());
    mModelAktivni->submitAll();
    aktualizujFiltr();  // re-applies current filter and re-hides column 0

    if (ui->comboTabulky->currentText() == "autor") {
        nactiSeznamAutoru();
    }
}

void MainWindow::onFiltruj() {
    aktualizujFiltr();
}

void MainWindow::onAutorVybran(QListWidgetItem *polozka) {
    mVybranyAutorId = polozka->data(Qt::UserRole).toInt();
    aktualizujFiltr();
}

MainWindow::~MainWindow() {
    delete ui;
}
