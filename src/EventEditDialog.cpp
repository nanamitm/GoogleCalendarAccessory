#include "EventEditDialog.h"

#include <QCheckBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTimeEdit>
#include <QVBoxLayout>

EventEditDialog::EventEditDialog(const QDate &date, QWidget *parent)
    : QDialog(parent)
{
    buildUi();
    CalendarEvent event;
    event.start = QDateTime(date, QTime(9, 0));
    event.end = QDateTime(date, QTime(10, 0));
    loadEvent(event);
}

EventEditDialog::EventEditDialog(const CalendarEvent &event, QWidget *parent)
    : QDialog(parent),
      originalEvent_(event)
{
    buildUi();
    loadEvent(event);
}

CalendarEvent EventEditDialog::event() const
{
    CalendarEvent event = originalEvent_;
    event.title = titleEdit_->text().trimmed().isEmpty()
        ? QStringLiteral("(無題)")
        : titleEdit_->text().trimmed();
    event.location = locationEdit_->text().trimmed();
    event.description = descriptionEdit_->toPlainText().trimmed();
    event.allDay = allDayEdit_->isChecked();
    event.start = QDateTime(startDateEdit_->date(), allDayEdit_->isChecked() ? QTime(0, 0) : startTimeEdit_->time());
    event.end = QDateTime(endDateEdit_->date(), allDayEdit_->isChecked() ? QTime(0, 0) : endTimeEdit_->time());
    if (!event.allDay && event.end <= event.start) {
        event.end = event.start.addSecs(3600);
    }
    if (event.allDay && event.end.date() < event.start.date()) {
        event.end = QDateTime(event.start.date(), QTime(0, 0));
    }
    return event;
}

void EventEditDialog::updateTimeEditors()
{
    const bool allDay = allDayEdit_->isChecked();
    startTimeEdit_->setEnabled(!allDay);
    endTimeEdit_->setEnabled(!allDay);
}

void EventEditDialog::buildUi()
{
    setWindowTitle(QStringLiteral("予定"));
    resize(460, 360);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    titleEdit_ = new QLineEdit(this);
    locationEdit_ = new QLineEdit(this);
    descriptionEdit_ = new QTextEdit(this);
    descriptionEdit_->setFixedHeight(90);
    startDateEdit_ = new QDateEdit(this);
    startDateEdit_->setCalendarPopup(true);
    startTimeEdit_ = new QTimeEdit(this);
    endDateEdit_ = new QDateEdit(this);
    endDateEdit_->setCalendarPopup(true);
    endTimeEdit_ = new QTimeEdit(this);
    allDayEdit_ = new QCheckBox(QStringLiteral("終日"), this);

    form->addRow(QStringLiteral("タイトル"), titleEdit_);
    form->addRow(QStringLiteral("場所"), locationEdit_);
    form->addRow(QStringLiteral("開始日"), startDateEdit_);
    form->addRow(QStringLiteral("開始時刻"), startTimeEdit_);
    form->addRow(QStringLiteral("終了日"), endDateEdit_);
    form->addRow(QStringLiteral("終了時刻"), endTimeEdit_);
    form->addRow(QString(), allDayEdit_);
    form->addRow(QStringLiteral("メモ"), descriptionEdit_);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("キャンセル"));
    connect(buttons, &QDialogButtonBox::accepted, this, &EventEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &EventEditDialog::reject);
    connect(allDayEdit_, &QCheckBox::toggled, this, &EventEditDialog::updateTimeEditors);
    layout->addWidget(buttons);

    setStyleSheet(QStringLiteral(R"(
        QDialog {
            background: #20262c;
            color: #f7f8fb;
            font-family: "Yu Gothic UI", "Meiryo", sans-serif;
            font-size: 13px;
        }
        QLineEdit, QDateEdit, QTimeEdit, QTextEdit {
            background: #151a20;
            border: 1px solid #3a4652;
            border-radius: 6px;
            color: #f7f8fb;
            min-height: 28px;
            padding: 2px 8px;
        }
        QPushButton {
            background: #3f8fd2;
            border: 0;
            border-radius: 6px;
            color: white;
            min-height: 30px;
            min-width: 86px;
        }
    )"));
}

void EventEditDialog::loadEvent(const CalendarEvent &event)
{
    titleEdit_->setText(event.title);
    locationEdit_->setText(event.location);
    descriptionEdit_->setPlainText(event.description);
    allDayEdit_->setChecked(event.allDay);
    startDateEdit_->setDate(event.start.date());
    startTimeEdit_->setTime(event.start.time().isValid() ? event.start.time() : QTime(9, 0));
    if (event.allDay && event.end.isValid()) {
        endDateEdit_->setDate(event.end.date().addDays(-1));
    } else {
        endDateEdit_->setDate(event.end.isValid() ? event.end.date() : event.start.date());
    }
    endTimeEdit_->setTime(event.end.time().isValid() ? event.end.time() : QTime(10, 0));
    updateTimeEditors();
}
