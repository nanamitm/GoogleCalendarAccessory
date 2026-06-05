#include "GoogleCalendarClient.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QTimeZone>
#include <QUrlQuery>

namespace {
constexpr auto kAuthEndpoint = "https://accounts.google.com/o/oauth2/v2/auth";
constexpr auto kTokenEndpoint = "https://oauth2.googleapis.com/token";
constexpr auto kCalendarEndpoint = "https://www.googleapis.com/calendar/v3/calendars/%1/events";
constexpr auto kEventEndpoint = "https://www.googleapis.com/calendar/v3/calendars/%1/events/%2";

QString base64Url(QByteArray bytes)
{
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
}

GoogleCalendarClient::GoogleCalendarClient(QObject *parent)
    : QObject(parent),
      settings_(QCoreApplication::applicationDirPath() + QStringLiteral("/calendar_accessory.ini"), QSettings::IniFormat)
{
    loadSettings();

    connect(&callbackServer_, &QTcpServer::newConnection,
            this, &GoogleCalendarClient::handleAuthorizationRequest);
}

QString GoogleCalendarClient::clientId() const { return clientId_; }
QString GoogleCalendarClient::calendarId() const { return calendarId_; }
int GoogleCalendarClient::days() const { return days_; }
bool GoogleCalendarClient::hasAccessToken() const { return !accessToken_.isEmpty(); }

bool GoogleCalendarClient::isConfigured() const
{
    return !clientId_.isEmpty() && !clientId_.startsWith(QStringLiteral("YOUR_"));
}

void GoogleCalendarClient::reloadSettings()
{
    loadSettings();
}

void GoogleCalendarClient::loadSettings()
{
    clientId_ = settings_.value(QStringLiteral("Google/client_id")).toString().trimmed();
    clientSecret_ = settings_.value(QStringLiteral("Google/client_secret")).toString().trimmed();
    calendarId_ = settings_.value(QStringLiteral("Google/calendar_id"), QStringLiteral("primary")).toString().trimmed();
    days_ = settings_.value(QStringLiteral("Google/days"), 7).toInt();
    refreshToken_ = settings_.value(QStringLiteral("Google/refresh_token")).toString();
    accessToken_ = settings_.value(QStringLiteral("Google/access_token")).toString();
    accessTokenExpiry_ = settings_.value(QStringLiteral("Google/access_token_expiry")).toDateTime();
}

void GoogleCalendarClient::saveTokens()
{
    settings_.setValue(QStringLiteral("Google/access_token"), accessToken_);
    settings_.setValue(QStringLiteral("Google/refresh_token"), refreshToken_);
    settings_.setValue(QStringLiteral("Google/access_token_expiry"), accessTokenExpiry_);
    settings_.sync();
}

void GoogleCalendarClient::signIn()
{
    loadSettings();
    if (!isConfigured()) {
        emit errorOccurred(QStringLiteral("calendar_accessory.ini に Google/client_id を設定してください。"));
        return;
    }

    if (!callbackServer_.isListening() && !callbackServer_.listen(QHostAddress::LocalHost, 0)) {
        emit errorOccurred(QStringLiteral("OAuth受信用のローカルポートを開けませんでした。"));
        return;
    }

    callbackPort_ = callbackServer_.serverPort();
    codeVerifier_ = makeCodeVerifier();
    expectedState_ = makeCodeVerifier();

    emit statusChanged(QStringLiteral("ブラウザーでGoogleログインを待っています..."));
    QDesktopServices::openUrl(buildAuthorizationUrl());
}

void GoogleCalendarClient::fetchEvents()
{
    const QDate today = QDate::currentDate();
    fetchEventsForRange(today, today.addDays(days_ - 1));
}

void GoogleCalendarClient::fetchEventsForRange(const QDate &firstDay, const QDate &lastDay)
{
    pendingFirstDay_ = firstDay;
    pendingLastDay_ = lastDay;

    loadSettings();
    if (!isConfigured()) {
        emit errorOccurred(QStringLiteral("calendar_accessory.ini に Google/client_id を設定してください。"));
        return;
    }

    const QDateTime timeMin(firstDay, QTime(0, 0));
    const QDateTime timeMax(lastDay.addDays(1), QTime(0, 0));

    if (accessToken_.isEmpty()) {
        if (!refreshToken_.isEmpty()) {
            refreshAccessToken();
            return;
        }
        emit errorOccurred(QStringLiteral("Googleにログインしてください。"));
        return;
    }

    if (accessTokenExpiry_.isValid() && accessTokenExpiry_ < QDateTime::currentDateTimeUtc().addSecs(60)) {
        refreshAccessToken();
        return;
    }

    requestEvents(timeMin.toUTC(), timeMax.toUTC());
}

void GoogleCalendarClient::saveEvent(const CalendarEvent &event)
{
    loadSettings();
    if (!isConfigured()) {
        emit errorOccurred(QStringLiteral("設定画面で Google クライアントIDを設定してください。"));
        return;
    }
    if (accessToken_.isEmpty()) {
        if (!refreshToken_.isEmpty()) {
            pendingSaveEvent_ = event;
            hasPendingSaveEvent_ = true;
            refreshAccessToken();
            return;
        }
        emit errorOccurred(QStringLiteral("予定を保存するにはGoogleにログインしてください。"));
        return;
    }

    if (accessTokenExpiry_.isValid() && accessTokenExpiry_ < QDateTime::currentDateTimeUtc().addSecs(60)) {
        pendingSaveEvent_ = event;
        hasPendingSaveEvent_ = true;
        refreshAccessToken();
        return;
    }

    sendSaveEventRequest(event);
}

void GoogleCalendarClient::deleteEvent(const CalendarEvent &event)
{
    loadSettings();
    if (!isConfigured()) {
        emit errorOccurred(QStringLiteral("設定画面で Google クライアントIDを設定してください。"));
        return;
    }
    if (event.id.isEmpty()) {
        emit errorOccurred(QStringLiteral("削除対象の予定IDがありません。"));
        return;
    }
    if (accessToken_.isEmpty()) {
        if (!refreshToken_.isEmpty()) {
            pendingDeleteEvent_ = event;
            hasPendingDeleteEvent_ = true;
            refreshAccessToken();
            return;
        }
        emit errorOccurred(QStringLiteral("予定を削除するにはGoogleにログインしてください。"));
        return;
    }

    if (accessTokenExpiry_.isValid() && accessTokenExpiry_ < QDateTime::currentDateTimeUtc().addSecs(60)) {
        pendingDeleteEvent_ = event;
        hasPendingDeleteEvent_ = true;
        refreshAccessToken();
        return;
    }

    sendDeleteEventRequest(event);
}

void GoogleCalendarClient::sendSaveEventRequest(const CalendarEvent &event)
{
    QUrl url;
    const QByteArray method = event.id.isEmpty() ? QByteArray("POST") : QByteArray("PUT");
    if (event.id.isEmpty()) {
        url = QUrl(QString::fromLatin1(kCalendarEndpoint).arg(QString::fromUtf8(QUrl::toPercentEncoding(calendarId_))));
    } else {
        url = QUrl(QString::fromLatin1(kEventEndpoint)
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(calendarId_)),
                 QString::fromUtf8(QUrl::toPercentEncoding(event.id))));
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + accessToken_.toUtf8());

    auto *reply = network_.sendCustomRequest(request, method, QJsonDocument(eventToJson(event)).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, event]() {
        const QByteArray data = reply->readAll();
        const QJsonObject json = QJsonDocument::fromJson(data).object();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            if (statusCode == 401 || statusCode == 403) {
                pendingSaveEvent_ = event;
                hasPendingSaveEvent_ = true;
                emit errorOccurred(QStringLiteral("予定を保存する権限がありません。再ログインすると保存を続行します。"));
                return;
            }
            const QString reason = json.value(QStringLiteral("error")).toObject()
                .value(QStringLiteral("message")).toString();
            emit errorOccurred(reason.isEmpty()
                ? QStringLiteral("予定の保存に失敗しました。再ログインが必要な場合があります。")
                : reason);
            return;
        }

        emit statusChanged(QStringLiteral("予定を保存しました。"));
        emit eventSaved();
    });
}

