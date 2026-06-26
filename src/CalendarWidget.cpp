#include "CalendarWidget.h"

#include "EventEditDialog.h"
#include "SettingsDialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QDate>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QScreen>
#include <QStyle>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

CalendarWidget::CalendarWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    loadPersistentSettings();
    visibleMonth_ = QDate::currentDate();
    currentDate_ = QDate::currentDate();

    connect(&client_, &GoogleCalendarClient::eventsReady, this, &CalendarWidget::renderEvents);
    connect(&client_, &GoogleCalendarClient::eventSaved, this, &CalendarWidget::loadVisibleMonth);
    connect(&client_, &GoogleCalendarClient::statusChanged, this, &CalendarWidget::showStatus);
    connect(&client_, &GoogleCalendarClient::errorOccurred, this, &CalendarWidget::showError);
    connect(&refreshTimer_, &QTimer::timeout, this, &CalendarWidget::loadVisibleMonth);
    connect(&countdownTimer_, &QTimer::timeout, this, &CalendarWidget::updateCountdown);
    connect(&dateChangeTimer_, &QTimer::timeout, this, &CalendarWidget::handleDateChanged);
    connect(&timerFlashTimer_, &QTimer::timeout, this, &CalendarWidget::updateTimerFlash);

    refreshTimer_.start(5 * 60 * 1000);
    dateChangeTimer_.start(60 * 1000);
    QTimer::singleShot(100, this, [this]() {
        if (!client_.isConfigured()) {
            openSettings();
        }
        loadVisibleMonth();
    });
}

