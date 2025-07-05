#ifndef STATUSWINDOW_H
#define STATUSWINDOW_H

#include <QMainWindow>
#include <QStringList>

namespace Ui {
class StatusWindow;
}

class StatusWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit StatusWindow(QWidget *parent = nullptr);
    ~StatusWindow();

    void setParts(const QStringList& parts);  // Load parts
    void updateStatus(int currentIndex);      // Update GUI
    void moveToBottomRight();                 // Positioning

signals:
    void finished();  // Signal to quit app

private:
    Ui::StatusWindow *ui;
    QStringList partList;
    int currentIndex = 0;
};

#endif // STATUSWINDOW_H
