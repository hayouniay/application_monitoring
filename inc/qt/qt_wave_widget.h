#ifndef QT_WAVE_WIDGET_H
#define QT_WAVE_WIDGET_H

#include <QColor>
#include <QTime>
#include <QVector>
#include <QWidget>

/*
 * A self-contained, dependency-free live line graph ("wave") drawn
 * with QPainter - no QtCharts module required. Holds a bounded rolling
 * history of samples and repaints itself each time a new one arrives.
 * Supports an optional second series on the same axes (used for I/O
 * read/write). Colors adapt to the app's current light/dark theme.
 */
class NeoQtWaveWidget : public QWidget {
  Q_OBJECT

public:
  explicit NeoQtWaveWidget(QWidget *parent = nullptr);

  void setSeries(const QString &label, const QColor &color);
  void setSecondSeries(const QString &label, const QColor &color);

  void setUnit(const QString &unit);

  /* Percent-style metrics (CPU/Memory/Swap) look best on a fixed
   * 0-100 axis rather than jumping around with auto-scale. */
  void setFixedRange(double minValue, double maxValue);
  void setAutoRange();

  void setMaxSamples(int maxSamples);

  /*
   * When enabled, the X axis shows real wall-clock time-of-day
   * (HH:mm:ss) at each sample instead of no labels at all
   */
  void setShowTimeAxis(bool show);

public slots:
  void addSample(double value, double secondValue = 0.0);
  void clear();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  bool isDarkTheme() const;

  QString m_label;
  QColor m_color;

  bool m_hasSecondSeries;
  QString m_label2;
  QColor m_color2;

  QString m_unit;

  QVector<double> m_samples;
  QVector<double> m_samples2;
  QVector<QTime> m_timestamps;
  int m_maxSamples;
  bool m_showTimeAxis;

  bool m_fixedRange;
  double m_rangeMin;
  double m_rangeMax;
};

#endif /* QT_WAVE_WIDGET_H */
