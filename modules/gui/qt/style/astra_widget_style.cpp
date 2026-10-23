/*****************************************************************************
 * Copyright (C) 2026 Astra Media contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "astra_widget_style.hpp"

#include <cstdlib>
#include <cstring>

#include <vlc_common.h>
#include <vlc_variables.h>

#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QMenu>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

#include "maininterface/mainctx.hpp"
#include "util/color_scheme_model.hpp"

namespace {

QString rgba(const QColor& c)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red()).arg(c.green()).arg(c.blue()).arg(QString::number(c.alphaF(), 'f', 3));
}

QColor alpha(QColor c, qreal a)
{
    c.setAlphaF(a);
    return c;
}

// Menus get rounded corners (they need a translucent, frameless window) and
// the accept button of every dialog becomes the blue primary button
class MenuPolisher : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() == QEvent::Polish)
        {
            if (auto menu = qobject_cast<QMenu*>(obj))
            {
                if (!menu->testAttribute(Qt::WA_TranslucentBackground))
                {
                    menu->setAttribute(Qt::WA_TranslucentBackground);
                    menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint
                                         | Qt::NoDropShadowWindowHint);
                }
            }
            else if (auto button = qobject_cast<QPushButton*>(obj))
            {
                if (auto box = qobject_cast<QDialogButtonBox*>(button->parentWidget()))
                {
                    const auto role = box->buttonRole(button);
                    if ((role == QDialogButtonBox::AcceptRole || role == QDialogButtonBox::YesRole)
                        && !button->property("astraPrimary").isValid())
                    {
                        button->setProperty("astraPrimary", true);
                        button->style()->unpolish(button);
                        button->style()->polish(button);
                    }
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

} // namespace

AstraWidgetStyle::AstraWidgetStyle(MainCtx* ctx, QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
}

void AstraWidgetStyle::install()
{
    // classic widgets get the same look on every system
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        QApplication::setStyle(fusion);

    QFont font = QApplication::font();
#ifdef _WIN32
    font.setFamilies({ QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI") });
#endif
    if (font.pointSizeF() > 0 && font.pointSizeF() < 9.5)
        font.setPointSizeF(9.5);
    QApplication::setFont(font, "QWidget");

    qApp->installEventFilter(new MenuPolisher(this));

    if (m_ctx && m_ctx->getColorScheme())
        connect(m_ctx->getColorScheme(), &ColorSchemeModel::currentChanged, this, &AstraWidgetStyle::apply);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QStyleHints* hints = QGuiApplication::styleHints())
        connect(hints, &QStyleHints::colorSchemeChanged, this, &AstraWidgetStyle::apply);
#endif

    apply();
}

bool AstraWidgetStyle::isDark() const
{
    const ColorSchemeModel* model = m_ctx ? m_ctx->getColorScheme() : nullptr;
    const auto scheme = model ? model->currentScheme() : ColorSchemeModel::System;

    if (scheme == ColorSchemeModel::Day)
        return false;
    if (scheme == ColorSchemeModel::Night)
        return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QStyleHints* hints = QGuiApplication::styleHints())
        return hints->colorScheme() == Qt::ColorScheme::Dark;
#endif
    return false;
}

void AstraWidgetStyle::apply()
{
    const bool dark = isDark();
    if (m_applied && dark == m_lastDark)
        return;
    m_applied = true;
    m_lastDark = dark;

    QColor accent = dark ? QColor(0x0A, 0x84, 0xFF) : QColor(0x00, 0x7A, 0xFF);
    if (m_ctx)
    {
        char* custom = var_InheritString(m_ctx->getIntf(), "qt-accent-color");
        if (custom)
        {
            QColor c(QString::fromUtf8(custom));
            if (strcmp(custom, "default") && strcmp(custom, "system") && c.isValid())
                accent = c;
            free(custom);
        }
    }

    QPalette p;
    const QColor window = dark ? QColor("#1E1E20") : QColor("#F2F2F7");
    const QColor base = dark ? QColor("#2C2C2E") : QColor("#FFFFFF");
    const QColor text = dark ? QColor("#FFFFFF") : QColor("#1C1C1E");
    const QColor secondary = dark ? QColor("#98989D") : QColor("#6E6E73");
    const QColor button = dark ? QColor("#3A3A3C") : QColor("#FFFFFF");

    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, dark ? QColor("#323234") : QColor("#F7F7FA"));
    p.setColor(QPalette::ToolTipBase, dark ? QColor("#3A3A3C") : QColor("#FFFFFF"));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, secondary);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Light, dark ? QColor("#48484A") : QColor("#FFFFFF"));
    p.setColor(QPalette::Midlight, dark ? QColor("#3A3A3C") : QColor("#E5E5EA"));
    p.setColor(QPalette::Mid, dark ? QColor("#2C2C2E") : QColor("#D1D1D6"));
    p.setColor(QPalette::Dark, dark ? QColor("#1C1C1E") : QColor("#AEAEB2"));
    p.setColor(QPalette::Shadow, QColor(0, 0, 0, 120));
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::LinkVisited, accent.darker(115));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    p.setColor(QPalette::Accent, accent);
#endif
    const QColor disabled = dark ? QColor("#636366") : QColor("#AEAEB2");
    p.setColor(QPalette::Disabled, QPalette::Text, disabled);
    p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight, dark ? QColor("#48484A") : QColor("#D1D1D6"));

    QApplication::setPalette(p, "QWidget");
    qApp->setStyleSheet(styleSheet(dark, accent));
}

QString AstraWidgetStyle::styleSheet(bool dark, const QColor& accent)
{
    const QString v = dark ? QStringLiteral("dark") : QStringLiteral("light");

    const QString win = dark ? "#1E1E20" : "#F2F2F7";
    const QString card = dark ? "#2C2C2E" : "#FFFFFF";
    const QString control = dark ? "#3A3A3C" : "#FFFFFF";
    const QString controlHover = dark ? "#48484A" : "#F5F5F7";
    const QString controlPressed = dark ? "#545456" : "#E8E8ED";
    const QString field = dark ? "#1C1C1E" : "#FFFFFF";
    const QString popup = dark ? "#2C2C2E" : "#FFFFFF";
    const QString text = dark ? "#FFFFFF" : "#1C1C1E";
    const QString secondary = dark ? "#98989D" : "#6E6E73";
    const QString disabled = dark ? "#636366" : "#AEAEB2";
    const QString border = dark ? "rgba(255, 255, 255, 0.10)" : "rgba(0, 0, 0, 0.10)";
    const QString borderStrong = dark ? "rgba(255, 255, 255, 0.22)" : "rgba(0, 0, 0, 0.22)";
    const QString softHover = dark ? "rgba(255, 255, 255, 0.07)" : "rgba(0, 0, 0, 0.05)";
    const QString track = dark ? "rgba(255, 255, 255, 0.16)" : "rgba(0, 0, 0, 0.12)";
    const QString scroll = dark ? "rgba(255, 255, 255, 0.28)" : "rgba(0, 0, 0, 0.25)";
    const QString scrollHover = dark ? "rgba(255, 255, 255, 0.45)" : "rgba(0, 0, 0, 0.40)";
    const QString segBg = dark ? "rgba(118, 118, 128, 0.24)" : "rgba(118, 118, 128, 0.12)";
    const QString segSel = dark ? "#636366" : "#FFFFFF";
    const QString acc = accent.name();
    const QString accHover = accent.lighter(110).name();
    const QString accPressed = accent.darker(115).name();
    const QString accSoft = rgba(alpha(accent, dark ? 0.24 : 0.14));

    QString css = QStringLiteral(R"CSS(
QDialog, QMainWindow, QWizard { background: %WIN%; }
QWidget { color: %TEXT%; }
QLabel { background: transparent; }
QToolTip { background: %POPUP%; color: %TEXT%; border: 1px solid %BORDER%; border-radius: 6px; padding: 4px 8px; }

QGroupBox {
    background: %CARD%;
    border: 1px solid %BORDER%;
    border-radius: 12px;
    margin-top: 24px;
    padding: 14px 12px 12px 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 6px;
    padding: 0 4px 4px 4px;
    color: %SECONDARY%;
    font-weight: 600;
}
QGroupBox::indicator { width: 16px; height: 16px; }

QScrollArea, QStackedWidget { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }

QPushButton {
    background: %CONTROL%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 5px 16px;
    min-height: 20px;
}
QPushButton:hover { background: %CONTROL_HOVER%; }
QPushButton:pressed { background: %CONTROL_PRESSED%; }
QPushButton:checked { background: %ACC_SOFT%; border-color: %ACC%; }
QPushButton:disabled { color: %DISABLED%; }
QPushButton[astraPrimary="true"] {
    background: %ACC%;
    color: #FFFFFF;
    border: 1px solid %ACC_PRESSED%;
    font-weight: 600;
}
QPushButton[astraPrimary="true"]:hover { background: %ACC_HOVER%; }
QPushButton[astraPrimary="true"]:pressed { background: %ACC_PRESSED%; }
QPushButton:flat { background: transparent; border: none; color: %ACC%; }
QPushButton::menu-indicator { image: url(:/astra/chevron-down-%V%.svg); width: 12px; subcontrol-position: right center; right: 6px; }

QToolButton {
    background: transparent;
    border: none;
    border-radius: 8px;
    padding: 4px;
    color: %TEXT%;
}
QToolButton:hover { background: %SOFT_HOVER%; }
QToolButton:pressed { background: %CONTROL_PRESSED%; }
QToolButton:checked { background: %ACC_SOFT%; }
QToolButton#toolbarEditor {
    background: %CONTROL%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 5px 16px;
}
QToolButton#toolbarEditor:hover { background: %CONTROL_HOVER%; }
QToolButton#playButton {
    background: %ACC%;
    color: #FFFFFF;
    border: 1px solid %ACC_PRESSED%;
    border-radius: 8px;
    padding: 5px 30px 5px 16px;
    font-weight: 600;
}
QToolButton#playButton:hover { background: %ACC_HOVER%; }
QToolButton#playButton:pressed { background: %ACC_PRESSED%; }
QToolButton#playButton:disabled { background: %DISABLED%; border-color: %DISABLED%; }
QToolButton#playButton::menu-button {
    border: none;
    border-left: 1px solid rgba(255, 255, 255, 0.35);
    width: 22px;
    border-top-right-radius: 8px;
    border-bottom-right-radius: 8px;
}
QToolButton#playButton::menu-arrow { image: url(:/astra/chevron-down-white.svg); width: 12px; height: 12px; }
QToolButton::menu-indicator { image: url(:/astra/chevron-down-%V%.svg); width: 10px; height: 10px; subcontrol-position: right center; subcontrol-origin: padding; right: 4px; }

QComboBox, QFontComboBox {
    background: %CONTROL%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 7px;
    padding: 4px 28px 4px 10px;
    min-height: 20px;
}
QComboBox:hover { border-color: %BORDER_STRONG%; }
QComboBox:focus, QComboBox:on { border: 1px solid %ACC%; }
QComboBox:disabled { color: %DISABLED%; }
QComboBox:editable { background: %FIELD%; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 26px; border: none; background: transparent; }
QComboBox::down-arrow { image: url(:/astra/chevron-down-%V%.svg); width: 14px; height: 14px; }
QComboBox QAbstractItemView {
    background: %POPUP%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 4px;
    outline: 0;
    selection-background-color: %ACC%;
    selection-color: #FFFFFF;
}
QComboBox QAbstractItemView::item { min-height: 24px; padding: 2px 8px; border-radius: 5px; }

QLineEdit, QSpinBox, QDoubleSpinBox, QTimeEdit, QDateEdit, QDateTimeEdit, QPlainTextEdit, QTextEdit, QTextBrowser, QKeySequenceEdit {
    background: %FIELD%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 7px;
    padding: 4px 8px;
    min-height: 20px;
    selection-background-color: %ACC%;
    selection-color: #FFFFFF;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTimeEdit:focus, QDateEdit:focus, QDateTimeEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border: 1px solid %ACC%;
}
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QTimeEdit:disabled { color: %DISABLED%; }
QLineEdit[readOnly="true"] { background: %WIN%; }
QSpinBox, QDoubleSpinBox, QTimeEdit, QDateEdit, QDateTimeEdit { padding-right: 22px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
    subcontrol-origin: border;
    width: 20px;
    border: none;
    background: transparent;
}
QAbstractSpinBox::up-button { subcontrol-position: top right; margin-top: 2px; }
QAbstractSpinBox::down-button { subcontrol-position: bottom right; margin-bottom: 2px; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background: %SOFT_HOVER%; border-radius: 4px; }
QAbstractSpinBox::up-arrow { image: url(:/astra/chevron-up-%V%.svg); width: 11px; height: 11px; }
QAbstractSpinBox::down-arrow { image: url(:/astra/chevron-down-%V%.svg); width: 11px; height: 11px; }

QCheckBox, QRadioButton { spacing: 8px; color: %TEXT%; background: transparent; }
QCheckBox:disabled, QRadioButton:disabled { color: %DISABLED%; }
QCheckBox::indicator, QGroupBox::indicator, QTreeView::indicator, QListView::indicator, QTableView::indicator {
    width: 16px;
    height: 16px;
    border-radius: 5px;
    border: 1px solid %BORDER_STRONG%;
    background: %FIELD%;
}
QCheckBox::indicator:hover { border-color: %ACC%; }
QCheckBox::indicator:checked, QGroupBox::indicator:checked, QTreeView::indicator:checked, QListView::indicator:checked, QTableView::indicator:checked {
    background: %ACC%;
    border-color: %ACC%;
    image: url(:/astra/check.svg);
}
QCheckBox::indicator:indeterminate, QTreeView::indicator:indeterminate {
    background: %ACC%;
    border-color: %ACC%;
    image: url(:/astra/dash.svg);
}
QCheckBox::indicator:disabled { background: %WIN%; border-color: %BORDER%; }
QCheckBox::indicator:checked:disabled { background: %DISABLED%; border-color: %DISABLED%; }
QRadioButton::indicator {
    width: 16px;
    height: 16px;
    border-radius: 9px;
    border: 1px solid %BORDER_STRONG%;
    background: %FIELD%;
}
QRadioButton::indicator:hover { border-color: %ACC%; }
QRadioButton::indicator:checked {
    width: 8px;
    height: 8px;
    border: 5px solid %ACC%;
    background: #FFFFFF;
}

QSlider { background: transparent; }
QSlider::groove:horizontal { height: 4px; background: %TRACK%; border-radius: 2px; }
QSlider::sub-page:horizontal { background: %ACC%; border-radius: 2px; }
QSlider::handle:horizontal {
    background: #FFFFFF;
    border: 1px solid rgba(0, 0, 0, 0.20);
    width: 18px;
    height: 18px;
    margin: -8px 0;
    border-radius: 10px;
}
QSlider::groove:vertical { width: 4px; background: %TRACK%; border-radius: 2px; }
QSlider::add-page:vertical { background: %ACC%; border-radius: 2px; }
QSlider::handle:vertical {
    background: #FFFFFF;
    border: 1px solid rgba(0, 0, 0, 0.20);
    width: 18px;
    height: 18px;
    margin: 0 -8px;
    border-radius: 10px;
}
QSlider::handle:hover { border-color: %ACC%; }

QProgressBar {
    background: %TRACK%;
    border: none;
    border-radius: 4px;
    min-height: 8px;
    max-height: 8px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk { background: %ACC%; border-radius: 4px; }

QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }
QScrollBar::handle:vertical { background: %SCROLL%; border-radius: 4px; min-height: 32px; margin: 0 2px; }
QScrollBar::handle:horizontal { background: %SCROLL%; border-radius: 4px; min-width: 32px; margin: 2px 0; }
QScrollBar::handle:hover { background: %SCROLL_HOVER%; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; border: none; background: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QTabWidget::pane {
    background: %CARD%;
    border: 1px solid %BORDER%;
    border-radius: 12px;
    top: -1px;
}
QTabWidget::tab-bar { alignment: center; }
QTabBar { background: transparent; }
QTabBar::tab {
    background: transparent;
    color: %SECONDARY%;
    padding: 6px 16px;
    margin: 2px 2px 6px 2px;
    border-radius: 7px;
    border: none;
}
QTabBar::tab:hover:!selected { color: %TEXT%; background: %SOFT_HOVER%; }
QTabBar::tab:selected { background: %SEG_SEL%; color: %TEXT%; font-weight: 600; }
QTabBar::close-button { subcontrol-position: right; }

QTreeView, QListView, QTableView, QTreeWidget, QListWidget, QTableWidget, QColumnView {
    background: %CARD%;
    alternate-background-color: %ALT%;
    border: 1px solid %BORDER%;
    border-radius: 10px;
    outline: 0;
    padding: 4px;
    selection-background-color: %ACC%;
    selection-color: #FFFFFF;
}
QTreeView { show-decoration-selected: 1; }
QListView::item { padding: 4px 6px; border-radius: 6px; }
QTreeView::item { padding: 4px 4px; border: none; }
QTableView::item { padding: 2px 4px; border: none; }
QTreeView::item:hover:!selected, QListView::item:hover:!selected, QTableView::item:hover:!selected { background: %SOFT_HOVER%; }
QTreeView::item:selected, QListView::item:selected, QTableView::item:selected { background: %ACC%; color: #FFFFFF; }
QTreeView::branch:selected { background: %ACC%; }
QTreeView::branch:hover:!selected { background: %SOFT_HOVER%; }
QTreeView::branch { background: transparent; border-image: none; }
QTreeView::branch:has-children:!has-siblings:closed, QTreeView::branch:closed:has-children:has-siblings { image: url(:/astra/chevron-right-%V%.svg); }
QTreeView::branch:open:has-children:!has-siblings, QTreeView::branch:open:has-children:has-siblings { image: url(:/astra/chevron-down-%V%.svg); }
QHeaderView { background: transparent; }
QHeaderView::section {
    background: %CARD%;
    color: %SECONDARY%;
    border: none;
    border-bottom: 1px solid %BORDER%;
    padding: 6px 8px;
    font-weight: 600;
}
QTableCornerButton::section { background: %CARD%; border: none; }

QMenuBar { background: %WIN%; border: none; }
QMenuBar::item { background: transparent; padding: 4px 10px; border-radius: 6px; }
QMenuBar::item:selected { background: %SOFT_HOVER%; }
QMenu {
    background: %POPUP%;
    border: 1px solid %BORDER_STRONG%;
    border-radius: 10px;
    padding: 6px;
}
QMenu::item { padding: 6px 28px 6px 12px; border-radius: 6px; color: %TEXT%; background: transparent; }
QMenu::item:selected { background: %ACC%; color: #FFFFFF; }
QMenu::item:disabled { color: %DISABLED%; }
QMenu::separator { height: 1px; background: %BORDER%; margin: 5px 8px; }
QMenu::icon { padding-left: 8px; }
QMenu::right-arrow { image: url(:/astra/chevron-right-%V%.svg); width: 12px; height: 12px; }

QSplitter::handle { background: transparent; }
QSplitter::handle:horizontal { width: 6px; }
QSplitter::handle:vertical { height: 6px; }
QSizeGrip { background: transparent; }
QStatusBar { background: %WIN%; color: %SECONDARY%; }
QDialogButtonBox { button-layout: 0; }

/* ── Preferences ───────────────────────────────────────────── */
#astraPrefsTitle { color: %TEXT%; font-weight: 700; padding: 6px 2px 2px 2px; }
#astraCategoryBar QToolButton {
    color: %SECONDARY%;
    padding: 8px 6px 6px 6px;
    border-radius: 12px;
    font-weight: 500;
}
#astraCategoryBar QToolButton:hover { background: %SOFT_HOVER%; color: %TEXT%; }
#astraCategoryBar QToolButton:checked { background: %ACC_SOFT%; color: %TEXT%; font-weight: 600; }
QFrame#astraSegmented {
    background: %SEG_BG%;
    border: none;
    border-radius: 9px;
    margin: 0;
    padding: 2px;
}
QFrame#astraSegmented QRadioButton {
    padding: 5px 16px;
    border-radius: 7px;
    color: %TEXT%;
    background: transparent;
}
QFrame#astraSegmented QRadioButton:hover { background: %SOFT_HOVER%; }
QFrame#astraSegmented QRadioButton:checked { background: %SEG_SEL%; font-weight: 600; }
QFrame#astraSegmented QRadioButton::indicator { width: 0; height: 0; border: none; background: transparent; image: none; }
)CSS");

    css.replace("%WIN%", win)
       .replace("%CARD%", card)
       .replace("%CONTROL_HOVER%", controlHover)
       .replace("%CONTROL_PRESSED%", controlPressed)
       .replace("%CONTROL%", control)
       .replace("%FIELD%", field)
       .replace("%POPUP%", popup)
       .replace("%TEXT%", text)
       .replace("%SECONDARY%", secondary)
       .replace("%DISABLED%", disabled)
       .replace("%BORDER_STRONG%", borderStrong)
       .replace("%BORDER%", border)
       .replace("%SOFT_HOVER%", softHover)
       .replace("%TRACK%", track)
       .replace("%SCROLL_HOVER%", scrollHover)
       .replace("%SCROLL%", scroll)
       .replace("%SEG_BG%", segBg)
       .replace("%SEG_SEL%", segSel)
       .replace("%ALT%", dark ? "#2F2F31" : "#FAFAFC")
       .replace("%ACC_HOVER%", accHover)
       .replace("%ACC_PRESSED%", accPressed)
       .replace("%ACC_SOFT%", accSoft)
       .replace("%ACC%", acc)
       .replace("%V%", v);
    return css;
}