void CalendarWidget::buildUi()
{
    setWindowTitle(QStringLiteral("Google Calendar Accessory"));
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(430, 430);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(6);

    statusLabel_ = new QLabel(QStringLiteral("起動中..."), this);
    statusLabel_->setObjectName(QStringLiteral("status"));
    statusLabel_->setVisible(false);
    statusLabel_->setCursor(Qt::SizeAllCursor);

    auto *header = new QHBoxLayout();
    header->setSpacing(6);

    auto *previousButton = new QToolButton(this);
    previousButton->setText(QStringLiteral("<"));
    previousButton->setObjectName(QStringLiteral("navButton"));
    previousButton->setToolTip(QStringLiteral("前月"));
    previousButton->setCursor(Qt::SizeAllCursor);

    monthLabel_ = new QLabel(this);
    monthLabel_->setObjectName(QStringLiteral("month"));
    monthLabel_->setAlignment(Qt::AlignCenter);
    monthLabel_->setCursor(Qt::SizeAllCursor);

    auto *nextButton = new QToolButton(this);
    nextButton->setText(QStringLiteral(">"));
    nextButton->setObjectName(QStringLiteral("navButton"));
    nextButton->setToolTip(QStringLiteral("次月"));
    nextButton->setCursor(Qt::SizeAllCursor);

    header->addWidget(previousButton);
    header->addWidget(monthLabel_, 1);
    auto *todayButton = new QToolButton(this);
    todayButton->setText(QStringLiteral("今日"));
    todayButton->setObjectName(QStringLiteral("todayButton"));
    todayButton->setToolTip(QStringLiteral("今月へ戻る"));
    header->addWidget(todayButton);
    header->addWidget(nextButton);

    auto *calendarFrame = new QFrame(this);
    calendarFrame->setObjectName(QStringLiteral("calendar"));
    calendarGrid_ = new QGridLayout(calendarFrame);
    calendarGrid_->setContentsMargins(8, 8, 8, 8);
    calendarGrid_->setHorizontalSpacing(4);
    calendarGrid_->setVerticalSpacing(4);

    const QStringList weekdays = {
        QStringLiteral("日"), QStringLiteral("月"), QStringLiteral("火"), QStringLiteral("水"),
        QStringLiteral("木"), QStringLiteral("金"), QStringLiteral("土")
    };
    for (int column = 0; column < weekdays.size(); ++column) {
        auto *label = new QLabel(weekdays.at(column), calendarFrame);
        label->setObjectName(QStringLiteral("weekday"));
        label->setAlignment(Qt::AlignCenter);
        calendarGrid_->addWidget(label, 0, column);
    }

    for (int row = 0; row < 6; ++row) {
        for (int column = 0; column < 7; ++column) {
            auto *button = new QToolButton(calendarFrame);
            button->setObjectName(QStringLiteral("dayButton"));
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            button->setMinimumSize(48, 42);
            dayButtons_.append(button);
            calendarGrid_->addWidget(button, row + 1, column);
        }
    }

    countdownLabel_ = new QLabel(this);
    countdownLabel_->setObjectName(QStringLiteral("countdown"));
    countdownLabel_->setAlignment(Qt::AlignCenter);
    countdownLabel_->setText(QStringLiteral("クリックで開始"));
    countdownLabel_->setToolTip(QStringLiteral("クリック: 開始/一時停止、ダブルクリック: リセット"));

    auto *countdownBar = new QHBoxLayout();
    countdownBar->setSpacing(6);
    countdownMinutesEdit_ = new QSpinBox(this);
    countdownMinutesEdit_->setObjectName(QStringLiteral("timerMinutes"));
    countdownMinutesEdit_->setRange(1, 999);
    countdownMinutesEdit_->setValue(30);
    countdownMinutesEdit_->setSuffix(QStringLiteral("分"));
    countdownBar->addWidget(countdownLabel_, 1);
    countdownBar->addWidget(countdownMinutesEdit_);

    root->addLayout(header);
    root->addWidget(calendarFrame, 1);
    root->addLayout(countdownBar);

    statusLabel_->installEventFilter(this);
    monthLabel_->installEventFilter(this);
    countdownLabel_->installEventFilter(this);
    previousButton->installEventFilter(this);
    nextButton->installEventFilter(this);

    connect(previousButton, &QToolButton::clicked, this, &CalendarWidget::showPreviousMonth);
    connect(nextButton, &QToolButton::clicked, this, &CalendarWidget::showNextMonth);
    connect(todayButton, &QToolButton::clicked, this, &CalendarWidget::showToday);
    connect(countdownMinutesEdit_, qOverload<int>(&QSpinBox::valueChanged), this, &CalendarWidget::applyCountdownMinutes);

    setStyleSheet(QStringLiteral(R"(
        QWidget {
            color: #f7f8fb;
            font-family: "Yu Gothic UI", "Meiryo", sans-serif;
            font-size: 13px;
        }
        CalendarWidget {
            background: transparent;
        }
        #title {
            font-size: 19px;
            font-weight: 700;
        }
        #status {
            color: #cfd7df;
            font-size: 12px;
        }
        #month {
            color: #f7f8fb;
            font-size: 16px;
            font-weight: 700;
            background: rgba(22, 28, 34, 118);
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 6px;
            min-height: 30px;
        }
        #navButton, #todayButton {
            background: rgba(22, 28, 34, 118);
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 6px;
            color: #f7f8fb;
            font-size: 13px;
            min-width: 36px;
            min-height: 30px;
        }
        #navButton {
            font-size: 16px;
            min-width: 32px;
        }
        #navButton:hover, #todayButton:hover {
            background: rgba(255, 255, 255, 34);
        }
        #calendar {
            background: rgba(22, 28, 34, 210);
            border: 1px solid rgba(255, 255, 255, 42);
            border-radius: 8px;
        }
        #weekday {
            color: #cfd7df;
            font-size: 12px;
            font-weight: 700;
        }
        #dayButton {
            background: rgba(255, 255, 255, 14);
            border: 1px solid rgba(255, 255, 255, 16);
            border-radius: 6px;
            color: #f7f8fb;
            font-size: 13px;
            padding: 4px;
        }
        #dayButton:hover {
            background: rgba(255, 255, 255, 34);
        }
        #dayButton[muted="true"] {
            color: #7f8994;
            background: rgba(255, 255, 255, 8);
        }
        #dayButton[today="true"] {
            border-color: #81d4fa;
        }
        #dayButton[hasEvents="true"] {
            background: rgba(66, 165, 245, 58);
        }
        #countdown {
            background: rgba(22, 28, 34, 150);
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 6px;
            color: #f7f8fb;
            font-size: 13px;
            min-height: 30px;
            padding: 0 8px;
        }
        #countdown[alert="true"] {
            background: rgba(229, 72, 77, 210);
            border-color: rgba(255, 255, 255, 90);
        }
        #timerMinutes {
            background: rgba(22, 28, 34, 150);
            border: 1px solid rgba(255, 255, 255, 24);
            border-radius: 6px;
            color: #f7f8fb;
            font-size: 12px;
            min-height: 30px;
            min-width: 52px;
            padding: 0 6px;
        }
        #timerMinutes:hover {
            background: rgba(255, 255, 255, 34);
        }
    )"));
}

