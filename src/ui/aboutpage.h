#ifndef ABOUTPAGE_H
#define ABOUTPAGE_H

#include <QWidget>
#include <QLabel>

class ConfigManager;
class HightLabel : public QLabel
{
    Q_OBJECT
public:
    HightLabel(const QString &text, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
};

class AboutPage : public QWidget
{
    Q_OBJECT
public:
    explicit AboutPage(ConfigManager *config, QWidget *parent = nullptr);
private:
    ConfigManager *m_config;
    QLabel *m_storageLabel;
};

#endif // ABOUTPAGE_H
