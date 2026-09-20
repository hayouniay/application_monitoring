#include "qt/qt_wave_widget.h"
#include "qt/qt_application.h"
#include "qt/qt_theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

NeoQtWaveWidget::NeoQtWaveWidget(QWidget *parent)
    : QWidget(parent), m_label(QStringLiteral("Series")), m_color(33, 111, 237),
      m_hasSecondSeries(false), m_unit(), m_maxSamples(120),
      m_fixedRange(false), m_rangeMin(0.0), m_rangeMax(100.0) {
  setMinimumHeight(140);
}

void NeoQtWaveWidget::setSeries(const QString &label, const QColor &color) {
  m_label = label;
  m_color = color;
  update();
}

void NeoQtWaveWidget::setSecondSeries(const QString &label,
                                      const QColor &color) {
  m_hasSecondSeries = true;
  m_label2 = label;
  m_color2 = color;
  update();
}

void NeoQtWaveWidget::setUnit(const QString &unit) { m_unit = unit; }

void NeoQtWaveWidget::setFixedRange(double minValue, double maxValue) {
  m_fixedRange = true;
  m_rangeMin = minValue;
  m_rangeMax = maxValue;
}

void NeoQtWaveWidget::setAutoRange() { m_fixedRange = false; }

void NeoQtWaveWidget::setMaxSamples(int maxSamples) {
  m_maxSamples = std::max(2, maxSamples);
}

void NeoQtWaveWidget::addSample(double value, double secondValue) {
  m_samples.append(value);

  while (m_samples.size() > m_maxSamples) {
    m_samples.removeFirst();
  }

  if (m_hasSecondSeries) {
    m_samples2.append(secondValue);

    while (m_samples2.size() > m_maxSamples) {
      m_samples2.removeFirst();
    }
  }

  update();
}

void NeoQtWaveWidget::clear() {
  m_samples.clear();
  m_samples2.clear();
  update();
}

bool NeoQtWaveWidget::isDarkTheme() const {
  if (auto *application = qobject_cast<NeoQtApplication *>(qApp)) {
    return application->currentTheme() == NeoTheme::Dark;
  }

  return false;
}

void NeoQtWaveWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const bool dark = isDarkTheme();

  const QColor backgroundColor =
      dark ? QColor(0x17, 0x1c, 0x25) : QColor(0xff, 0xff, 0xff);
  const QColor borderColor =
      dark ? QColor(0x26, 0x2c, 0x37) : QColor(0xe0, 0xe4, 0xea);
  const QColor gridColor =
      dark ? QColor(0x26, 0x2c, 0x37) : QColor(0xe6, 0xe9, 0xef);
  const QColor textColor =
      dark ? QColor(0xb7, 0xc0, 0xcf) : QColor(0x5a, 0x64, 0x72);

  const QRectF widgetRect = rect().adjusted(0, 0, -1, -1);

  /* Card background */
  painter.setPen(QPen(borderColor, 1));
  painter.setBrush(backgroundColor);
  painter.drawRoundedRect(widgetRect, 8, 8);

  /* Reserve margins for the legend line (top) and axis labels
   * (left/bottom). */
  const int marginTop = 26;
  const int marginBottom = 18;
  const int marginLeft = 44;
  const int marginRight = 12;

  const QRectF plotRect(marginLeft, marginTop,
                        width() - marginLeft - marginRight,
                        height() - marginTop - marginBottom);

  /* --- Legend / current-value line -------------------------------- */

  const double lastValue = m_samples.isEmpty() ? 0.0 : m_samples.last();

  QFont legendFont = painter.font();

  if (legendFont.pointSizeF() > 0) {
    legendFont.setPointSizeF(legendFont.pointSizeF() * 0.92);
  } else if (legendFont.pixelSize() > 0) {
    /* The app's global stylesheet sets font-size in px rather than
     * pt, which makes pointSizeF() report -1 - scale pixel size
     * instead in that case rather than passing a negative value
     * to setPointSizeF(). */
    legendFont.setPixelSize(
        std::max(1, static_cast<int>(legendFont.pixelSize() * 0.92)));
  }

  painter.setFont(legendFont);

  QString legendText = QStringLiteral("%1: %2%3")
                           .arg(m_label)
                           .arg(lastValue, 0, 'f', 1)
                           .arg(m_unit);

  painter.setPen(m_color);
  painter.drawText(QRectF(10, 4, width() - 20, 18),
                   Qt::AlignLeft | Qt::AlignVCenter, legendText);

  if (m_hasSecondSeries) {
    const double lastValue2 = m_samples2.isEmpty() ? 0.0 : m_samples2.last();

    const QString legendText2 = QStringLiteral("%1: %2%3")
                                    .arg(m_label2)
                                    .arg(lastValue2, 0, 'f', 1)
                                    .arg(m_unit);

    const QFontMetrics metrics(legendFont);
    const int firstWidth = metrics.horizontalAdvance(legendText) + 24;

    painter.setPen(m_color2);
    painter.drawText(QRectF(10 + firstWidth, 4, width() - 20 - firstWidth, 18),
                     Qt::AlignLeft | Qt::AlignVCenter, legendText2);
  }

  /* --- Determine the plotted range ---------------------------------- */

  double rangeMin = m_rangeMin;
  double rangeMax = m_rangeMax;

  if (!m_fixedRange) {
    double observedMin = 0.0;
    double observedMax = 1.0;
    bool first = true;

    for (double value : m_samples) {
      if (first || value < observedMin)
        observedMin = value;
      if (first || value > observedMax)
        observedMax = value;
      first = false;
    }

    for (double value : m_samples2) {
      if (value < observedMin)
        observedMin = value;
      if (value > observedMax)
        observedMax = value;
    }

    if (observedMax <= observedMin) {
      observedMax = observedMin + 1.0;
    }

    /* A little headroom so the curve doesn't touch the edges. */
    const double padding = (observedMax - observedMin) * 0.15;

    rangeMin = std::max(0.0, observedMin - padding);
    rangeMax = observedMax + padding;
  }

  if (rangeMax <= rangeMin) {
    rangeMax = rangeMin + 1.0;
  }

  /* --- Gridlines + axis labels ---------------------------------- */

  painter.setPen(QPen(gridColor, 1));

  const int gridLines = 4;

  for (int i = 0; i <= gridLines; ++i) {
    const double fraction = static_cast<double>(i) / gridLines;
    const double y = plotRect.bottom() - fraction * plotRect.height();

    painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));

    const double value = rangeMin + fraction * (rangeMax - rangeMin);

    painter.setPen(textColor);
    painter.drawText(QRectF(0, y - 8, marginLeft - 6, 16),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString::number(value, 'f', value < 10 ? 1 : 0));
    painter.setPen(QPen(gridColor, 1));
  }

  /* --- Helper to map a sample index/value to a widget point --------- */

  const auto mapPoint = [&](int index, int count, double value) -> QPointF {
    const double x =
        (count <= 1)
            ? plotRect.left()
            : plotRect.left() +
                  (static_cast<double>(index) / (count - 1)) * plotRect.width();

    double fraction = (value - rangeMin) / (rangeMax - rangeMin);
    fraction = std::clamp(fraction, 0.0, 1.0);

    const double y = plotRect.bottom() - fraction * plotRect.height();

    return QPointF(x, y);
  };

  /* --- Draw the curve(s) --------------------------------------------- */

  const auto drawSeries = [&](const QVector<double> &samples,
                              const QColor &color, bool fill) {
    if (samples.size() < 2) {
      return;
    }

    QPainterPath path;

    for (int i = 0; i < samples.size(); ++i) {
      const QPointF point = mapPoint(i, samples.size(), samples[i]);

      if (i == 0) {
        path.moveTo(point);
      } else {
        path.lineTo(point);
      }
    }

    if (fill) {
      QPainterPath fillPath = path;
      fillPath.lineTo(plotRect.right(), plotRect.bottom());
      fillPath.lineTo(plotRect.left(), plotRect.bottom());
      fillPath.closeSubpath();

      QLinearGradient gradient(0, plotRect.top(), 0, plotRect.bottom());
      QColor fillColor = color;
      fillColor.setAlpha(70);
      gradient.setColorAt(0.0, fillColor);
      fillColor.setAlpha(0);
      gradient.setColorAt(1.0, fillColor);

      painter.setPen(Qt::NoPen);
      painter.setBrush(gradient);
      painter.drawPath(fillPath);
    }

    painter.setPen(QPen(color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
  };

  painter.save();
  painter.setClipRect(plotRect);

  drawSeries(m_samples, m_color, !m_hasSecondSeries);

  if (m_hasSecondSeries) {
    drawSeries(m_samples2, m_color2, false);
  }

  painter.restore();
}
