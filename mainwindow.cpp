#include "mainwindow.h"

#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <unordered_map>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(QSize(800, 600));

    loggerTextBox = new QPlainTextEdit(this);
    loggerTextBox->setReadOnly(true);
    loggerTextBox->setMaximumHeight(100);

    menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    fileMenu = menuBar->addMenu(tr("&File"));
    openFileAction = new QAction(tr("&Open"), this);
    fileMenu->addAction(openFileAction);
    /*set open file interaction*/
    connect(openFileAction, &QAction::triggered, this, &MainWindow::openFileActionFn);

    customPlot = new QCustomPlot(this);
    customPlot->setMouseTracking(true);
    customPlot->axisRect()->setupFullAxesBox();
    customPlot->yAxis->setSelectableParts(QCPAxis::spAxis);
    customPlot->xAxis->setLabel("x Axis");
    customPlot->yAxis->setLabel("y Axis");
    // set Y-axis to be interactive for range dragging
    // customPlot->axisRect()->setRangeZoom(Qt::Vertical);

    /*set zoom and scrolling interactions*/
    customPlot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag | QCP::iSelectAxes | QCP::iSelectPlottables);

    // seems like zooming and scrolling still works without these callbacks?
    // connect(customPlot, SIGNAL(onMousePress(QMouseEvent*)), this, SLOT(onMousePress()));
    // connect(customPlot, &QCustomPlot::mouseWheel, this, &MainWindow::onMouseWheel);
    // connect(customPlot, &QCustomPlot::mousePress, this, &MainWindow::onMousePress);
    connect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);
    // connect(customPlot, &QCustomPlot::mouseRelease, this, &MainWindow::onMouseRelease);

    candlestickPlot = new QCPFinancial(customPlot->xAxis, customPlot->yAxis);
    candlestickPlot->setChartStyle(QCPFinancial::csCandlestick);
    candlestickPlot->setBrushPositive(QColor(0, 255, 0));
    candlestickPlot->setBrushNegative(QColor(255, 0, 0));

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(customPlot);
    mainLayout->addWidget(loggerTextBox);

    centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    onMousePressRecordPoint = new QPointF();

    logBasic("MainWindow initialized\n");
}

void MainWindow::openFileActionFn()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath());
    try
    {
        readCsv(filePath);

        minX = std::numeric_limits<double>::max();
        minY = std::numeric_limits<double>::max();
        maxX = std::numeric_limits<double>::min();
        maxY = std::numeric_limits<double>::min();

        indicatorsPlot = customPlot->addGraph();
        indicatorsPlot->setPen(QPen(Qt::blue));

        // assuming keys always contain timestamp, price_open, price_high, price_low, price_close, volume
        log("%1 timestamps read\n", csvDataMap.at("timestamp").size());
        for (int i = 0; i < csvDataMap.at("timestamp").size(); i++)
        {
            candlestickPlot->addData(csvDataMap.at("timestamp")[i], csvDataMap.at("price_open")[i],
                csvDataMap.at("price_high")[i], csvDataMap.at("price_low")[i], csvDataMap.at("price_close")[i]);
            if (csvDataMap.at("sma9").at(i) > -1e6)
            {
                indicatorsPlot->addData(csvDataMap.at("timestamp")[i], csvDataMap.at("sma9").at(i));
                updateMinMaxAxisValues(csvDataMap["timestamp"][i], csvDataMap["price_high"][i]);
            }
        }
        candlestickPlot->setWidth(50);
        candlestickPlot->rescaleAxes();
        customPlot->replot();
    }
    catch (std::exception& e)
    {
        log("Caught exception: %1", e.what());
    }
}

void MainWindow::updateMinMaxAxisValues(double x, double y)
{
    minX = qMin(minX, x);
    maxX = qMax(maxX, x);
    minY = qMin(minY, y);
    maxY = qMax(maxY, y);
}

void MainWindow::readCsv(const QString& filePath)
{
    if (!filePath.isEmpty())
    {
        // QVector<double> timestamps, open, high, low, close;
        csvDataMap.clear();
        keyIndices.clear();
        // int keysCount = 0;

        QFile csvfile(filePath);
        if (csvfile.open(QIODevice::ReadOnly))
        {
            log("Opening %1\n", filePath);
            QString readData = csvfile.readAll();
            QStringList lines = readData.split("\n");
            bool readKeys = true;
            for (const auto& line : lines)
            {
                if (readKeys)
                {
                    QStringList keys = line.split(",");
                    logBasic("Found csvkeys: ");
                    int keyIndex = 0;
                    for (const auto& k : keys)
                    {
                        if (k.length() == 0)
                        {
                            break; // csv file might have a trailing comma
                        }
                        log("%1, ", k);
                        csvDataMap[k] = QVector<double>();
                        keyIndices[keyIndex++] = k;
                        // keysCount++;
                    }
                    logBasic("\n");
                    readKeys = false;
                }
                else
                {
                    if (line.size() == 0)
                    {
                        continue; // could be an empty line at the end of file
                    }
                    QStringList tokens = line.split(",");
                    int keyIndex = 0;
                    for (const auto& [index, key] : keyIndices)
                    // for(int index = 0; index < keysCount; index++)
                    {
                        if (tokens.at(index).size() > 0)
                            csvDataMap.at(key).append(tokens.at(index).toDouble());
                        else
                            csvDataMap.at(key).append(-1e6); // use -1e6 as magic value for NaN values for any trailing
                                                             // indicators that aren't calculated yet
                    }
                }
            }
        }
        else
        {
            log("Error oepning %1\n", filePath);
        }
    }
}

