#ifndef SETTINGSWIDGETS_H
#define SETTINGSWIDGETS_H

#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QStyle>
#include <QMessageBox>
#include <QSize>

static const QStringList kBoolItems = {"General", "On", "Off"};

// Plain numeric spin box.
static QSpinBox *makeSpinBox(int min, int max, int val, QWidget *parent) {
    auto *sb = new QSpinBox(parent);
    sb->setRange(min, max);
    sb->setValue(val);
    return sb;
}

// Numeric spin box where 0 means "not overridden" (rendered as "General").
static QSpinBox *makeOverrideSpinBox(int min, int max, int val, QWidget *parent) {
    auto *sb = new QSpinBox(parent);
    sb->setRange(min, max);
    sb->setValue(val);
    sb->setSpecialValueText("General");
    return sb;
}

// General/On/Off combo; unknown value falls back to "General".
static QComboBox *makeBoolCombo(const QString &current, QWidget *parent) {
    auto *cb = new QComboBox(parent);
    cb->addItems(kBoolItems);
    int idx = kBoolItems.indexOf(current);
    cb->setCurrentIndex(idx < 0 ? 0 : idx);
    return cb;
}

static QString boolToCombo(bool val) { return val ? "On" : "Off"; }

// Flat warning-icon button showing an info dialog on click.
static QPushButton *makeInfoButton(const QString &title, const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(parent);
    btn->setIcon(btn->style()->standardIcon(QStyle::SP_MessageBoxWarning));
    btn->setIconSize(QSize(16, 16));
    btn->setFixedSize(20, 20);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    QObject::connect(btn, &QPushButton::clicked, parent, [title, text]() {
        QMessageBox::information(nullptr, title, text);
    });
    return btn;
}

#endif // SETTINGSWIDGETS_H