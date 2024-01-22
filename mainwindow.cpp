#include "mainwindow.h"

#include "./ui_mainwindow.h"

#include <QFileDialog>
#include <unordered_map>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    resize(QSize(1920, 800));

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
    //customPlot->xAxis->setLabel("x Axis");
    //customPlot->yAxis->setLabel("y Axis");

    /*set zoom and scrolling interactions*/
    customPlot->setInteractions(QCP::iRangeZoom | QCP::iRangeDrag | QCP::iSelectAxes | QCP::iSelectPlottables);
    //mouse move events for displaying data tooltip when mouse hovers over
    connect(customPlot, &QCustomPlot::mouseMove, this, &MainWindow::onMouseMove);

    candlestickPlot = new QCPFinancial(customPlot->xAxis, customPlot->yAxis);
    candlestickPlot->setChartStyle(QCPFinancial::csCandlestick);
    candlestickPlot->setBrushPositive(QColor(0, 255, 0));
    candlestickPlot->setBrushNegative(QColor(255, 0, 0));
    candlestickPlot->setName("Candles");

    //intialize volume bar graph
    //TODO: make it so we can display oscillators graph as well
    volumeAxisRect = new QCPAxisRect(customPlot);
    customPlot->plotLayout()->addElement(1, 0, volumeAxisRect);
    volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 150));
    volumeAxisRect->axis(QCPAxis::atBottom)->setLayer("axes");
    volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer("grid");
    //doesnt seem to work. May just have to accept that it can scroll
    // and zoom below zero
    volumeAxisRect->axis(QCPAxis::atLeft)->setRangeLower(0);

    //bring the 2 plots close together
    customPlot->plotLayout()->setRowSpacing(1);
    volumeAxisRect->setAutoMargins(QCP::msLeft|QCP::msRight|QCP::msBottom);
    volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));

    //intialize volume bars
    volumeBars = new QCPBars(volumeAxisRect->axis(QCPAxis::atBottom), volumeAxisRect->axis(QCPAxis::atLeft));
    volumeBars->setName("Volume");

    //tie the volume and candlestick axis range zoom/drags together
    connect(customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), volumeAxisRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
    connect(volumeAxisRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange)), customPlot->xAxis, SLOT(setRange(QCPRange)));

    //set layout
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

        //TODO: make it so we can add multiple indicators
        indicatorsPlot = customPlot->addGraph();
        indicatorsPlot->setPen(QPen(Qt::blue));

        // assuming keys always contain timestamp, price_open, price_high, price_low, price_close, volume
        log("%1 timestamps read\n", csvDataMap.at("timestamp").size());

        for (int i = 0; i < csvDataMap.at("timestamp").size(); i++)
        {
            candlestickPlot->addData(csvDataMap.at("timestamp")[i], csvDataMap.at("price_open")[i],
                csvDataMap.at("price_high")[i], csvDataMap.at("price_low")[i], csvDataMap.at("price_close")[i]);
            volumeBars->addData(csvDataMap.at("timestamp")[i], csvDataMap.at("volume")[i]);

            if (csvDataMap.at("sma9").at(i) > -1e6)
            {
                indicatorsPlot->addData(csvDataMap.at("timestamp")[i], csvDataMap.at("sma9").at(i));
                updateMinMaxAxisValues(csvDataMap["timestamp"][i], csvDataMap["price_high"][i]);
            }
        }

        //automatically converts the unixtimestamp into string datetime
        QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker(new QCPAxisTickerDateTime);
        customPlot->xAxis->setTicker(dateTimeTicker);
        customPlot->xAxis->setTickLength(csvDataMap.at("timestamp").size());
        dateTimeTicker->setDateTimeFormat("yyyy-MM-dd\nhh:mm:ss");

        QSharedPointer<QCPAxisTickerDateTime> volumeDateTimeTicker(new QCPAxisTickerDateTime);
        volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(volumeDateTimeTicker);
        volumeDateTimeTicker->setDateTimeFormat("yyyy-MM-dd\nhh:mm:ss");

        candlestickPlot->setWidth(50);
        candlestickPlot->rescaleAxes();
        volumeBars->setWidth(50);
        volumeBars->rescaleAxes();
        customPlot->legend->setVisible(true);
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

void MainWindow::onMouseMove(QMouseEvent* event)
{
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

            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(static_cast<quint64>(it->key));
            QString dateString = dateTime.toString("yyyy-MM-dd hh:mm:ss");
            // Format floating-point numbers with 2 decimal places precision
            QString openString = QString::number(it->open, 'f', 2);
            QString highString = QString::number(it->high, 'f', 2);
            QString lowString = QString::number(it->low, 'f', 2);
            QString closeString = QString::number(it->close, 'f', 2);

            QString trackerText = QString("Timestamp: %1\nO: %2\nH: %3\nL: %4\nC: %5")
                                      .arg(dateString, openString, highString, lowString, closeString);

            customPlot->setToolTip(trackerText);
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