bool CalendarWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == countdownLabel_) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                ignoreCountdownRelease_ = true;
                resetCountdown();
                mouseEvent->accept();
                return true;
            }
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (ignoreCountdownRelease_) {
                    ignoreCountdownRelease_ = false;
                    mouseEvent->accept();
                    return true;
                }
                toggleCountdown();
                mouseEvent->accept();
                return true;
            }
        }
    }

    const bool dragTarget = watched == statusLabel_
        || watched == monthLabel_
        || (qobject_cast<QToolButton *>(watched) && qobject_cast<QToolButton *>(watched)->objectName() == QStringLiteral("navButton"));
    if (!dragTarget) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            dragOrigin_ = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
            windowDragMoved_ = false;
            if (watched == monthLabel_) {
                mouseEvent->accept();
                return true;
            }
        }
    }

    if (event->type() == QEvent::MouseMove) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->buttons() & Qt::LeftButton) {
            windowDragMoved_ = true;
            move(mouseEvent->globalPosition().toPoint() - dragOrigin_);
            mouseEvent->accept();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease && windowDragMoved_) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            windowDragMoved_ = false;
            savePersistentSettings();
            mouseEvent->accept();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void CalendarWidget::closeEvent(QCloseEvent *event)
{
    savePersistentSettings();
    QWidget::closeEvent(event);
}

void CalendarWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *refresh = menu.addAction(QStringLiteral("更新"));
    QAction *settings = menu.addAction(QStringLiteral("設定"));
    QAction *login = menu.addAction(QStringLiteral("Googleにログイン"));
    menu.addSeparator();
    QAction *quit = menu.addAction(QStringLiteral("終了"));

    QAction *chosen = menu.exec(event->globalPos());
    if (chosen == refresh) {
        loadVisibleMonth();
    } else if (chosen == settings) {
        openSettings();
    } else if (chosen == login) {
        client_.signIn();
    } else if (chosen == quit) {
        qApp->quit();
    }
}

void CalendarWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragOrigin_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}

void CalendarWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragOrigin_);
    }
}

void CalendarWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        savePersistentSettings();
    }
    QWidget::mouseReleaseEvent(event);
}

void CalendarWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!loadingPersistentSettings_) {
        savePersistentSettings();
    }
}

void CalendarWidget::renderEvents(const QList<CalendarEvent> &events)
{
    eventsByDate_.clear();
    for (const CalendarEvent &event : events) {
        QDate date = event.start.date();
        if (event.allDay && event.end.isValid()) {
            const QDate lastDate = event.end.date().addDays(-1);
            while (date <= lastDate) {
                eventsByDate_[date].append(event);
                date = date.addDays(1);
            }
        } else {
            eventsByDate_[date].append(event);
        }
    }

    renderCalendar();
}

void CalendarWidget::showStatus(const QString &status)
{
    statusLabel_->setText(status);
}

