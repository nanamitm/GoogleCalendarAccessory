#include "SettingsDialog.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
constexpr auto kStartupValueName = "GoogleCalendarAccessory";
}

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("設定"));
    resize(540, 330);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->addTab(createGoogleTab(), QStringLiteral("Google"));
    tabs->addTab(createDisplayTab(), QStringLiteral("表示"));
    tabs->addTab(createStartupTab(), QStringLiteral("起動"));
    layout->addWidget(tabs, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("キャンセル"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);

    setStyleSheet(QStringLiteral(R"(
        QDialog, QWidget {
            background: #20262c;
            color: #f7f8fb;
            font-family: "Yu Gothic UI", "Meiryo", sans-serif;
            font-size: 13px;
        }
        QTabWidget::pane {
            border: 1px solid #3a4652;
            border-radius: 6px;
        }
        QTabBar::tab {
            background: #151a20;
            border: 1px solid #3a4652;
            border-bottom: 0;
            padding: 7px 16px;
        }
        QTabBar::tab:selected {
            background: #2c3742;
        }
        QLineEdit, QSpinBox {
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
        QPushButton:hover {
            background: #55a1e2;
        }
    )"));

    loadSettings();
}

QWidget *SettingsDialog::createGoogleTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);

    auto *hint = new QLabel(QStringLiteral("Google Cloud Consoleで作成したデスクトップアプリ用OAuthクライアントを設定します。"), tab);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *form = new QFormLayout();
    clientIdEdit_ = new QLineEdit(tab);
    clientIdEdit_->setPlaceholderText(QStringLiteral("YOUR_CLIENT_ID.apps.googleusercontent.com"));

    clientSecretEdit_ = new QLineEdit(tab);
    clientSecretEdit_->setPlaceholderText(QStringLiteral("通常は空でOK"));

    calendarIdEdit_ = new QLineEdit(tab);
    calendarIdEdit_->setPlaceholderText(QStringLiteral("primary"));

    daysEdit_ = new QSpinBox(tab);
    daysEdit_->setRange(1, 60);
    daysEdit_->setSuffix(QStringLiteral(" 日"));

    form->addRow(QStringLiteral("クライアントID"), clientIdEdit_);
    form->addRow(QStringLiteral("クライアントシークレット"), clientSecretEdit_);
    form->addRow(QStringLiteral("カレンダーID"), calendarIdEdit_);
    form->addRow(QStringLiteral("取得日数"), daysEdit_);
    layout->addLayout(form);
    layout->addStretch(1);
    return tab;
}

QWidget *SettingsDialog::createDisplayTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    auto *form = new QFormLayout();

    auto *opacityRow = new QWidget(tab);
    auto *opacityLayout = new QHBoxLayout(opacityRow);
    opacityLayout->setContentsMargins(0, 0, 0, 0);
    opacitySlider_ = new QSlider(Qt::Horizontal, opacityRow);
    opacitySlider_->setRange(35, 100);
    opacitySpin_ = new QSpinBox(opacityRow);
    opacitySpin_->setRange(35, 100);
    opacitySpin_->setSuffix(QStringLiteral("%"));
    opacityLayout->addWidget(opacitySlider_, 1);
    opacityLayout->addWidget(opacitySpin_);
    connect(opacitySlider_, &QSlider::valueChanged, opacitySpin_, &QSpinBox::setValue);
    connect(opacitySpin_, qOverload<int>(&QSpinBox::valueChanged), opacitySlider_, &QSlider::setValue);

    alwaysOnTopEdit_ = new QCheckBox(QStringLiteral("常に手前に表示"), tab);
    resetPositionEdit_ = new QCheckBox(QStringLiteral("次回保存時にウィンドウ位置をリセット"), tab);

    form->addRow(QStringLiteral("透明度"), opacityRow);
    form->addRow(QString(), alwaysOnTopEdit_);
    form->addRow(QString(), resetPositionEdit_);
    layout->addLayout(form);
    layout->addStretch(1);
    return tab;
}

QWidget *SettingsDialog::createStartupTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    startupEdit_ = new QCheckBox(QStringLiteral("Windows起動時に開始"), tab);
    layout->addWidget(startupEdit_);
    auto *hint = new QLabel(QStringLiteral("現在の実行ファイルをユーザーのスタートアップに登録します。管理者権限は不要です。"), tab);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    layout->addStretch(1);
    return tab;
}

void SettingsDialog::accept()
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Google/client_id"), clientIdEdit_->text().trimmed());
    settings.setValue(QStringLiteral("Google/client_secret"), clientSecretEdit_->text().trimmed());
    settings.setValue(QStringLiteral("Google/calendar_id"), calendarIdEdit_->text().trimmed().isEmpty()
        ? QStringLiteral("primary")
        : calendarIdEdit_->text().trimmed());
    settings.setValue(QStringLiteral("Google/days"), daysEdit_->value());
    settings.setValue(QStringLiteral("Display/opacity"), opacitySpin_->value());
    settings.setValue(QStringLiteral("Display/always_on_top"), alwaysOnTopEdit_->isChecked());
    if (resetPositionEdit_->isChecked()) {
        settings.setValue(QStringLiteral("Display/reset_position"), true);
    }
    settings.sync();

    setStartupRegistered(startupEdit_->isChecked());
    QDialog::accept();
}

void SettingsDialog::loadSettings()
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    clientIdEdit_->setText(settings.value(QStringLiteral("Google/client_id")).toString());
    clientSecretEdit_->setText(settings.value(QStringLiteral("Google/client_secret")).toString());
    calendarIdEdit_->setText(settings.value(QStringLiteral("Google/calendar_id"), QStringLiteral("primary")).toString());
    daysEdit_->setValue(settings.value(QStringLiteral("Google/days"), 7).toInt());

    const int opacity = settings.value(QStringLiteral("Display/opacity"), 100).toInt();
    opacitySlider_->setValue(qBound(35, opacity, 100));
    opacitySpin_->setValue(qBound(35, opacity, 100));
    alwaysOnTopEdit_->setChecked(settings.value(QStringLiteral("Display/always_on_top"), true).toBool());
    resetPositionEdit_->setChecked(false);
    startupEdit_->setChecked(isStartupRegistered());
}

QString SettingsDialog::settingsPath() const
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/calendar_accessory.ini");
}

QString SettingsDialog::startupRegistryPath() const
{
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

bool SettingsDialog::isStartupRegistered() const
{
    QSettings registry(startupRegistryPath(), QSettings::NativeFormat);
    return registry.contains(QString::fromLatin1(kStartupValueName));
}

void SettingsDialog::setStartupRegistered(bool registered) const
{
    QSettings registry(startupRegistryPath(), QSettings::NativeFormat);
    if (registered) {
        registry.setValue(QString::fromLatin1(kStartupValueName),
                          QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    } else {
        registry.remove(QString::fromLatin1(kStartupValueName));
    }
    registry.sync();
}
