

#include <Qt>
#include <QtWidgets>
#include <QGroupBox>
#include <QWidget>
#include <QHBoxLayout>
#include <QBarSet>
#ifndef QT_NO_PRINTER
#include <QPrintDialog>
#endif
#include <iostream>
#include <cmath>

#include "imageviewer-qt5.h"

#define DEFAULT_CROSS_SLIDER 49
#define DEFAULT_QUANTIZATION_SLIDER 8
#define DEFAULT_CONTRAST_SLIDER 0
#define DEFAULT_BRIGHTNESS_SLIDER 0

ImageViewer::ImageViewer()
{

    image = NULL;
    originalImage = NULL;
    histogramChart = NULL;
    histogramChartView = NULL;
    resize(1600, 600);

    startLogging();

    generateMainGui();
    renewLogging();

    generateControlPanels();
    createActions();
    createMenus();

    resize(QGuiApplication::primaryScreen()->availableSize() * 0.85);
    QObject::connect(this, &ImageViewer::imageUpdated, this, &ImageViewer::imageChanged);
}

bool ImageViewer::imageIsLoaded()
{
    return image != NULL && originalImage != NULL;
}

/* SLOTS */

void ImageViewer::imageChanged(QImage *image)
{
    int MN = image->width() * image->height();

    // create histogram
    int hist[GRAY_SPECTRUM] = {0};
    createHistogram(image, hist);

    // calculate average
    double avg = 0.0;
    for (int k = 0; k < GRAY_SPECTRUM; k++)
    {
        avg += hist[k] * k;
    }
    avg /= MN;

    // calculate variance
    double var = 0.0;
    for (int k = 0; k < GRAY_SPECTRUM; k++)
    {
        var += (hist[k] / (double)MN) * pow(k - avg, 2);
    }

    QBarSet *hist_set = new QBarSet("histogram");
    for (int k = 0; k < GRAY_SPECTRUM; k++)
    {
        hist_set->append(hist[k]);
    }
    // display values
    averageInfo->setNum(avg);
    varianceInfo->setNum(var);

    QBarSeries *series = new QBarSeries();
    series->append(hist_set);

    QChart *histogramChart = new QChart();
    histogramChart->addSeries(series);
    histogramChart->setTitle("Histogram");
    histogramChart->setAnimationOptions(QChart::SeriesAnimations);

    QValueAxis *axisX = new QValueAxis();
    axisX->setRange(0.0, 255.0);
    histogramChart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    histogramChart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    histogramChart->legend()->setVisible(false);
    histogramChart->legend()->setAlignment(Qt::AlignBottom);

    QChartView *histogramChartView = new QChartView(histogramChart);
    histogramChartView->setRenderHint(QPainter::Antialiasing);

    stack->addWidget(histogramChartView);
    stack->setCurrentWidget(histogramChartView);

    updateImageDisplay();
}

void ImageViewer::applyExampleAlgorithm()
{
    if (imageIsLoaded())
    {
        iteratePixels([this](int i, int j) {
            image->setPixelColor(i, j, rgbToGrayColor(image->pixelColor(i, j)));
        });
        emit imageUpdated(image);
        logFile << "transformed to grayscale" << std::endl;
        renewLogging();
    }
}

void ImageViewer::crossSliderValueChanged(int value)
{
    drawCross(value);
}

void ImageViewer::quantizationSliderValueChanged(int value)
{
    quantizeImage(value);
}

void ImageViewer::brightnessSliderValueChanged(int value)
{
    changeBrightness(value);
}
void ImageViewer::contrastSliderValueChanged(int value)
{
    changeContrast(value);
}
void ImageViewer::robustContrastSliderValueChanged(int value)
{
    changeRobustContrast(value);
}

/* PUBLIC */

void ImageViewer::createHistogram(QImage *image, int *hist)
{
    for (int i = 0; i < image->width(); i++)
    {
        for (int j = 0; j < image->height(); j++)
        {
            int val = rgbToGray(image->pixelColor(i, j));
            hist[val] += 1;
        }
    }
}

