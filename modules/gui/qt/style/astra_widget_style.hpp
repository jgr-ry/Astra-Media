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
#ifndef ASTRA_WIDGET_STYLE_HPP
#define ASTRA_WIDGET_STYLE_HPP

#include <QColor>
#include <QObject>

class MainCtx;

/**
 * Astra Media look for the classic Qt Widgets dialogs (preferences, media
 * information, effects, open dialogs...): Fusion style, an Apple-like palette
 * and a style sheet with rounded controls. It follows the interface color
 * scheme (system, light or dark).
 */
class AstraWidgetStyle : public QObject
{
    Q_OBJECT
public:
    explicit AstraWidgetStyle(MainCtx* ctx, QObject* parent = nullptr);

    // apply now, and again whenever the color scheme changes
    void install();

    static QString styleSheet(bool dark, const QColor& accent);

private:
    void apply();
    bool isDark() const;

    MainCtx* m_ctx = nullptr;
    bool m_applied = false;
    bool m_lastDark = false;
};

#endif // ASTRA_WIDGET_STYLE_HPP
