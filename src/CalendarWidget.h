#pragma once

#include "GoogleCalendarClient.h"

#include <QGridLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QTimer>
#include <QWidget>

class CalendarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CalendarWidget(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void renderEvents(const QList<CalendarEvent> &events);
    void showStatus(const QString &status);
    void showError(const QString &message);
    void showPreviousMonth();
    void showNextMonth();
    void openSettings();
    void updateCountdown();
    void applyCountdownMinutes();
    void handleDateChanged();
    void showToday();
    void updateTimerFlash();

private:
    QString formatEvent(const CalendarEvent &event) const;
    QString formatEventLine(const CalendarEvent &event) const;
    QString formatDayTooltip(const QDate &date) const;
    void buildUi();
    void loadVisibleMonth();
    void renderCalendar();
    void showDayPopup(const QDate &date);
    void startCountdown(int minutes);
    void cancelCountdown();
    void pauseCountdown();
    void toggleCountdown();
    void resetCountdown();
    void stopTimerFlash();
    void loadPersistentSettings();
    void savePersistentSettings() const;
    void applyDisplaySettings();
    QString settingsPath() const;

    GoogleCalendarClient client_;
    QLabel *monthLabel_ = nullptr;
    QGridLayout *calendarGrid_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *countdownLabel_ = nullptr;
    QSpinBox *countdownMinutesEdit_ = nullptr;
    QVector<QToolButton *> dayButtons_;
    QMap<QDate, QList<CalendarEvent>> eventsByDate_;
    QTimer refreshTimer_;
    QTimer countdownTimer_;
    QTimer dateChangeTimer_;
    QTimer timerFlashTimer_;
    QDate visibleMonth_;
    QDate currentDate_;
    QDateTime countdownTarget_;
    qint64 countdownRemainingSeconds_ = 0;
    QPoint dragOrigin_;
    bool windowDragMoved_ = false;
    bool ignoreCountdownRelease_ = false;
    bool loadingPersistentSettings_ = false;
    bool timerFlashOn_ = false;
    int timerFlashCount_ = 0;
};