void ImageViewer::drawCross(int value)
{
    if (imageIsLoaded())
    {
        int size = std::min(image->width(), image->height());
        int max_width = (int)(size * ((value + 1) / 100.0));
        int offset = (int)((size - max_width) / 2.0);
        for (int i = 0; i < size; i++)
        {
            int x_l = i;
            int y_l = i;
            int x_r = size - i - 1;
            int y_r = i;

            if (i > offset && i < offset + max_width)
            {
                image->setPixelColor(x_l, y_l, QColor(255, 0, 0));
                image->setPixelColor(x_r, y_r, QColor(255, 0, 0));
            }
            else
            {
                image->setPixelColor(x_l, y_l, originalImage->pixelColor(x_l, y_l));
                image->setPixelColor(x_r, y_r, originalImage->pixelColor(x_r, y_l));
            }
        }
        emit imageUpdated(image);
        logFile << "drew red cross" << std::endl;
        renewLogging();
    }
}

void ImageViewer::quantizeImage(int value)
{
    if (imageIsLoaded())
    {
        int div = pow(2, (8 - value));
        if (div > 0)
        {
            iteratePixels([this, div](int i, int j) {
                QColor color = originalImage->pixelColor(i, j);
                color.setRed((color.red() / div) * div);
                color.setBlue((color.blue() / div) * div);
                color.setGreen((color.green() / div) * div);
                image->setPixelColor(i, j, color);
            });
        }
        emit imageUpdated(image);
        logFile << "quantized to " << value << "-bit" << std::endl;
        logFile << div << std::endl;
        renewLogging();
    }
}

void ImageViewer::changeBrightness(int value)
{
    if (imageIsLoaded())
    {
        value = (int)((value / 100.0) * 255);
        iteratePixels([this, value](int i, int j) {
            std::tuple<int, int, int> color = rgbToYCbCr(originalImage->pixelColor(i, j));
            int intensity = std::get<0>(color) + value;
            std::get<0>(color) = intensity > 255 ? 255 : intensity;
            image->setPixelColor(i, j, yCbCrToRgb(color));
        });
        emit imageUpdated(image);
        logFile << "added brightness of " << value << std::endl;
        renewLogging();
    }
}
void ImageViewer::changeContrast(int value)
{
    if (imageIsLoaded())
    {
        double factor = (value / 10.0) + 1;
        iteratePixels([this, factor](int i, int j) {
            std::tuple<int, int, int> color = rgbToYCbCr(originalImage->pixelColor(i, j));
            int intensity = (int)(std::get<0>(color) * factor);
            std::get<0>(color) = intensity > 255 ? 255 : intensity;
            image->setPixelColor(i, j, yCbCrToRgb(color));
        });
        emit imageUpdated(image);
        logFile << "changed contrast with factor of " << factor << std::endl;
        renewLogging();
    }
}

void ImageViewer::changeRobustContrast(int value)
{
    if (imageIsLoaded())
    {
        double factor = (value / 100.0) / 2;
        int MN = image->width() * image->height();
        int n_a_low = MN * factor;
        int n_a_high = MN * (1 - factor);
        int a_low;
        int a_high;
        bool seenLow = false;
        bool seenHigh = false;
        int sum = 0;
        for (int a = 0; a < GRAY_SPECTRUM; a++)
        {
            sum += o_hist[a];
            if (sum >= n_a_low && !seenLow)
            {
                a_low = a;
                seenLow = true;
            }
            if (sum >= n_a_high && !seenHigh)
            {
                a_high = a;
                seenHigh = true;
            }
            if (seenLow && seenHigh)
            {
                break;
            }
        }
        double ratio = (GRAY_SPECTRUM - 1) / (double)(a_high - a_low);
        iteratePixels([this, a_low, a_high, ratio](int i, int j) {
            std::tuple<int, int, int> color = rgbToYCbCr(originalImage->pixelColor(i, j));
            int intensity = std::get<0>(color);
            if (intensity <= a_low)
            {
                intensity = 0;
            }
            else if (intensity >= a_high)
            {
                intensity = GRAY_SPECTRUM - 1;
            }
            else
            {
                intensity = (intensity - a_low) * ratio;
            }
            std::get<0>(color) = intensity;
            image->setPixelColor(i, j, yCbCrToRgb(color));
        });
        emit imageUpdated(image);
        logFile << "changed robust contrast with percentage of " << factor << std::endl;
        renewLogging();
    }
}

int ImageViewer::rgbToGray(int red, int green, int blue)
{
    return (int)((16 + (1 / 256.0) * (65.738 * red + 129.057 * green + 25.064 * blue)));
}
int ImageViewer::rgbToGray(QColor color)
{
    return rgbToGray(color.red(), color.green(), color.blue());
}