void GoogleCalendarClient::sendDeleteEventRequest(const CalendarEvent &event)
{
    const QUrl url(QString::fromLatin1(kEventEndpoint)
        .arg(QString::fromUtf8(QUrl::toPercentEncoding(calendarId_)),
             QString::fromUtf8(QUrl::toPercentEncoding(event.id))));

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + accessToken_.toUtf8());

    auto *reply = network_.sendCustomRequest(request, QByteArray("DELETE"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, event]() {
        const QByteArray data = reply->readAll();
        const QJsonObject json = QJsonDocument::fromJson(data).object();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        reply->deleteLater();

        if (error != QNetworkReply::NoError && statusCode != 410) {
            if (statusCode == 401 || statusCode == 403) {
                pendingDeleteEvent_ = event;
                hasPendingDeleteEvent_ = true;
                emit errorOccurred(QStringLiteral("予定を削除する権限がありません。再ログインすると削除を続行します。"));
                return;
            }
            const QString reason = json.value(QStringLiteral("error")).toObject()
                .value(QStringLiteral("message")).toString();
            emit errorOccurred(reason.isEmpty()
                ? QStringLiteral("予定の削除に失敗しました。")
                : reason);
            return;
        }

        emit statusChanged(QStringLiteral("予定を削除しました。"));
        emit eventSaved();
    });
}

