#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "qcustomplot.h"

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    void openFileActionFn();
    void updateMinMaxAxisValues(double x, double y);
    void onMouseWheel(QWheelEvent* event);
    void onMousePress(QMouseEvent* event);
    void onMouseMove(QMouseEvent* event);
    void onMouseRelease(QMouseEvent* event);

    void appendLog(const QString& message, const QTextCharFormat& format)
    {
        QTextCursor cursor(loggerTextBox->document());
        cursor.movePosition(QTextCursor::End);
        cursor.setCharFormat(format);
        cursor.insertText(message);
        loggerTextBox->setTextCursor(cursor);
    }

    void logBasic(const QString& message)
    {
        QTextCursor cursor(loggerTextBox->document());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(message);
        loggerTextBox->setTextCursor(cursor);
    }

    template <typename... Args>
    void log(const QString& format, Args... args)
    {
        QString formattedMsg;
        QTextStream stream(&formattedMsg);
        stream.setFieldAlignment(QTextStream::AlignLeft);

        stream << format.arg(args...);

        QTextCharFormat customFormat;
        customFormat.setFontWeight(QFont::Bold);
        customFormat.setForeground(Qt::blue);

        appendLog(formattedMsg, customFormat);
    }

    void readCsv(const QString& filePath);

private:
    Ui::MainWindow* ui;
    QWidget* centralWidget;
    QMenuBar* menuBar;
    QMenu* fileMenu;
    QAction* openFileAction;

    QCustomPlot* customPlot;
    QPointF* onMousePressRecordPoint;
    double minX, minY, maxX, maxY;

    QCPFinancial* candlestickPlot;
    QCPGraph* indicatorsPlot;

    QPlainTextEdit* loggerTextBox;

    std::unordered_map<QString, QVector<double>> csvDataMap;
    std::unordered_map<int, QString> keyIndices;
};
#endif // MAINWINDOW_H
