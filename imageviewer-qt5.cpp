

#include <Qt>
#include <QtWidgets>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QRect>
#include <QSlider>
#include <QSpinBox>
#include <QStackedLayout>
#include <QTableWidget>
#include <QWidget>

#ifndef QT_NO_PRINTER
#include <QPrintDialog>
#endif
#include <iostream>
#include <cmath>

using namespace std;

#include "imageviewer-qt5.h"
#include "utils/QUnevenIntSpinBox.h"

#define DEFAULT_CROSS_SLIDER 49
#define DEFAULT_QUANTIZATION_SLIDER 8
#define DEFAULT_CONTRAST_SLIDER 0
#define DEFAULT_BRIGHTNESS_SLIDER 0
#define DEFAULT_FILTER_SIZE 3
#define DEFAULT_FILTER_INPUT 1
#define DEFAULT_SIGMA_INPUT 1
#define DEFAULT_DERIVATION_CHECKBOX Qt::Unchecked
#define MIN_FILTER_INPUT -100
#define MIN_SIGMA_INPUT 0.5
#define MAX_FILTER_INPUT 100
#define MAX_SIGMA_INPUT 8.0
#define MAX_FILTER_SIZE 13

#define HIST_SPACING 3
#define HIST_PADDING 20

ImageViewer::ImageViewer()
{
    image = NULL;
    originalImage = NULL;
    histogramChart = NULL;
    histogramChartView = NULL;
    isDerivationFilter = DEFAULT_DERIVATION_CHECKBOX == Qt::Checked;

    setBorderStrategy(borderPad);
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

/* 
 * SLOTS
 */

void ImageViewer::imageChanged(QImage *image)
{
    updateImageInformation(image);
    updateImageDisplay();
    renewLogging();
}

void ImageViewer::applyExampleAlgorithmClicked()
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
void ImageViewer::filterMChanged(int value)
{
    changeFilterTableHeight(value);
}
void ImageViewer::filterNChanged(int value)
{
    changeFilterTableWidth(value);
}
void ImageViewer::borderStrategyChangedPad()
{
    setBorderStrategy(borderPad);
}
void ImageViewer::borderStrategyChangedConstant()
{
    setBorderStrategy(borderConstant);
}
void ImageViewer::borderStrategyChangedMirror()
{
    setBorderStrategy(borderMirror);
}

void ImageViewer::applyFilterClicked()
{

    Eigen::MatrixXd filter = MatrixXd::Constant(filterTable->rowCount(), filterTable->columnCount(), 0);
    iterateRect(filterTable->rowCount(), filterTable->columnCount(), [this, &filter](int i, int j) {
        QWidget *cellContent = filterTable->cellWidget(i, j);
        if (cellContent != NULL)
        {
            QSpinBox *widget = dynamic_cast<QSpinBox *>(cellContent);
            if (widget != NULL)
            {
                filter(i, j) = widget->value();
            }
        }
    });
    applyFilter(filter);
}

void ImageViewer::applyGaussianFilterClicked()
{
    applyFilter(createGaussianKernel(sigmaSpinBox->value()));
}

void ImageViewer::derivationFilterStateChanged(int state)
{
    setIsDerivationFilter(state == Qt::Checked);
}

/*
 * PUBLIC
 */

// setters

void ImageViewer::setBorderStrategy(std::function<QColor(int, int, QImage *)> strategy)
{
    borderStrategy = strategy;
}

void ImageViewer::setIsDerivationFilter(bool state)
{
    isDerivationFilter = state;
}

// actions

void ImageViewer::updateImageInformation(QImage *image)
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

    // display values
    averageInfo->setNum(avg);
    varianceInfo->setNum(var);

    // find max value to scale histogram
    int max = 0;
    for (int k = 0; k < GRAY_SPECTRUM; k++)
    {
        if (hist[k] > max)
        {
            max = hist[k];
        }
    }

    // histogram
    if (histogramChart != NULL)
    {
        // delete all bars if histogram already exists
        for (auto &item : histogramChart->items())
        {
            histogramChart->removeItem(item);
        }
    }
    else
    {
        histogramChart = new QGraphicsScene();
        histogramChartView = new QGraphicsView(histogramChart);
        // flip histogram for comfort
        histogramChartView->scale(1, -1);
        stack->addWidget(histogramChartView);
        stack->setCurrentWidget(histogramChartView);
    }
    int width = histogramChartView->size().width();
    int height = histogramChartView->size().height();

    int barWidth = (int)((width - GRAY_SPECTRUM * HIST_SPACING) / (double)GRAY_SPECTRUM) + 0.5;
    int posX = 0.0;
    for (int i = 0; i < GRAY_SPECTRUM; i++)
    {
        int barHeight = (int)((hist[i] / (double)max) * (height - HIST_PADDING)) + 0.5;
        histogramChart->addRect(QRect(posX, 0, barWidth, barHeight));
        posX += barWidth + HIST_SPACING;
    }
}

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
        logFile << "drew red cross" << std::endl;
        emit imageUpdated(image);
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
        logFile << "quantized to " << value << "-bit" << std::endl;
        logFile << div << std::endl;
        emit imageUpdated(image);
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
        logFile << "added brightness of " << value << std::endl;
        emit imageUpdated(image);
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
        logFile << "changed contrast with factor of " << factor << std::endl;
        emit imageUpdated(image);
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
        logFile << "changed robust contrast with percentage of " << factor << std::endl;
        emit imageUpdated(image);
    }
}