void GoogleCalendarClient::handleAuthorizationRequest()
{
    auto *socket = callbackServer_.nextPendingConnection();
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);

    if (!socket->waitForReadyRead(3000)) {
        socket->disconnectFromHost();
        return;
    }

    const QString request = QString::fromUtf8(socket->readAll());
    const QString firstLine = request.section(QLatin1Char('\n'), 0, 0);
    const QString path = firstLine.section(QLatin1Char(' '), 1, 1);
    const QUrl callbackUrl(QStringLiteral("http://127.0.0.1") + path);
    const QUrlQuery query(callbackUrl);

    const QString code = query.queryItemValue(QStringLiteral("code"));
    const QString state = query.queryItemValue(QStringLiteral("state"));
    const QString error = query.queryItemValue(QStringLiteral("error"));

    const QByteArray body = "<html><body><h2>認証が完了しました</h2>このウィンドウを閉じてください。</body></html>";
    socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: ");
    socket->write(QByteArray::number(body.size()));
    socket->write("\r\n\r\n");
    socket->write(body);
    socket->disconnectFromHost();
    callbackServer_.close();

    if (!error.isEmpty()) {
        emit errorOccurred(QStringLiteral("Google認証がキャンセルされました: %1").arg(error));
        return;
    }
    if (code.isEmpty() || state != expectedState_) {
        emit errorOccurred(QStringLiteral("OAuth応答を確認できませんでした。"));
        return;
    }

    exchangeCodeForToken(code);
}

