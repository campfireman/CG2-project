#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H
#define GRAY_SPECTRUM 256

#include <QMainWindow>
#ifndef QT_NO_PRINTER
#include <QPrinter>
#endif

// eigen library for matrix SVD
#pragma GCC diagnostic push
// -Wall didn't work for some reason
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wint-in-bool-context"
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include "utils/Eigen/SVD"
#include "utils/Eigen/Core"
#pragma GCC diagnostic pop
using namespace Eigen;

#include "fstream"
#include <functional>
#include <tuple>
#include <vector>
class QAction;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QMenu;
class QRect;
class QScrollArea;
class QScrollBar;
class QSlider;
class QSpinBox;
class QStackedLayout;
class QTableWidget;
class QTextEdit;
class QUnevenIntSpinBox;
class QVBoxLayout;
class QTabWidget;
class QPushButton;
class QSpinBox;
class QColor;

class ImageViewer : public QMainWindow
{
    Q_OBJECT

private slots:
    void imageChanged(QImage *image);
    void applyExampleAlgorithmClicked();
    void crossSliderValueChanged(int value);
    void quantizationSliderValueChanged(int value);
    void brightnessSliderValueChanged(int value);
    void contrastSliderValueChanged(int value);
    void robustContrastSliderValueChanged(int value);
    void filterMChanged(int value);
    void filterNChanged(int value);
    void borderStrategyChangedPad();
    void borderStrategyChangedConstant();
    void borderStrategyChangedMirror();
    void applyFilterClicked();
    void applyGaussianFilterClicked();

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

    // setters
    void setBorderStrategy(std::function<QColor(int, int, QImage *)> strategy);

    // actions
    void updateImageInformation(QImage *image);
    void quantizeImage(int value);
    void drawCross(int value);
    void changeBrightness(int value);
    void changeContrast(int value);
    void changeRobustContrast(int value);
    void changeFilterTableWidth(int value);
    void changeFilterTableHeight(int value);
    void setFilterTableWidgets();
    void applyFilter(Eigen::MatrixXd filter);
    void createHistogram(QImage *image, int *hist);

    // helpers
    int rgbToGray(int red, int green, int blue);
    int rgbToGray(QColor color);
    QColor rgbToGrayColor(QColor color);
    void iterateRect(int width, int height, std::function<void(int, int)> func);
    void iteratePixels(std::function<void(int, int)> func);
    std::tuple<int, int, int> rgbToYCbCr(std::tuple<int, int, int> rgb);
    std::tuple<int, int, int> rgbToYCbCr(QColor rgb);
    QColor yCbCrToRgb(std::tuple<int, int, int> val);
    int clamp(int value, int min, int max);
    Eigen::MatrixXd createGaussianKernel(double sigma);
    QColor getFilterPixel(int i, int j, QImage *image);

    static QColor borderPad(int i, int j, QImage *image);
    static QColor borderConstant(int i, int j, QImage *image);
    static QColor borderMirror(int i, int j, QImage *image);

protected:
    void resizeEvent(QResizeEvent *event);

private:
    void setDefaults();
    void resetImage();
    void generateControlPanels();

    void startLogging();
    void generateMainGui();

    void createActions();
    void createMenus();
    void updateActions();
    void scaleImage(double factor);
    void adjustScrollBar(QScrollBar *scrollBar, double factor);
    void renewLogging();

    // custom attributes
    QImage *originalImage;
    int o_hist[GRAY_SPECTRUM] = {0};
    QSlider *crossSlider;
    QLabel *varianceInfo;
    QLabel *averageInfo;
    QGraphicsScene *histogramChart;
    QGraphicsView *histogramChartView;
    QSlider *quantizationSlider;
    QStackedLayout *stack;
    QSlider *brightnessSlider;
    QSlider *contrastSlider;
    QSlider *robustContrastSlider;
    QUnevenIntSpinBox *filterM;
    QUnevenIntSpinBox *filterN;
    QTableWidget *filterTable;
    std::vector<std::vector<int>> *filter;
    QPushButton *applyFilterButton;
    std::function<QColor(int, int, QImage *)> borderStrategy;
    QSpinBox *sigmaSpinBox;

    QTabWidget *tabWidget;
    QTabWidget *imageInfo;
    QTextEdit *logBrowser;
    QWidget *centralwidget;
    QLabel *imageLabel;
    QScrollArea *scrollArea;
    double scaleFactor;
    QImage *image;
    QWidget *m_option_panel1;
    QVBoxLayout *m_option_layout1;

    QWidget *m_option_panel2;
    QVBoxLayout *m_option_layout2;

    QPushButton *button1;
    QPushButton *cross_draw_button;
    QSpinBox *spinbox1;

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