std::tuple<int, int, int> ImageViewer::rgbToYCbCr(QColor rgb)
{
    return rgbToYCbCr(std::tuple<int, int, int>(rgb.red(), rgb.green(), rgb.blue()));
}
std::tuple<int, int, int> ImageViewer::rgbToYCbCr(std::tuple<int, int, int> rgb)
{
    int red = std::get<0>(rgb);
    int green = std::get<1>(rgb);
    int blue = std::get<2>(rgb);
    return std::tuple<int, int, int>(
        rgbToGray(red, green, blue),
        128 + (int)((1 / 256.0) * (-37.945 * red + (-74.494) * green + 112.439 * blue)),
        128 + (int)((1 / 256.0) * (112.439 * red + (-94.154) * green + (-18.285) * blue)));
}
QColor ImageViewer::yCbCrToRgb(std::tuple<int, int, int> value)
{
    int y = std::get<0>(value);
    int cb = std::get<1>(value);
    int cr = std::get<2>(value);
    double yComponent = 298.082 * (y - 16);

    int r = (int)((1 / 256.0) * (yComponent + 408.583 * (cr - 128)));
    int g = (int)((1 / 256.0) * (yComponent + (-100.291) * (cb - 128) + (-208.120) * (cr - 128)));
    int b = (int)((1 / 256.0) * (yComponent + 516.411 * (cb - 128)));
    r = r < 0 ? 0 : r;
    r = r > 255 ? 255 : r;
    g = g < 0 ? 0 : g;
    g = g > 255 ? 255 : g;
    b = b < 0 ? 0 : b;
    b = b > 255 ? 255 : b;
    return QColor(
        r,
        g,
        b);
}
QColor ImageViewer::rgbToGrayColor(QColor color)
{
    int value = rgbToGray(color.red(), color.green(), color.blue());
    return QColor(value, value, value);
}

void ImageViewer::iteratePixels(std::function<void(int, int)> func)
{
    for (int i = 0; i < image->width(); i++)
    {
        for (int j = 0; j < image->height(); j++)
        {
            func(i, j);
        }
    }
}

/* PRIVATE */

void ImageViewer::setDefaults()
{
    crossSlider->blockSignals(true);
    crossSlider->setValue(DEFAULT_CROSS_SLIDER);
    crossSlider->blockSignals(false);
    quantizationSlider->blockSignals(true);
    quantizationSlider->setValue(DEFAULT_QUANTIZATION_SLIDER);
    quantizationSlider->blockSignals(false);
}

