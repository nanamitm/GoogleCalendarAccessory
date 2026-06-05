#include "CalendarWidget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("GoogleCalendarAccessory"));
    QApplication::setOrganizationName(QStringLiteral("Codex"));

    CalendarWidget widget;
    widget.show();

    return app.exec();
}
