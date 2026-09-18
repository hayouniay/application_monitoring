#include "qt/qt_theme.h"

namespace {

const char *const kLightStyleSheet = R"(
QMainWindow, QDialog {
    background-color: #f4f6f9;
    color: #1c2430;
}

QWidget {
    color: #1c2430;
    font-size: 13px;
}

QToolBar {
    background-color: #ffffff;
    border-bottom: 1px solid #e0e4ea;
    padding: 6px 10px;
    spacing: 8px;
}

QStatusBar {
    background-color: #ffffff;
    border-top: 1px solid #e0e4ea;
    color: #5a6472;
}

QGroupBox {
    background-color: #ffffff;
    border: 1px solid #e0e4ea;
    border-radius: 8px;
    margin-top: 14px;
    padding: 14px 10px 10px 10px;
    font-weight: 600;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
    color: #3a4453;
}

QLabel {
    background: transparent;
}

QPushButton {
    background-color: #ffffff;
    border: 1px solid #d3d9e2;
    border-radius: 6px;
    padding: 6px 14px;
    color: #1c2430;
    font-weight: 500;
}

QPushButton:hover {
    background-color: #eef2f8;
    border-color: #b9c2d0;
}

QPushButton:pressed {
    background-color: #e2e8f1;
}

QPushButton:disabled {
    color: #9aa3b1;
    background-color: #f1f3f6;
}

QPushButton#themeToggleButton {
    background-color: #1c2430;
    color: #f4f6f9;
    border: 1px solid #1c2430;
}

QPushButton#themeToggleButton:hover {
    background-color: #313d4d;
}

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background-color: #ffffff;
    border: 1px solid #d3d9e2;
    border-radius: 6px;
    padding: 5px 8px;
    selection-background-color: #2f6fed;
    selection-color: #ffffff;
}

QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid #2f6fed;
}

QComboBox::drop-down {
    border: none;
    width: 20px;
}

QCheckBox {
    spacing: 8px;
}

QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #c3cad5;
    border-radius: 4px;
    background: #ffffff;
}

QCheckBox::indicator:checked {
    background-color: #2f6fed;
    border-color: #2f6fed;
}

QTableView {
    background-color: #ffffff;
    alternate-background-color: #f7f9fc;
    gridline-color: #e6e9ef;
    border: 1px solid #e0e4ea;
    border-radius: 8px;
    selection-background-color: #dce8ff;
    selection-color: #10192b;
}

QHeaderView::section {
    background-color: #f0f2f6;
    color: #3a4453;
    padding: 6px 8px;
    border: none;
    border-bottom: 1px solid #e0e4ea;
    border-right: 1px solid #e6e9ef;
    font-weight: 600;
}

QTableView::item {
    padding: 3px 6px;
}

QPlainTextEdit {
    background-color: #ffffff;
    border: 1px solid #e0e4ea;
    border-radius: 8px;
    padding: 8px;
    color: #1c2430;
}

QSplitter::handle {
    background-color: transparent;
}

QScrollBar:vertical, QScrollBar:horizontal {
    background: transparent;
    border: none;
    margin: 0;
}

QScrollBar::handle {
    background: #c3cad5;
    border-radius: 5px;
    min-height: 24px;
    min-width: 24px;
}

QScrollBar::handle:hover {
    background: #a9b2c0;
}

QScrollBar::add-line, QScrollBar::sub-line {
    height: 0;
    width: 0;
}
)";

const char *const kDarkStyleSheet = R"(
QMainWindow, QDialog {
    background-color: #14181f;
    color: #e6e9ef;
}

QWidget {
    color: #e6e9ef;
    font-size: 13px;
}

QToolBar {
    background-color: #1a1f28;
    border-bottom: 1px solid #262c37;
    padding: 6px 10px;
    spacing: 8px;
}

QStatusBar {
    background-color: #1a1f28;
    border-top: 1px solid #262c37;
    color: #9aa3b1;
}

QGroupBox {
    background-color: #1a1f28;
    border: 1px solid #262c37;
    border-radius: 8px;
    margin-top: 14px;
    padding: 14px 10px 10px 10px;
    font-weight: 600;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
    color: #b7c0cf;
}

QLabel {
    background: transparent;
}

QPushButton {
    background-color: #1f2530;
    border: 1px solid #313a48;
    border-radius: 6px;
    padding: 6px 14px;
    color: #e6e9ef;
    font-weight: 500;
}

QPushButton:hover {
    background-color: #2a3140;
    border-color: #3d4757;
}

QPushButton:pressed {
    background-color: #171c25;
}

QPushButton:disabled {
    color: #5a6472;
    background-color: #181d26;
}

QPushButton#themeToggleButton {
    background-color: #4f8dfd;
    color: #0d1117;
    border: 1px solid #4f8dfd;
}

QPushButton#themeToggleButton:hover {
    background-color: #6c9dfd;
}

QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background-color: #1a1f28;
    border: 1px solid #313a48;
    border-radius: 6px;
    padding: 5px 8px;
    color: #e6e9ef;
    selection-background-color: #4f8dfd;
    selection-color: #0d1117;
}

QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border: 1px solid #4f8dfd;
}

QComboBox::drop-down {
    border: none;
    width: 20px;
}

QCheckBox {
    spacing: 8px;
}

QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid #3d4757;
    border-radius: 4px;
    background: #1a1f28;
}

QCheckBox::indicator:checked {
    background-color: #4f8dfd;
    border-color: #4f8dfd;
}

QTableView {
    background-color: #171c25;
    alternate-background-color: #1c222c;
    gridline-color: #262c37;
    border: 1px solid #262c37;
    border-radius: 8px;
    color: #e6e9ef;
    selection-background-color: #2c3e63;
    selection-color: #ffffff;
}

QHeaderView::section {
    background-color: #1f2530;
    color: #b7c0cf;
    padding: 6px 8px;
    border: none;
    border-bottom: 1px solid #262c37;
    border-right: 1px solid #232a35;
    font-weight: 600;
}

QTableView::item {
    padding: 3px 6px;
}

QPlainTextEdit {
    background-color: #171c25;
    border: 1px solid #262c37;
    border-radius: 8px;
    padding: 8px;
    color: #e6e9ef;
}

QSplitter::handle {
    background-color: transparent;
}

QScrollBar:vertical, QScrollBar:horizontal {
    background: transparent;
    border: none;
    margin: 0;
}

QScrollBar::handle {
    background: #313a48;
    border-radius: 5px;
    min-height: 24px;
    min-width: 24px;
}

QScrollBar::handle:hover {
    background: #3d4757;
}

QScrollBar::add-line, QScrollBar::sub-line {
    height: 0;
    width: 0;
}
)";

} // namespace

QString neoThemeStyleSheet(NeoTheme theme) {
  switch (theme) {
  case NeoTheme::Dark:
    return QString::fromUtf8(kDarkStyleSheet);

  case NeoTheme::Light:
  default:
    return QString::fromUtf8(kLightStyleSheet);
  }
}

QString neoThemeName(NeoTheme theme) {
  switch (theme) {
  case NeoTheme::Dark:
    return QStringLiteral("dark");

  case NeoTheme::Light:
  default:
    return QStringLiteral("light");
  }
}

NeoTheme neoThemeFromName(const QString &name) {
  if (name.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0)
    return NeoTheme::Dark;

  return NeoTheme::Light;
}