void ImageViewer::generateControlPanels()
{
    // ex1

    m_option_panel1 = new QWidget();
    m_option_layout1 = new QVBoxLayout();
    m_option_panel1->setLayout(m_option_layout1);

    button1 = new QPushButton();
    button1->setText("Apply algorithm");
    QObject::connect(button1, SIGNAL(clicked()), this, SLOT(applyExampleAlgorithm()));

    QGroupBox *cross_group = new QGroupBox("Red cross");
    QVBoxLayout *cross_layout = new QVBoxLayout();

    QLabel *crossSliderLabel = new QLabel("Cross width");
    crossSlider = new QSlider(Qt::Horizontal);
    crossSlider->setValue(DEFAULT_CROSS_SLIDER);
    QObject::connect(crossSlider, SIGNAL(valueChanged(int)), this, SLOT(crossSliderValueChanged(int)));

    cross_layout->addWidget(crossSliderLabel);
    cross_layout->addWidget(crossSlider);
    cross_group->setLayout(cross_layout);

    m_option_layout1->addWidget(button1);
    m_option_layout1->addWidget(cross_group);
    tabWidget->addTab(m_option_panel1, "1");

    // ex2

    m_option_panel2 = new QWidget();
    m_option_layout2 = new QVBoxLayout();
    m_option_panel2->setLayout(m_option_layout2);

    QHBoxLayout *avg_info = new QHBoxLayout();
    averageInfo = new QLabel();
    avg_info->addWidget(new QLabel("Average gray: "));
    avg_info->addWidget(averageInfo);

    QHBoxLayout *var_info = new QHBoxLayout();
    varianceInfo = new QLabel();
    var_info->addWidget(new QLabel("Variance: "));
    var_info->addWidget(varianceInfo);

    quantizationSlider = new QSlider(Qt::Horizontal);
    quantizationSlider->setTickInterval(1);
    quantizationSlider->setRange(1, 8);
    quantizationSlider->setValue(DEFAULT_QUANTIZATION_SLIDER);
    QObject::connect(quantizationSlider, SIGNAL(valueChanged(int)), this, SLOT(quantizationSliderValueChanged(int)));

    QVBoxLayout *quantizationLayout = new QVBoxLayout();
    quantizationLayout->addWidget(new QLabel("Quantization"));
    quantizationLayout->addWidget(quantizationSlider);

    brightnessSlider = new QSlider(Qt::Horizontal);
    brightnessSlider->setRange(1, 100);
    brightnessSlider->setValue(DEFAULT_BRIGHTNESS_SLIDER);
    QObject::connect(brightnessSlider, SIGNAL(valueChanged(int)), this, SLOT(brightnessSliderValueChanged(int)));

    QVBoxLayout *brightnessLayout = new QVBoxLayout();
    brightnessLayout->addWidget(new QLabel("Brightness"));
    brightnessLayout->addWidget(brightnessSlider);

    contrastSlider = new QSlider(Qt::Horizontal);
    contrastSlider->setRange(1, 10);
    contrastSlider->setValue(DEFAULT_BRIGHTNESS_SLIDER);
    QObject::connect(contrastSlider, SIGNAL(valueChanged(int)), this, SLOT(contrastSliderValueChanged(int)));

    QVBoxLayout *contrastLayout = new QVBoxLayout();
    contrastLayout->addWidget(new QLabel("Contrast"));
    contrastLayout->addWidget(contrastSlider);

    robustContrastSlider = new QSlider(Qt::Horizontal);
    robustContrastSlider->setRange(1, 100);
    robustContrastSlider->setValue(DEFAULT_BRIGHTNESS_SLIDER);
    QObject::connect(robustContrastSlider, SIGNAL(valueChanged(int)), this, SLOT(robustContrastSliderValueChanged(int)));

    QVBoxLayout *robustContrastLayout = new QVBoxLayout();
    robustContrastLayout->addWidget(new QLabel("Robust robustContrast"));
    robustContrastLayout->addWidget(robustContrastSlider);

    stack = new QStackedLayout();

    m_option_layout2->addLayout(avg_info);
    m_option_layout2->addLayout(var_info);
    m_option_layout2->addLayout(quantizationLayout);
    m_option_layout2->addLayout(brightnessLayout);
    m_option_layout2->addLayout(contrastLayout);
    m_option_layout2->addLayout(robustContrastLayout);
    m_option_layout2->addLayout(stack);

    tabWidget->addTab(m_option_panel2, "2");

    // ex3

    QWidget *m_option_panel3 = new QWidget();
    QVBoxLayout *m_option_layout3 = new QVBoxLayout();
    m_option_panel3->setLayout(m_option_layout3);

    tabWidget->addTab(m_option_panel3, "3");

    // ex4

    QWidget *m_option_panel4 = new QWidget();
    QVBoxLayout *m_option_layout4 = new QVBoxLayout();
    m_option_panel4->setLayout(m_option_layout4);

    tabWidget->addTab(m_option_panel4, "4");

    // ex5

    QWidget *m_option_panel5 = new QWidget();
    QVBoxLayout *m_option_layout5 = new QVBoxLayout();
    m_option_panel5->setLayout(m_option_layout5);

    tabWidget->addTab(m_option_panel5, "5");

    tabWidget->show();
}

/**************************************************************************************** 
*
*   ab hier kommen technische Details, die nicht notwenig für das Verständnis und die 
*   Bearbeitung sind.
*
*
*****************************************************************************************/

void ImageViewer::startLogging()
{
    //LogFile
    logFile.open("log.txt", std::ios::out);
    logFile << "Logging: \n"
            << std::endl;
}

void ImageViewer::renewLogging()
{
    QFile file("log.txt"); // Create a file handle for the file named
    QString line;
    file.open(QIODevice::ReadOnly); // Open the file

    QTextStream stream(&file); // Set the stream to read from myFile
    logBrowser->clear();
    while (!stream.atEnd())
    {

        line = stream.readLine(); // this reads a line (QString) from the file
        logBrowser->append(line);
    }
}

