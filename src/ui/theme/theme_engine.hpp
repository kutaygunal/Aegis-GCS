#pragma once

#include <QWidget>
#include <QPalette>
#include <QApplication>

namespace aegis::ui {

/**
 * @brief Manages QSS stylesheets and color tokens for operator safety.
 *
 * Safety-critical colors (red/amber/green) are locked tokens to prevent
 * operator misinterpretation across plugins.
 */
class ThemeEngine {
public:
    static void applyDarkTheme(QWidget* target = nullptr);
    static void applyLightTheme(QWidget* target = nullptr);
    static QString styleSheet();

    // Safety-critical color tokens (locked — never overridden by plugins)
    static constexpr const char* COLOR_GOOD      = "#00C853";   // Green
    static constexpr const char* COLOR_WARNING   = "#FFD600";   // Amber
    static constexpr const char* COLOR_CRITICAL  = "#FF1744";   // Red
    static constexpr const char* COLOR_INFO      = "#2979FF";   // Blue
    static constexpr const char* COLOR_TEXT      = "#ECEFF1";   // Off-white
    static constexpr const char* COLOR_SURFACE   = "#263238";   // Dark grey
    static constexpr const char* COLOR_BACKGROUND = "#1a1a1a";  // Black-ish
};

} // namespace aegis::ui