// seems like zooming and scrolling still works without these callbacks?
#if 0
void  MainWindow::onMouseWheel(QWheelEvent* event)
{
    // Handle zooming
    double sensitivity = 0.00001;
    double factor = (event->angleDelta().y() > 0) ? (1.0 + sensitivity) : (1.0 - sensitivity);

    customPlot->axisRect()->axis(QCPAxis::atBottom)->scaleRange(factor, 1.0);
    customPlot->axisRect()->axis(QCPAxis::atLeft)->scaleRange(factor, 1.0);

    event->accept();
}

void MainWindow::onMousePress(QMouseEvent* event)
{
    // Handle dragging start
    /*if (event->button() == Qt::RightButton)
    {
        onMousePressRecordPoint->setX(event->position().x());
        onMousePressRecordPoint->setY(event->position().y());

        customPlot->setInteraction(QCP::iRangeDrag, true);
        event->accept();
    }*/
    //if (event->button() == Qt::LeftButton && customPlot->axisRect()->axis(QCPAxis::atLeft)->selectedParts().testFlag(QCPAxis::spAxis))
    /*if (customPlot->yAxis->selectedParts().testFlag(QCPAxis::spAxis))
    {
        //customPlot->axisRect()->setRangeZoom(Qt::Vertical);
        onMousePressRecordPoint->setX(event->position().x());
        onMousePressRecordPoint->setY(event->position().y());
        event->accept();
    }*/
}
#endif
void MainWindow::onMouseMove(QMouseEvent* event)
{
    // Handle dragging
    /*if (customPlot->testAttribute(Qt::WA_UnderMouse) && customPlot->interactions() == QCP::iRangeDrag)
    {
        customPlot->axisRect()->axis(QCPAxis::atBottom)->moveRange(event->position().x() -
    onMousePressRecordPoint->x()); customPlot->axisRect()->axis(QCPAxis::atLeft)->moveRange(onMousePressRecordPoint->y()
    - event->position().y()); event->accept();
    }*/
    /*double sensitivity = 0.00001;
    auto mousePosDelta = event->pos().y() - onMousePressRecordPoint->y();
    double factor = (mousePosDelta > 0) ? (1.0 + sensitivity) : (1.0 - sensitivity);
    if (customPlot->testAttribute(Qt::WA_UnderMouse) && customPlot->axisRect()->rangeZoom().testFlag(Qt::Vertical))
    {
        customPlot->axisRect()->axis(QCPAxis::atLeft)->scaleRange(factor, 1.0);
        event->accept();
    }*/
    if (candlestickPlot == nullptr)
    {
        return;
    }
    QVariant details;
    QCPFinancialDataContainer::const_iterator it = candlestickPlot->data()->constEnd();
    if (candlestickPlot->selectTest(event->position(), true, &details))
    {
        QCPDataSelection dataPoints = details.value<QCPDataSelection>();
        if (dataPoints.dataPointCount() > 0)
        {
            it = candlestickPlot->data()->at(dataPoints.dataRange().begin());
            QString trackerText = QString("Timestamp: %1\nO: %2\nH: %3\nL: %4\nC: %4")
                                      .arg(it->key)
                                      .arg(it->close)
                                      .arg(it->high)
                                      .arg(it->low)
                                      .arg(it->close);
            customPlot->setToolTip(trackerText);
        }
    }
}
#if 0
void  MainWindow::onMouseRelease(QMouseEvent* event)
{
    // Handle dragging end
    /*if (event->button() == Qt::RightButton)
    {
        customPlot->setInteraction(QCP::iRangeDrag, false);
        event->accept();
    }*/
    /*if (event->button() == Qt::LeftButton && customPlot->axisRect()->rangeZoom().testFlag(Qt::Vertical))
    {
        //customPlot->axisRect()->setRangeZoom(Qt::);  // Disable range zooming after release
        event->accept();
    }*/
}
#endif
MainWindow::~MainWindow()
{
    delete ui;
}