void ImageViewer::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    centralwidget->setMinimumWidth(width());
    centralwidget->setMinimumHeight(height());
    centralwidget->setMaximumWidth(width());
    centralwidget->setMaximumHeight(height());
    logBrowser->setMinimumWidth(width() - 40);
    logBrowser->setMaximumWidth(width() - 40);
}

void ImageViewer::updateImageDisplay()
{
    imageLabel->setPixmap(QPixmap::fromImage(*image));
}

void ImageViewer::generateMainGui()
{
    /* Tab widget */
    tabWidget = new QTabWidget(this);
    tabWidget->setObjectName(QStringLiteral("tabWidget"));

    /* Center widget */
    centralwidget = new QWidget(this);
    centralwidget->setObjectName(QStringLiteral("centralwidget"));
    //centralwidget->setFixedSize(200,200);
    //setCentralWidget(centralwidget);

    imageLabel = new QLabel;
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents(true);

    /* Center widget */
    scrollArea = new QScrollArea;
    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);

    setCentralWidget(scrollArea);

    /* HBox layout */
    QGridLayout *gLayout = new QGridLayout(centralwidget);
    gLayout->setObjectName(QStringLiteral("hboxLayout"));
    gLayout->addWidget(new QLabel(), 1, 1);
    gLayout->setVerticalSpacing(50);
    gLayout->addWidget(tabWidget, 2, 1);
    gLayout->addWidget(scrollArea, 2, 2);

    logBrowser = new QTextEdit(this);
    logBrowser->setMinimumHeight(100);
    logBrowser->setMaximumHeight(200);
    logBrowser->setMinimumWidth(width());
    logBrowser->setMaximumWidth(width());
    gLayout->addWidget(logBrowser, 3, 1, 1, 2);
    gLayout->setVerticalSpacing(50);
}

bool ImageViewer::loadFile(const QString &fileName)
{
    if (image != NULL)
    {
        delete image;
        image = NULL;
    }
    if (originalImage != NULL)
    {
        delete originalImage;
        originalImage = NULL;
    }

    image = new QImage(fileName);
    originalImage = new QImage(image->copy());

    if (image->isNull())
    {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Cannot load %1.").arg(QDir::toNativeSeparators(fileName)));
        setWindowFilePath(QString());
        imageLabel->setPixmap(QPixmap());
        imageLabel->adjustSize();
        return false;
    }

    scaleFactor = 1.0;

    emit imageUpdated(image);
    createHistogram(originalImage, o_hist);
    setDefaults();

    printAct->setEnabled(true);
    fitToWindowAct->setEnabled(true);
    updateActions();

    if (!fitToWindowAct->isChecked())
        imageLabel->adjustSize();

    setWindowFilePath(fileName);
    logFile << "geladen: " << fileName.toStdString().c_str() << std::endl;
    renewLogging();
    return true;
}

void ImageViewer::open()
{
    QStringList mimeTypeFilters;
    foreach (const QByteArray &mimeTypeName, QImageReader::supportedMimeTypes())
        mimeTypeFilters.append(mimeTypeName);
    mimeTypeFilters.sort();
    const QStringList picturesLocations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
    QFileDialog dialog(this, tr("Open File"),
                       picturesLocations.isEmpty() ? QDir::currentPath() : picturesLocations.first());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setMimeTypeFilters(mimeTypeFilters);
    dialog.selectMimeTypeFilter("image/jpeg");

    while (dialog.exec() == QDialog::Accepted && !loadFile(dialog.selectedFiles().first()))
    {
    }
}

void ImageViewer::print()
{
    Q_ASSERT(imageLabel->pixmap());
#if !defined(QT_NO_PRINTER) && !defined(QT_NO_PRINTDIALOG)
    QPrintDialog dialog(&printer, this);
    if (dialog.exec())
    {
        QPainter painter(&printer);
        QRect rect = painter.viewport();
        QSize size = imageLabel->pixmap()->size();
        size.scale(rect.size(), Qt::KeepAspectRatio);
        painter.setViewport(rect.x(), rect.y(), size.width(), size.height());
        painter.setWindow(imageLabel->pixmap()->rect());
        painter.drawPixmap(0, 0, *imageLabel->pixmap());
    }
#endif
}

void ImageViewer::zoomIn()
{
    scaleImage(1.25);
}

void ImageViewer::zoomOut()
{
    scaleImage(0.8);
}

void ImageViewer::normalSize()
{
    imageLabel->adjustSize();
    scaleFactor = 1.0;
}

