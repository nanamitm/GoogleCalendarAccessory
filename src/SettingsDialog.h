#pragma once

#include <QDialog>

class QLineEdit;
class QCheckBox;
class QSlider;
class QSpinBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void accept() override;

private:
    QWidget *createGoogleTab();
    QWidget *createDisplayTab();
    QWidget *createStartupTab();
    void loadSettings();
    QString settingsPath() const;
    QString startupRegistryPath() const;
    bool isStartupRegistered() const;
    void setStartupRegistered(bool registered) const;

    QLineEdit *clientIdEdit_ = nullptr;
    QLineEdit *clientSecretEdit_ = nullptr;
    QLineEdit *calendarIdEdit_ = nullptr;
    QSpinBox *daysEdit_ = nullptr;
    QSlider *opacitySlider_ = nullptr;
    QSpinBox *opacitySpin_ = nullptr;
    QCheckBox *alwaysOnTopEdit_ = nullptr;
    QCheckBox *resetPositionEdit_ = nullptr;
    QCheckBox *startupEdit_ = nullptr;
};
