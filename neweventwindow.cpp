#include "neweventwindow.h"
#include "ui_neweventwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QCompleter>
#include <QStringListModel>
#include "utils/timeutils.h"

NewEventWindow::NewEventWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NewEventWindow)
    , completer(nullptr) {
    ui->setupUi(this);
    
    // Configurar o formato de data/hora para MM/DD/YYYY HH:MM
    ui->timeEventStart->setDisplayFormat("MM/dd/yyyy HH:mm");
    ui->timeEventStart->setDateTime(Utils::DateTimeUtils::now());
    
    // Habilitar calendário popup - muito mais intuitivo!
    ui->timeEventStart->setCalendarPopup(true);
    
    // Inicializar o completer para autocomplete
    completer = new QCompleter(this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    ui->edtEventName->setCompleter(completer);
    
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &NewEventWindow::onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &NewEventWindow::onRejected);
}

NewEventWindow::~NewEventWindow() {
    delete ui;
}

QString NewEventWindow::getEventName() const {
    return ui->edtEventName->text().trimmed();
}

QDateTime NewEventWindow::getEventStartTime() const {
    return ui->timeEventStart->dateTime();
}

void NewEventWindow::setEventName(const QString &name) const {
    ui->edtEventName->setText(name);
}

void NewEventWindow::setEventStartTime(const QDateTime &dateTime) const {
    ui->timeEventStart->setDateTime(dateTime);
}

void NewEventWindow::onAccepted() {
    if (getEventName().isEmpty()) {
        QMessageBox::warning(this, "Error", "Event name is missing");
        ui->edtEventName->setFocus();
        reject();
        return;
    }

    if (getEventStartTime() < Utils::DateTimeUtils::now()) {
        QMessageBox::warning(this, "Error", "Event shouldn't be in past");
        ui->timeEventStart->setDateTime(Utils::DateTimeUtils::now());
        ui->timeEventStart->setFocus();
        reject();
        return;
    }
    accept();
}

void NewEventWindow::onRejected() {
    reject();
}

// Método para definir lista de eventos existentes (autocomplete)
void NewEventWindow::setExistingEventNames(const QStringList &eventNames) const {
    if (completer) {
        auto *model = new QStringListModel(eventNames, completer);
        completer->setModel(model);
    }
}
