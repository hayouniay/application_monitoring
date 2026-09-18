#ifndef QT_THEME_H
#define QT_THEME_H

#include <QString>

enum class NeoTheme { Light, Dark };

/*
 * Returns the complete Qt stylesheet (QSS) for the requested theme.
 */
QString neoThemeStyleSheet(NeoTheme theme);

/*
 * Stable string identifier used for persisting the theme choice
 * (e.g. via QSettings).
 */
QString neoThemeName(NeoTheme theme);

/*
 * Parses a theme name back into a NeoTheme value.
 * Defaults to NeoTheme::Light for unknown/empty input.
 */
NeoTheme neoThemeFromName(const QString &name);

#endif /* QT_THEME_H */