void ImageViewer::changeFilterTableWidth(int value)
{
    filterTable->setColumnCount(value);
    setFilterTableWidgets();
}

void ImageViewer::changeFilterTableHeight(int value)
{
    filterTable->setRowCount(value);
    setFilterTableWidgets();
}

void ImageViewer::setFilterTableWidgets()
{
    iterateRect(filterTable->rowCount(), filterTable->columnCount(), [this](int i, int j) {
        QWidget *cellContent = filterTable->cellWidget(i, j);
        if (cellContent == NULL)
        {
            QSpinBox *widget = new QSpinBox();
            widget->setValue(DEFAULT_FILTER_INPUT);
            widget->setMinimum(MIN_FILTER_INPUT);
            widget->setMaximum(MAX_FILTER_INPUT);
            filterTable->setCellWidget(i, j, widget);
        }
    });
}

void ImageViewer::applyFilter(Eigen::MatrixXd filter)
{

    if (imageIsLoaded())
    {
        logFile << "Applying this filter:" << endl
                << filter << endl;

        // check if separable using SVD
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(filter, Eigen::ComputeThinU | Eigen::ComputeThinV);
        bool isSeparable = svd.rank() == 1;
        std::function<int(int x, int y, QImage *image)> filterFunc;

        if (isSeparable)
        {
            // use SVD to determine if filter is separable
            // thanks to https://web.archive.org/web/20200804115435/https://bartwronski.com/2020/02/03/separate-your-filters-svd-and-low-rank-approximation-of-image-filters/
            VectorXd H_x = svd.matrixV()(Eigen::all, 0) * sqrt(svd.singularValues()[0]);
            VectorXd H_y = svd.matrixU()(Eigen::all, 0) * sqrt(svd.singularValues()[0]);
            int x_h = (int)(H_x.size()) / 2;
            int y_h = (int)(H_y.size()) / 2;
            logFile << "Filter is separable!" << std::endl;
            logFile << "H_x:" << endl
                    << H_x << endl;
            logFile << "H_y:" << endl
                    << H_y << endl;

            std::vector<std::vector<double>> buffer(image->width(), std::vector<double>(image->height(), 0));

            // apply 1D filter x dimenstion
            iteratePixels([this, H_x, x_h, &buffer](int x, int y) {
                double total_x = 0;
                double n = 0;
                for (int i = 0; i < H_x.size(); i++)
                {
                    QColor pixel = getFilterPixel(x - x_h + i, y, originalImage);
                    int intensity;
                    intensity = isDerivationFilter ? rgbToGray(pixel) : std::get<0>(rgbToYCbCr(pixel));
                    total_x += H_x(i) * intensity;
                    n += abs(H_x(i));
                }
                buffer[x][y] = isDerivationFilter ? total_x : total_x / n;
            });

            // apply 1D filter y dimenstion
            iteratePixels([this, H_y, y_h, buffer](int x, int y) {
                double n = 0;
                double total_y = 0;
                for (int i = 0; i < H_y.size(); i++)
                {
                    int intensity;
                    int y_pos = y - y_h + i;
                    if (isOutOfRange(x, y_pos, image->width(), image->height()))
                    {
                        QColor pixel = borderStrategy(x, y_pos, originalImage);
                        intensity = isDerivationFilter ? rgbToGray(pixel) : std::get<0>(rgbToYCbCr(pixel));
                    }
                    else
                    {
                        intensity = buffer[x][y_pos];
                    }
                    total_y += H_y(i) * intensity;
                    n += abs(H_y(i));
                }
                applyFilterValue(total_y, x, y, n, originalImage, image);
            });
        }
        else
        {
            double n = 0.0;
            iterateRect(filter.rows(), filter.cols(), [&filter, &n](int x, int y) {
                n += abs(filter(x, y));
            });
            int x_h = (int)(filter.rows()) / 2;
            int y_h = (int)(filter.cols()) / 2;
            iteratePixels([this, x_h, y_h, filter, n](int x, int y) {
                double value = 0;
                for (int u = 0; u < filter.cols(); u++)
                {
                    for (int v = 0; v < filter.rows(); v++)
                    {
                        auto yCbCr = rgbToYCbCr(getFilterPixel(x - x_h + u, y - y_h + v, originalImage));
                        value += filter(v, u) * std::get<0>(yCbCr);
                    }
                }
                applyFilterValue(value, x, y, n, originalImage, image);
            });
        }

        emit imageUpdated(image);
    }
}