void CalendarWidget::showError(const QString &message)
{
    statusLabel_->setText(message);
    if (message.contains(QStringLiteral("権限"))) {
        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("Googleカレンダー"),
            message + QStringLiteral("\n\nこのまま再ログインしますか？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (answer == QMessageBox::Yes) {
            client_.signIn();
        }
        renderCalendar();
        return;
    }
    if (message.contains(QStringLiteral("保存"))
        || message.contains(QStringLiteral("トークン"))) {
        QMessageBox::warning(this, QStringLiteral("Googleカレンダー"), message);
    }
    renderCalendar();
}

void CalendarWidget::showPreviousMonth()
{
    visibleMonth_ = visibleMonth_.addMonths(-1);
    loadVisibleMonth();
}

void CalendarWidget::showNextMonth()
{
    visibleMonth_ = visibleMonth_.addMonths(1);
    loadVisibleMonth();
}

void CalendarWidget::showToday()
{
    currentDate_ = QDate::currentDate();
    visibleMonth_ = currentDate_;
    loadVisibleMonth();
}

void CalendarWidget::openSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        client_.reloadSettings();
        applyDisplaySettings();
        loadVisibleMonth();
    }
}

void CalendarWidget::updateCountdown()
{
    if (!countdownTimer_.isActive()) {
        return;
    }

    if (!countdownTarget_.isValid()) {
        cancelCountdown();
        return;
    }

    const qint64 secondsLeft = QDateTime::currentDateTime().secsTo(countdownTarget_);
    if (secondsLeft <= 0) {
        countdownTimer_.stop();
        countdownTarget_ = QDateTime();
        countdownRemainingSeconds_ = 0;
        countdownLabel_->setText(QStringLiteral("時間です"));
        QApplication::beep();
        timerFlashCount_ = 0;
        timerFlashOn_ = false;
        timerFlashTimer_.start(500);
        QMessageBox::information(this, QStringLiteral("カウントダウン"), QStringLiteral("時間になりました。"));
        return;
    }

    const qint64 minutes = secondsLeft / 60;
    const qint64 seconds = secondsLeft % 60;
    countdownLabel_->setText(QStringLiteral("カウントダウン  %1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0')));
}

void CalendarWidget::applyCountdownMinutes()
{
    savePersistentSettings();
    if (countdownTimer_.isActive()) {
        return;
    }

    countdownRemainingSeconds_ = countdownMinutesEdit_->value() * 60;
    const qint64 minutes = countdownRemainingSeconds_ / 60;
    countdownLabel_->setText(QStringLiteral("待機中  %1:00").arg(minutes, 2, 10, QLatin1Char('0')));
}

void CalendarWidget::updateTimerFlash()
{
    timerFlashOn_ = !timerFlashOn_;
    countdownLabel_->setProperty("alert", timerFlashOn_);
    countdownLabel_->style()->unpolish(countdownLabel_);
    countdownLabel_->style()->polish(countdownLabel_);
    ++timerFlashCount_;
    if (timerFlashCount_ >= 20) {
        stopTimerFlash();
    }
}

void CalendarWidget::handleDateChanged()
{
    const QDate today = QDate::currentDate();
    if (today == currentDate_) {
        return;
    }

    const QDate previousDate = currentDate_;
    currentDate_ = today;

    const bool followingCurrentMonth = visibleMonth_.year() == previousDate.year()
        && visibleMonth_.month() == previousDate.month();
    if (followingCurrentMonth) {
        visibleMonth_ = today;
        loadVisibleMonth();
        return;
    }

    renderCalendar();
}

QString CalendarWidget::formatEvent(const CalendarEvent &event) const
{
    const QDate today = QDate::currentDate();
    const QString dateText = event.start.date() == today
        ? QStringLiteral("今日")
        : event.start.date().toString(QStringLiteral("M/d"));

    QString timeText;
    if (event.allDay) {
        timeText = QStringLiteral("終日");
    } else {
        timeText = event.start.time().toString(QStringLiteral("H:mm"));
        if (event.end.isValid()) {
            timeText += QStringLiteral("-%1").arg(event.end.time().toString(QStringLiteral("H:mm")));
        }
    }

    QString text = QStringLiteral("%1  %2\n%3").arg(dateText, timeText, event.title);
    if (!event.location.isEmpty()) {
        text += QStringLiteral("\n%1").arg(event.location);
    }
    return text;
}

QString CalendarWidget::formatEventLine(const CalendarEvent &event) const
{
    if (event.allDay) {
        return QStringLiteral("終日  %1").arg(event.title);
    }

    QString line = QStringLiteral("%1  %2")
        .arg(event.start.time().toString(QStringLiteral("H:mm")), event.title);
    if (!event.location.isEmpty()) {
        line += QStringLiteral("\n%1").arg(event.location);
    }
    return line;
}

QString CalendarWidget::formatDayTooltip(const QDate &date) const
{
    const QList<CalendarEvent> events = eventsByDate_.value(date);
    if (events.isEmpty()) {
        return date.toString(QStringLiteral("yyyy年 M月 d日"));
    }

    QStringList lines;
    lines.append(date.toString(QStringLiteral("yyyy年 M月 d日")));
    for (const CalendarEvent &event : events) {
        lines.append(formatEventLine(event).replace(QStringLiteral("\n"), QStringLiteral(" / ")));
    }
    return lines.join(QStringLiteral("\n"));
}

void CalendarWidget::loadVisibleMonth()
{
    const QDate firstDay(visibleMonth_.year(), visibleMonth_.month(), 1);
    const QDate lastDay(visibleMonth_.year(), visibleMonth_.month(), firstDay.daysInMonth());
    monthLabel_->setText(firstDay.toString(QStringLiteral("yyyy年 M月")));
    renderCalendar();
    // Extend range to cover overflow cells from adjacent months shown in the grid
    const QDate gridStart = firstDay.addDays(-(firstDay.dayOfWeek() % 7));
    const QDate gridEnd = gridStart.addDays(dayButtons_.size() - 1);
    client_.fetchEventsForRange(gridStart, gridEnd);
}

void CalendarWidget::renderCalendar()
{
    if (!visibleMonth_.isValid() || !monthLabel_) {
        return;
    }

    const QDate firstDay(visibleMonth_.year(), visibleMonth_.month(), 1);
    const int sundayBasedOffset = firstDay.dayOfWeek() % 7;
    QDate cellDate = firstDay.addDays(-sundayBasedOffset);
    monthLabel_->setText(firstDay.toString(QStringLiteral("yyyy年 M月")));

    for (QToolButton *button : dayButtons_) {
        const bool inMonth = cellDate.month() == visibleMonth_.month();
        const bool today = cellDate == currentDate_;
        const int eventCount = eventsByDate_.value(cellDate).size();
        QString text = QString::number(cellDate.day());
        if (eventCount > 0) {
            text += QStringLiteral("\n%1件").arg(eventCount);
        }

        button->setText(text);
        button->setToolTip(formatDayTooltip(cellDate));
        button->setProperty("muted", !inMonth);
        button->setProperty("today", today);
        button->setProperty("hasEvents", eventCount > 0);
        button->style()->unpolish(button);
        button->style()->polish(button);

        disconnect(button, nullptr, this, nullptr);
        const QDate dateForButton = cellDate;
        connect(button, &QToolButton::clicked, this, [this, dateForButton]() {
            showDayPopup(dateForButton);
        });

        cellDate = cellDate.addDays(1);
    }
}

void CalendarWidget::showDayPopup(const QDate &date)
{
    QDialog dialog(this);
    dialog.setWindowTitle(date.toString(QStringLiteral("yyyy年 M月 d日")));
    dialog.resize(380, 300);

    auto *layout = new QVBoxLayout(&dialog);
    auto *title = new QLabel(date.toString(QStringLiteral("yyyy年 M月 d日")), &dialog);
    title->setObjectName(QStringLiteral("popupTitle"));
    layout->addWidget(title);

    const QList<CalendarEvent> events = eventsByDate_.value(date);
    if (events.isEmpty()) {
        layout->addWidget(new QLabel(QStringLiteral("予定はありません"), &dialog));
    } else {
        auto *eventsLayout = new QVBoxLayout();
        for (const CalendarEvent &event : events) {
            auto *row = new QHBoxLayout();
            auto *eventButton = new QPushButton(formatEventLine(event), &dialog);
            eventButton->setObjectName(QStringLiteral("eventButton"));
            eventButton->setFlat(true);
            eventButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            eventButton->setMinimumHeight(44);
            eventButton->setText(eventButton->text().replace(QStringLiteral("\n"), QStringLiteral(" / ")));
            connect(eventButton, &QPushButton::clicked, &dialog, [this, &dialog, event]() {
                EventEditDialog editor(event, this);
                if (editor.exec() == QDialog::Accepted) {
                    dialog.accept();
                    client_.saveEvent(editor.event());
                }
            });
            auto *deleteButton = new QPushButton(QStringLiteral("削除"), &dialog);
            deleteButton->setObjectName(QStringLiteral("deleteButton"));
            deleteButton->setEnabled(!event.id.isEmpty());
            connect(deleteButton, &QPushButton::clicked, &dialog, [this, &dialog, event]() {
                const auto answer = QMessageBox::question(
                    this,
                    QStringLiteral("予定削除"),
                    QStringLiteral("この予定を削除しますか？\n\n%1").arg(event.title),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
                if (answer == QMessageBox::Yes) {
                    dialog.accept();
                    client_.deleteEvent(event);
                }
            });
            row->addWidget(eventButton, 1);
            row->addWidget(deleteButton);
            eventsLayout->addLayout(row);
        }
        eventsLayout->addStretch(1);
        layout->addLayout(eventsLayout, 1);
    }

    auto *addButton = new QPushButton(QStringLiteral("予定を追加"), &dialog);
    connect(addButton, &QPushButton::clicked, &dialog, [this, &dialog, date]() {
        EventEditDialog editor(date, this);
        if (editor.exec() == QDialog::Accepted) {
            dialog.accept();
            client_.saveEvent(editor.event());
        }
    });
    layout->addWidget(addButton);

    auto *closeButton = new QPushButton(QStringLiteral("閉じる"), &dialog);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton);

    dialog.setStyleSheet(QStringLiteral(R"(
        QDialog {
            background: #20262c;
            color: #f7f8fb;
            font-family: "Yu Gothic UI", "Meiryo", sans-serif;
        }
        #popupTitle {
            font-size: 16px;
            font-weight: 700;
        }
        QTextEdit {
            background: #151a20;
            border: 1px solid #3a4652;
            border-radius: 6px;
            color: #f7f8fb;
            padding: 8px;
        }
        QPushButton {
            background: #3f8fd2;
            border: 0;
            border-radius: 6px;
            color: white;
            min-height: 30px;
        }
        #eventButton {
            background: #151a20;
            border: 1px solid #3a4652;
            color: #f7f8fb;
            padding: 8px;
            text-align: left;
        }
        #eventButton:hover {
            background: #26313b;
        }
        #deleteButton {
            background: #7d2d35;
            min-width: 54px;
        }
        #deleteButton:hover {
            background: #9a3b45;
        }
    )"));
    dialog.exec();
}

