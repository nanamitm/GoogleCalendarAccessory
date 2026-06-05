#pragma once

#include "GoogleCalendarClient.h"

#include <QDialog>

class QCheckBox;
class QDateEdit;
class QLineEdit;
class QTextEdit;
class QTimeEdit;

class EventEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EventEditDialog(const QDate &date, QWidget *parent = nullptr);
    explicit EventEditDialog(const CalendarEvent &event, QWidget *parent = nullptr);

    CalendarEvent event() const;

private slots:
    void updateTimeEditors();

private:
    void buildUi();
    void loadEvent(const CalendarEvent &event);

    CalendarEvent originalEvent_;
    QLineEdit *titleEdit_ = nullptr;
    QLineEdit *locationEdit_ = nullptr;
    QTextEdit *descriptionEdit_ = nullptr;
    QDateEdit *startDateEdit_ = nullptr;
    QTimeEdit *startTimeEdit_ = nullptr;
    QDateEdit *endDateEdit_ = nullptr;
    QTimeEdit *endTimeEdit_ = nullptr;
    QCheckBox *allDayEdit_ = nullptr;
};