// helpers

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

    return QColor(
        clamp(r, 0, GRAY_SPECTRUM - 1),
        clamp(g, 0, GRAY_SPECTRUM - 1),
        clamp(b, 0, GRAY_SPECTRUM - 1));
}
QColor ImageViewer::rgbToGrayColor(QColor color)
{
    int value = rgbToGray(color.red(), color.green(), color.blue());
    return QColor(value, value, value);
}

void ImageViewer::iterateRect(int width, int height, std::function<void(int, int)> func)
{
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < height; j++)
        {
            func(i, j);
        }
    }
}

void ImageViewer::iteratePixels(std::function<void(int, int)> func)
{
    iterateRect(image->width(), image->height(), func);
}

int ImageViewer::clamp(int value, int min, int max)
{
    if (value < min)
    {
        return min;
    }
    if (value > max)
    {
        return max;
    }
    return value;
}

Eigen::MatrixXd ImageViewer::createGaussianKernel(double sigma)
{
    int center = (int)(sigma * 3.0);
    Eigen::VectorXd h = Eigen::VectorXd(2 * center + 1);
    double sigma2 = sigma * sigma;
    for (int i = 0; i < h.size(); i++)
    {
        double r = center - i;
        h[i] = (double)(exp(-0.5 * (r * r) / sigma2));
    }
    return h * h.transpose();
}

QColor ImageViewer::getFilterPixel(int x, int y, QImage *image)
{
    if (isOutOfRange(x, y, image->width(), image->height()))
    {
        return borderStrategy(x, y, image);
    }
    return image->pixelColor(x, y);
}

bool ImageViewer::isOutOfRange(int x, int y, int width, int height)
{
    return x < 0 || y < 0 || x > width - 1 || y > height - 1;
}

#pragma GCC diagnostic push
// for common interface these variables are not used
#pragma GCC diagnostic ignored "-Wunused-parameter"
QColor ImageViewer::borderPad(int x, int y, QImage *image)
{
    return QColor(0, 0, 0);
}
#pragma GCC diagnostic pop

QColor ImageViewer::borderConstant(int x, int y, QImage *image)
{
    if (x > image->width() - 1)
    {
        x = image->width() - 1;
    }
    else if (x < 0)
    {
        x = 0;
    }
    if (y > image->height() - 1)
    {
        y = image->height() - 1;
    }
    else if (y < 0)
    {
        y = 0;
    }
    return image->pixelColor(x, y);
}
QColor ImageViewer::borderMirror(int x, int y, QImage *image)
{
    if (x > image->width() - 1)
    {
        int dist = x - image->width();
        x = image->width() - 1 - dist;
    }
    else if (x < 0)
    {
        x = -x;
    }
    if (y > image->height() - 1)
    {
        int dist = y - image->height();
        y = image->height() - 1 - dist;
    }
    else if (y < 0)
    {
        y = -y;
    }
    return image->pixelColor(x, y);
}

