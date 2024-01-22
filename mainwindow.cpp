#include "mainwindow.h"
#include "chartwindow.h"

#include "./ui_mainwindow.h"

#include <QFileDialog>

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
    chartingMenu = menuBar->addMenu(tr("&Charting"));
    addChartAction = new QAction(tr("&Open"), this);
    chartingMenu->addAction(addChartAction);
    /*set add chart interaction*/
    connect(addChartAction, &QAction::triggered, this, &MainWindow::addChartMenuAction);

    //set layout
    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addWidget(loggerTextBox);

    centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    logBasic("MainWindow initialized\n");
}

void MainWindow::addChartMenuAction()
{
    ChartWindow* chartWindow = new ChartWindow(this);
    chartWindow->show();
}

MainWindow::~MainWindow()
{
    delete ui;
}