void GoogleCalendarClient::exchangeCodeForToken(const QString &code)
{
    QUrlQuery body;
    body.addQueryItem(QStringLiteral("client_id"), clientId_);
    if (!clientSecret_.isEmpty()) {
        body.addQueryItem(QStringLiteral("client_secret"), clientSecret_);
    }
    body.addQueryItem(QStringLiteral("code"), code);
    body.addQueryItem(QStringLiteral("code_verifier"), codeVerifier_);
    body.addQueryItem(QStringLiteral("redirect_uri"), QStringLiteral("http://127.0.0.1:%1/").arg(callbackPort_));
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));

    QNetworkRequest request(QUrl(QString::fromLatin1(kTokenEndpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    auto *reply = network_.post(request, formEncode(body).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        reply->deleteLater();
        const QJsonObject json = QJsonDocument::fromJson(data).object();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(QStringLiteral("トークン取得に失敗しました。"));
            return;
        }

        accessToken_ = json.value(QStringLiteral("access_token")).toString();
        const QString newRefreshToken = json.value(QStringLiteral("refresh_token")).toString();
        if (!newRefreshToken.isEmpty()) {
            refreshToken_ = newRefreshToken;
        }
        accessTokenExpiry_ = QDateTime::currentDateTimeUtc().addSecs(json.value(QStringLiteral("expires_in")).toInt(3600));
        saveTokens();
        emit statusChanged(QStringLiteral("Googleログイン完了。予定を取得しています..."));
        if (hasPendingSaveEvent_) {
            const CalendarEvent event = pendingSaveEvent_;
            hasPendingSaveEvent_ = false;
            sendSaveEventRequest(event);
            return;
        }
        if (hasPendingDeleteEvent_) {
            const CalendarEvent event = pendingDeleteEvent_;
            hasPendingDeleteEvent_ = false;
            sendDeleteEventRequest(event);
            return;
        }
        if (pendingFirstDay_.isValid() && pendingLastDay_.isValid()) {
            fetchEventsForRange(pendingFirstDay_, pendingLastDay_);
        } else {
            fetchEvents();
        }
    });
}

void GoogleCalendarClient::refreshAccessToken()
{
    if (refreshToken_.isEmpty()) {
        emit errorOccurred(QStringLiteral("再ログインが必要です。"));
        return;
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("client_id"), clientId_);
    if (!clientSecret_.isEmpty()) {
        body.addQueryItem(QStringLiteral("client_secret"), clientSecret_);
    }
    body.addQueryItem(QStringLiteral("refresh_token"), refreshToken_);
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));

    QNetworkRequest request(QUrl(QString::fromLatin1(kTokenEndpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    auto *reply = network_.post(request, formEncode(body).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        reply->deleteLater();
        const QJsonObject json = QJsonDocument::fromJson(data).object();
        if (reply->error() != QNetworkReply::NoError) {
            accessToken_.clear();
            saveTokens();
            emit errorOccurred(QStringLiteral("アクセストークンの更新に失敗しました。再ログインしてください。"));
            return;
        }

        accessToken_ = json.value(QStringLiteral("access_token")).toString();
        accessTokenExpiry_ = QDateTime::currentDateTimeUtc().addSecs(json.value(QStringLiteral("expires_in")).toInt(3600));
        saveTokens();
        if (hasPendingSaveEvent_) {
            const CalendarEvent event = pendingSaveEvent_;
            hasPendingSaveEvent_ = false;
            sendSaveEventRequest(event);
            return;
        }
        if (hasPendingDeleteEvent_) {
            const CalendarEvent event = pendingDeleteEvent_;
            hasPendingDeleteEvent_ = false;
            sendDeleteEventRequest(event);
            return;
        }
        if (pendingFirstDay_.isValid() && pendingLastDay_.isValid()) {
            fetchEventsForRange(pendingFirstDay_, pendingLastDay_);
        } else {
            fetchEvents();
        }
    });
}

void GoogleCalendarClient::requestEvents(const QDateTime &timeMin, const QDateTime &timeMax)
{
    QUrl url(QString::fromLatin1(kCalendarEndpoint).arg(QString::fromUtf8(QUrl::toPercentEncoding(calendarId_))));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("singleEvents"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("orderBy"), QStringLiteral("startTime"));
    query.addQueryItem(QStringLiteral("timeMin"), timeMin.toString(Qt::ISODateWithMs));
    query.addQueryItem(QStringLiteral("timeMax"), timeMax.toString(Qt::ISODateWithMs));
    query.addQueryItem(QStringLiteral("maxResults"), QStringLiteral("250"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + accessToken_.toUtf8());

    emit statusChanged(QStringLiteral("予定を取得しています..."));
    auto *reply = network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        reply->deleteLater();
        const QJsonObject json = QJsonDocument::fromJson(data).object();
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(QStringLiteral("予定の取得に失敗しました。"));
            return;
        }

        QList<CalendarEvent> events;
        for (const QJsonValue &value : json.value(QStringLiteral("items")).toArray()) {
            const QJsonObject item = value.toObject();
            CalendarEvent event;
            event.id = item.value(QStringLiteral("id")).toString();
            event.title = item.value(QStringLiteral("summary")).toString(QStringLiteral("(無題)"));
            event.location = item.value(QStringLiteral("location")).toString();
            event.description = item.value(QStringLiteral("description")).toString();
            bool allDay = false;
            event.start = parseGoogleDateTime(item.value(QStringLiteral("start")).toObject(), &allDay);
            event.end = parseGoogleDateTime(item.value(QStringLiteral("end")).toObject(), nullptr);
            event.allDay = allDay;
            events.append(event);
        }

        emit eventsReady(events);
        emit statusChanged(QStringLiteral("%1件の予定").arg(events.size()));
    });
}