void ImageViewer::fitToWindow()
{
    bool fitToWindow = fitToWindowAct->isChecked();
    scrollArea->setWidgetResizable(fitToWindow);
    if (!fitToWindow)
    {
        normalSize();
    }
    updateActions();
}

void ImageViewer::about()
{
    QMessageBox::about(this, tr("About Image Viewer"),
                       tr("<p>The <b>Image Viewer</b> example shows how to combine QLabel "
                          "and QScrollArea to display an image. QLabel is typically used "
                          "for displaying a text, but it can also display an image. "
                          "QScrollArea provides a scrolling view around another widget. "
                          "If the child widget exceeds the size of the frame, QScrollArea "
                          "automatically provides scroll bars. </p><p>The example "
                          "demonstrates how QLabel's ability to scale its contents "
                          "(QLabel::scaledContents), and QScrollArea's ability to "
                          "automatically resize its contents "
                          "(QScrollArea::widgetResizable), can be used to implement "
                          "zooming and scaling features. </p><p>In addition the example "
                          "shows how to use QPainter to print an image.</p>"));
}

void ImageViewer::createActions()
{
    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcut(tr("Ctrl+O"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    printAct = new QAction(tr("&Print..."), this);
    printAct->setShortcut(tr("Ctrl+P"));
    printAct->setEnabled(false);
    connect(printAct, SIGNAL(triggered()), this, SLOT(print()));

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

    zoomInAct = new QAction(tr("Zoom &In (25%)"), this);
    zoomInAct->setShortcut(tr("Ctrl++"));
    zoomInAct->setEnabled(false);
    connect(zoomInAct, SIGNAL(triggered()), this, SLOT(zoomIn()));

    zoomOutAct = new QAction(tr("Zoom &Out (25%)"), this);
    zoomOutAct->setShortcut(tr("Ctrl+-"));
    zoomOutAct->setEnabled(false);
    connect(zoomOutAct, SIGNAL(triggered()), this, SLOT(zoomOut()));

    normalSizeAct = new QAction(tr("&Normal Size"), this);
    normalSizeAct->setShortcut(tr("Ctrl+S"));
    normalSizeAct->setEnabled(false);
    connect(normalSizeAct, SIGNAL(triggered()), this, SLOT(normalSize()));

    fitToWindowAct = new QAction(tr("&Fit to Window"), this);
    fitToWindowAct->setEnabled(false);
    fitToWindowAct->setCheckable(true);
    fitToWindowAct->setShortcut(tr("Ctrl+F"));
    connect(fitToWindowAct, SIGNAL(triggered()), this, SLOT(fitToWindow()));

    aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    aboutQtAct = new QAction(tr("About &Qt"), this);
    connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}

void ImageViewer::createMenus()
{
    fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(openAct);
    fileMenu->addAction(printAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    viewMenu = new QMenu(tr("&View"), this);
    viewMenu->addAction(zoomInAct);
    viewMenu->addAction(zoomOutAct);
    viewMenu->addAction(normalSizeAct);
    viewMenu->addSeparator();
    viewMenu->addAction(fitToWindowAct);

    helpMenu = new QMenu(tr("&Help"), this);
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);

    menuBar()->addMenu(fileMenu);
    menuBar()->addMenu(viewMenu);
    menuBar()->addMenu(helpMenu);
}

void ImageViewer::updateActions()
{
    zoomInAct->setEnabled(!fitToWindowAct->isChecked());
    zoomOutAct->setEnabled(!fitToWindowAct->isChecked());
    normalSizeAct->setEnabled(!fitToWindowAct->isChecked());
}

void ImageViewer::scaleImage(double factor)
{
    Q_ASSERT(imageLabel->pixmap());
    scaleFactor *= factor;
    imageLabel->resize(scaleFactor * imageLabel->pixmap()->size());

    adjustScrollBar(scrollArea->horizontalScrollBar(), factor);
    adjustScrollBar(scrollArea->verticalScrollBar(), factor);

    zoomInAct->setEnabled(scaleFactor < 10.0);
    zoomOutAct->setEnabled(scaleFactor > 0.05);
}

void ImageViewer::adjustScrollBar(QScrollBar *scrollBar, double factor)
{
    scrollBar->setValue(int(factor * scrollBar->value() + ((factor - 1) * scrollBar->pageStep() / 2)));
}