void CalendarWidget::startCountdown(int minutes)
{
    stopTimerFlash();
    countdownTarget_ = QDateTime::currentDateTime().addSecs(minutes * 60);
    countdownRemainingSeconds_ = minutes * 60;
    countdownTimer_.start(1000);
    updateCountdown();
}

void CalendarWidget::cancelCountdown()
{
    stopTimerFlash();
    countdownTimer_.stop();
    countdownTarget_ = QDateTime();
    countdownRemainingSeconds_ = 0;
    countdownLabel_->setText(QStringLiteral("クリックで開始"));
}

void CalendarWidget::pauseCountdown()
{
    stopTimerFlash();
    if (!countdownTimer_.isActive() || !countdownTarget_.isValid()) {
        return;
    }

    countdownRemainingSeconds_ = qMax<qint64>(1, QDateTime::currentDateTime().secsTo(countdownTarget_));
    countdownTimer_.stop();
    const qint64 minutes = countdownRemainingSeconds_ / 60;
    const qint64 seconds = countdownRemainingSeconds_ % 60;
    countdownLabel_->setText(QStringLiteral("一時停止  %1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0')));
}

void CalendarWidget::toggleCountdown()
{
    stopTimerFlash();
    if (countdownTimer_.isActive()) {
        pauseCountdown();
        return;
    }

    const qint64 seconds = countdownRemainingSeconds_ > 0
        ? countdownRemainingSeconds_
        : countdownMinutesEdit_->value() * 60;
    countdownTarget_ = QDateTime::currentDateTime().addSecs(seconds);
    countdownTimer_.start(1000);
    updateCountdown();
}

void CalendarWidget::resetCountdown()
{
    stopTimerFlash();
    countdownTimer_.stop();
    countdownTarget_ = QDateTime();
    applyCountdownMinutes();
}

void CalendarWidget::stopTimerFlash()
{
    timerFlashTimer_.stop();
    timerFlashOn_ = false;
    timerFlashCount_ = 0;
    if (countdownLabel_) {
        countdownLabel_->setProperty("alert", false);
        countdownLabel_->style()->unpolish(countdownLabel_);
        countdownLabel_->style()->polish(countdownLabel_);
    }
}

void CalendarWidget::loadPersistentSettings()
{
    loadingPersistentSettings_ = true;

    QSettings settings(settingsPath(), QSettings::IniFormat);
    const QSize savedSize = settings.value(QStringLiteral("Display/size"), size()).toSize();
    if (savedSize.isValid()) {
        resize(savedSize);
    }

    const QPoint savedPosition = settings.value(QStringLiteral("Display/position")).toPoint();
    if (!savedPosition.isNull()) {
        move(savedPosition);
    }

    const int timerMinutes = settings.value(QStringLiteral("Timer/minutes"), 30).toInt();
    countdownMinutesEdit_->setValue(qBound(1, timerMinutes, 999));
    applyCountdownMinutes();
    applyDisplaySettings();

    loadingPersistentSettings_ = false;
}

void CalendarWidget::savePersistentSettings() const
{
    if (loadingPersistentSettings_) {
        return;
    }

    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Display/position"), pos());
    settings.setValue(QStringLiteral("Display/size"), size());
    settings.setValue(QStringLiteral("Timer/minutes"), countdownMinutesEdit_->value());
    settings.sync();
}

void CalendarWidget::applyDisplaySettings()
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    const int opacity = qBound(35, settings.value(QStringLiteral("Display/opacity"), 100).toInt(), 100);
    setWindowOpacity(opacity / 100.0);

    const bool alwaysOnTop = settings.value(QStringLiteral("Display/always_on_top"), true).toBool();
    Qt::WindowFlags flags = windowFlags();
    if (alwaysOnTop) {
        flags |= Qt::WindowStaysOnTopHint;
    } else {
        flags &= ~Qt::WindowStaysOnTopHint;
    }
    if (flags != windowFlags()) {
        const bool wasVisible = isVisible();
        setWindowFlags(flags);
        if (wasVisible) {
            show();
        }
    }

    if (settings.value(QStringLiteral("Display/reset_position"), false).toBool()) {
        const QRect available = screen() ? screen()->availableGeometry() : QGuiApplication::primaryScreen()->availableGeometry();
        move(available.center() - rect().center());
        settings.remove(QStringLiteral("Display/reset_position"));
        settings.setValue(QStringLiteral("Display/position"), pos());
        settings.sync();
    }
}

QString CalendarWidget::settingsPath() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/calendar_accessory.ini");
}