QUrl GoogleCalendarClient::buildAuthorizationUrl() const
{
    QUrl url(QString::fromLatin1(kAuthEndpoint));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId_);
    query.addQueryItem(QStringLiteral("redirect_uri"), QStringLiteral("http://127.0.0.1:%1/").arg(callbackPort_));
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), QStringLiteral("https://www.googleapis.com/auth/calendar.events"));
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    query.addQueryItem(QStringLiteral("code_challenge"), makeCodeChallenge(codeVerifier_));
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("state"), expectedState_);
    url.setQuery(query);
    return url;
}

QString GoogleCalendarClient::makeCodeVerifier() const
{
    QByteArray bytes(32, Qt::Uninitialized);
    auto *begin = reinterpret_cast<quint32 *>(bytes.data());
    auto *end = begin + bytes.size() / int(sizeof(quint32));
    QRandomGenerator::global()->generate(begin, end);
    return base64Url(bytes);
}

QString GoogleCalendarClient::makeCodeChallenge(const QString &verifier) const
{
    return base64Url(QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256));
}

QString GoogleCalendarClient::formEncode(const QUrlQuery &query) const
{
    return QString::fromUtf8(query.toString(QUrl::FullyEncoded).toUtf8());
}

QDateTime GoogleCalendarClient::parseGoogleDateTime(const QJsonObject &object, bool *allDay) const
{
    if (object.contains(QStringLiteral("dateTime"))) {
        if (allDay) {
            *allDay = false;
        }
        return QDateTime::fromString(object.value(QStringLiteral("dateTime")).toString(), Qt::ISODate);
    }

    if (allDay) {
        *allDay = true;
    }
    const QDate date = QDate::fromString(object.value(QStringLiteral("date")).toString(), Qt::ISODate);
    return QDateTime(date, QTime(0, 0));
}

QJsonObject GoogleCalendarClient::eventToJson(const CalendarEvent &event) const
{
    QJsonObject json;
    json.insert(QStringLiteral("summary"), event.title);
    if (!event.location.isEmpty()) {
        json.insert(QStringLiteral("location"), event.location);
    }
    if (!event.description.isEmpty()) {
        json.insert(QStringLiteral("description"), event.description);
    }

    QJsonObject start;
    QJsonObject end;
    if (event.allDay) {
        start.insert(QStringLiteral("date"), event.start.date().toString(Qt::ISODate));
        end.insert(QStringLiteral("date"), event.end.date().addDays(1).toString(Qt::ISODate));
    } else {
        const QString timeZone = QString::fromUtf8(QTimeZone::systemTimeZoneId());
        start.insert(QStringLiteral("dateTime"), event.start.toString(Qt::ISODate));
        start.insert(QStringLiteral("timeZone"), timeZone);
        end.insert(QStringLiteral("dateTime"), event.end.toString(Qt::ISODate));
        end.insert(QStringLiteral("timeZone"), timeZone);
    }
    json.insert(QStringLiteral("start"), start);
    json.insert(QStringLiteral("end"), end);
    return json;
}