void ImageViewer::applyFilterValue(double value, int x, int y, double n, QImage *source, QImage *target)
{
    QColor newPixelValue;
    if (isDerivationFilter)
    {
        value = clamp(value + 127, 0, GRAY_SPECTRUM - 1);
        newPixelValue = QColor(value, value, value);
    }
    else
    {
        value /= n;
        auto old_color = rgbToYCbCr(source->pixelColor(x, y));
        std::get<0>(old_color) = value;
        newPixelValue = yCbCrToRgb(old_color);
    }
    target->setPixelColor(x, y, newPixelValue);
}

/*
 * PRIVATE
 */
void ImageViewer::setDefaults()
{
    crossSlider->blockSignals(true);
    crossSlider->setValue(DEFAULT_CROSS_SLIDER);
    crossSlider->blockSignals(false);
    quantizationSlider->blockSignals(true);
    quantizationSlider->setValue(DEFAULT_QUANTIZATION_SLIDER);
    quantizationSlider->blockSignals(false);
}

void ImageViewer::resetImage()
{
    delete image;
    image = new QImage(originalImage->copy());
}
void ImageViewer::generateControlPanels()
{
    /*
     * ex 1
     */

    m_option_panel1 = new QWidget();
    m_option_layout1 = new QVBoxLayout();
    m_option_panel1->setLayout(m_option_layout1);

    button1 = new QPushButton();
    button1->setText("Apply algorithm");
    QObject::connect(button1, SIGNAL(clicked()), this, SLOT(applyExampleAlgorithmClicked()));

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

    /*
     * ex 2
     */

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
    contrastSlider->setRange(0, 10);
    contrastSlider->setValue(DEFAULT_BRIGHTNESS_SLIDER);
    QObject::connect(contrastSlider, SIGNAL(valueChanged(int)), this, SLOT(contrastSliderValueChanged(int)));

    QVBoxLayout *contrastLayout = new QVBoxLayout();
    contrastLayout->addWidget(new QLabel("Contrast"));
    contrastLayout->addWidget(contrastSlider);

    robustContrastSlider = new QSlider(Qt::Horizontal);
    robustContrastSlider->setRange(0, 100);
    robustContrastSlider->setValue(DEFAULT_BRIGHTNESS_SLIDER);
    QObject::connect(robustContrastSlider, SIGNAL(valueChanged(int)), this, SLOT(robustContrastSliderValueChanged(int)));

    QVBoxLayout *robustContrastLayout = new QVBoxLayout();
    robustContrastLayout->addWidget(new QLabel("Robust Contrast"));
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

    /*
     * ex 3
     */

    QWidget *m_option_panel3 = new QWidget();
    QVBoxLayout *m_option_layout3 = new QVBoxLayout();
    m_option_panel3->setLayout(m_option_layout3);

    // border strategy

    QGroupBox *borderStrategyGroup = new QGroupBox(tr("Border Strategy"));
    QVBoxLayout *borderStrategyLayout = new QVBoxLayout;

    QRadioButton *pad = new QRadioButton(tr("Zero Padding"));
    QRadioButton *constant = new QRadioButton(tr("Constant Border"));
    QRadioButton *mirror = new QRadioButton(tr("Mirrored Border"));

    borderStrategyLayout->addWidget(pad);
    borderStrategyLayout->addWidget(constant);
    borderStrategyLayout->addWidget(mirror);
    borderStrategyLayout->addStretch(1);
    borderStrategyGroup->setLayout(borderStrategyLayout);

    QObject::connect(pad, SIGNAL(clicked()), SLOT(borderStrategyChangedPad()));
    QObject::connect(constant, SIGNAL(clicked()), SLOT(borderStrategyChangedConstant()));
    QObject::connect(mirror, SIGNAL(clicked()), SLOT(borderStrategyChangedMirror()));
    pad->setChecked(true);

    // normal Filter

    QGroupBox *filterGroup = new QGroupBox(tr("Normal Filter"));
    QVBoxLayout *filterLayout = new QVBoxLayout;

    QHBoxLayout *filterMInfo = new QHBoxLayout();
    filterM = new QUnevenIntSpinBox();
    filterM->setMinimum(1);
    filterM->setMaximum(MAX_FILTER_SIZE);
    filterM->setSingleStep(2);
    filterM->setValue(DEFAULT_FILTER_SIZE);
    filterMInfo->addWidget(new QLabel("Filter M Dimension: "));
    filterMInfo->addWidget(filterM);
    QObject::connect(filterM, SIGNAL(valueChanged(int)), this, SLOT(filterMChanged(int)));

    QHBoxLayout *filterNInfo = new QHBoxLayout();
    filterN = new QUnevenIntSpinBox();
    filterN->setMinimum(1);
    filterN->setMaximum(MAX_FILTER_SIZE);
    filterN->setSingleStep(2);
    filterN->setValue(DEFAULT_FILTER_SIZE);
    filterNInfo->addWidget(new QLabel("Filter N Dimension: "));
    filterNInfo->addWidget(filterN);
    QObject::connect(filterN, SIGNAL(valueChanged(int)), this, SLOT(filterNChanged(int)));

    filterTable = new QTableWidget(DEFAULT_FILTER_SIZE, DEFAULT_FILTER_SIZE);
    setFilterTableWidgets();

    QCheckBox *isDerivationCheckBox = new QCheckBox("Is derivation Filter?");
    isDerivationCheckBox->setCheckState(DEFAULT_DERIVATION_CHECKBOX);
    QObject::connect(isDerivationCheckBox, SIGNAL(stateChanged(int)), SLOT(derivationFilterStateChanged(int)));

    applyFilterButton = new QPushButton("Apply filter");
    QObject::connect(applyFilterButton, SIGNAL(clicked()), SLOT(applyFilterClicked()));

    filterLayout->addLayout(filterMInfo);
    filterLayout->addLayout(filterNInfo);
    filterLayout->addWidget(filterTable);
    filterLayout->addWidget(isDerivationCheckBox);
    filterLayout->addWidget(applyFilterButton);
    filterGroup->setLayout(filterLayout);

    // gaussian filter

    QGroupBox *gaussianFilterGroup = new QGroupBox(tr("Gaussian Filter"));
    QVBoxLayout *gaussianFilterLayout = new QVBoxLayout;

    QHBoxLayout *sigmaLayout = new QHBoxLayout();
    sigmaSpinBox = new QDoubleSpinBox();
    sigmaSpinBox->setMinimum(MIN_SIGMA_INPUT);
    sigmaSpinBox->setMaximum(MAX_SIGMA_INPUT);
    sigmaSpinBox->setValue(DEFAULT_SIGMA_INPUT);
    sigmaSpinBox->setSingleStep(0.1);
    sigmaLayout->addWidget(new QLabel("Sigma: "));
    sigmaLayout->addWidget(sigmaSpinBox);

    QPushButton *applyGaussianFilterButton = new QPushButton("Apply gaussian filter");
    QObject::connect(applyGaussianFilterButton, SIGNAL(clicked()), SLOT(applyGaussianFilterClicked()));

    gaussianFilterLayout->addLayout(sigmaLayout);
    gaussianFilterLayout->addWidget(applyGaussianFilterButton);
    gaussianFilterGroup->setLayout(gaussianFilterLayout);

    // add widgets

    m_option_layout3->addWidget(borderStrategyGroup);
    m_option_layout3->addWidget(filterGroup);
    m_option_layout3->addWidget(gaussianFilterGroup);

    tabWidget->addTab(m_option_panel3, "3");

    /*
     * ex 4
     */

    QWidget *m_option_panel4 = new QWidget();
    QVBoxLayout *m_option_layout4 = new QVBoxLayout();
    m_option_panel4->setLayout(m_option_layout4);

    tabWidget->addTab(m_option_panel4, "4");

    /*
     * ex 5
     */

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
