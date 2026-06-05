#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSettings>
#include <QTcpServer>
#include <QUrl>

struct CalendarEvent
{
    QString id;
    QString title;
    QString location;
    QString description;
    QDateTime start;
    QDateTime end;
    bool allDay = false;
};

class GoogleCalendarClient : public QObject
{
    Q_OBJECT

public:
    explicit GoogleCalendarClient(QObject *parent = nullptr);

    QString clientId() const;
    QString calendarId() const;
    int days() const;
    bool hasAccessToken() const;
    bool isConfigured() const;
    void reloadSettings();

public slots:
    void signIn();
    void fetchEvents();
    void fetchEventsForRange(const QDate &firstDay, const QDate &lastDay);
    void saveEvent(const CalendarEvent &event);
    void deleteEvent(const CalendarEvent &event);

signals:
    void statusChanged(const QString &status);
    void eventsReady(const QList<CalendarEvent> &events);
    void eventSaved();
    void errorOccurred(const QString &message);

private slots:
    void handleAuthorizationRequest();

private:
    void loadSettings();
    void saveTokens();
    void exchangeCodeForToken(const QString &code);
    void refreshAccessToken();
    void requestEvents(const QDateTime &timeMin, const QDateTime &timeMax);
    void sendSaveEventRequest(const CalendarEvent &event);
    void sendDeleteEventRequest(const CalendarEvent &event);
    QJsonObject eventToJson(const CalendarEvent &event) const;
    QUrl buildAuthorizationUrl() const;
    QString makeCodeVerifier() const;
    QString makeCodeChallenge(const QString &verifier) const;
    QString formEncode(const QUrlQuery &query) const;
    QDateTime parseGoogleDateTime(const QJsonObject &object, bool *allDay) const;

    QSettings settings_;
    QNetworkAccessManager network_;
    QTcpServer callbackServer_;

    QString clientId_;
    QString clientSecret_;
    QString calendarId_ = QStringLiteral("primary");
    int days_ = 7;

    QString accessToken_;
    QString refreshToken_;
    QDateTime accessTokenExpiry_;
    QDate pendingFirstDay_;
    QDate pendingLastDay_;
    CalendarEvent pendingSaveEvent_;
    bool hasPendingSaveEvent_ = false;
    CalendarEvent pendingDeleteEvent_;
    bool hasPendingDeleteEvent_ = false;

    QString codeVerifier_;
    QString expectedState_;
    quint16 callbackPort_ = 0;
};
