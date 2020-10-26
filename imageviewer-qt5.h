#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QMainWindow>
#ifndef QT_NO_PRINTER
#include <QPrinter>
#endif

#include "fstream"

#include <QtCharts>
#include <QChart>
#include <QBarSeries>
using namespace QtCharts;
class QAction;
class QLabel;
class QMenu;
class QScrollArea;
class QScrollBar;
class QTextEdit;
class QVBoxLayout;
class QTabWidget;
class QPushButton;
class QSpinBox;
class QColor;

class ImageViewer : public QMainWindow
{
    Q_OBJECT

private:
    // Beispiel für GUI Elemente
    QWidget *m_option_panel1;
    QVBoxLayout *m_option_layout1;

    QWidget *m_option_panel2;
    QVBoxLayout *m_option_layout2;

    QPushButton *button1;
    QPushButton *cross_draw_button;
    QSpinBox *spinbox1;

    // hier können weitere GUI Objekte hin wie Buttons Slider etc.

    // GUI global values
    int cross_slider_value;

private slots:

    void applyExampleAlgorithm();

    // hier können weitere als SLOTS definierte Funktionen hin, die auf Knopfdruck etc. aufgerufen werden.
    void drawCross();
    void imageChanged(QImage *image);
    void crossSliderValueChanged(int value);

    void open();
    void print();
    void zoomIn();
    void zoomOut();
    void normalSize();
    void fitToWindow();
    void about();

signals:
    void imageUpdated(QImage *image);

public:
    ImageViewer();
    bool loadFile(const QString &);
    void updateImageDisplay();
    bool imageIsLoaded();

    int rgbToGray(QColor color);
    QColor rgbToGrayColor(QColor color);

protected:
    void resizeEvent(QResizeEvent *event);

private:
    // in diesen Beiden Methoden sind Änderungen nötig bzw. sie dienen als
    // Vorlage für eigene Methoden.
    void generateControlPanels();

    // Ab hier technische Details die nicht für das Verständnis notwendig sind.
    void startLogging();
    void generateMainGui();

    void createActions();
    void createMenus();
    void updateActions();
    void scaleImage(double factor);
    void adjustScrollBar(QScrollBar *scrollBar, double factor);
    void renewLogging();

    QTabWidget *tabWidget;
    QTabWidget *imageInfo;
    QTextEdit *logBrowser;
    QWidget *centralwidget;
    QLabel *imageLabel;
    QLabel *varianceInfo;
    QLabel *averageInfo;
    QChart *histogramChart;
    QChartView *histogramChartView;
    QBarSeries *histogramData;
    QScrollArea *scrollArea;
    double scaleFactor;
    QImage *image;
    QImage *original_image;

    std::fstream logFile;

#ifndef QT_NO_PRINTER
    QPrinter printer;
#endif

    QAction *openAct;
    QAction *printAct;
    QAction *exitAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *normalSizeAct;
    QAction *fitToWindowAct;
    QAction *aboutAct;
    QAction *aboutQtAct;

    QMenu *fileMenu;
    QMenu *viewMenu;
    QMenu *helpMenu;
};

#endif
