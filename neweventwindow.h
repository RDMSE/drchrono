#ifndef NEWEVENTWINDOW_H
#define NEWEVENTWINDOW_H

#include <QDialog>
#include <QString>
#include <QDateTime>
#include <QCompleter>
#include <QStringList>

namespace Ui {
class NewEventWindow;
}

class NewEventWindow : public QDialog
{
    Q_OBJECT

public:
    explicit NewEventWindow(QWidget *parent = nullptr);
    ~NewEventWindow() override;
    
    // Métodos para capturar os dados de entrada
    [[nodiscard]] QString getEventName() const;
    [[nodiscard]] QDateTime getEventStartTime() const;
    
    // Métodos para definir valores (útil para edição)
    void setEventName(const QString &name) const;
    void setEventStartTime(const QDateTime &dateTime) const;
    
    // Método para definir lista de eventos existentes (autocomplete)
    void setExistingEventNames(const QStringList &eventNames) const;

private slots:
    void onAccepted();
    void onRejected();

private:
    Ui::NewEventWindow *ui;
    QCompleter *completer;
};

#endif // NEWEVENTWINDOW_H
