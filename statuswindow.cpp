#include "statuswindow.h"
#include "ui_statuswindow.h"
#include <QScreen>
#include <QGuiApplication>
#include <QApplication>

StatusWindow::StatusWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::StatusWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);

    moveToBottomRight();
}

StatusWindow::~StatusWindow()
{
    delete ui;
}

void StatusWindow::setParts(const QStringList &parts)
{
    partList = parts;
    updateStatus(0);
}

void StatusWindow::updateStatus(int currentIndex)
{
    if (currentIndex < partList.size()) {
        int remaining = partList.size() - currentIndex - 1;
        ui->count->setText(QString("Remaining: %1").arg(remaining));

        // Split the string by ":" and take the first part
        QString fullPart = partList[currentIndex];
        QString preview = fullPart.split(":").first();

        // Truncate if longer than 15 characters
        if (preview.length() > 15) {
            preview = preview.left(15) + "...";
        }

        ui->next->setText("Next: " + preview);
    }
}

void StatusWindow::moveToBottomRight()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    QRect screenGeometry = screen->availableGeometry();
    int x = screenGeometry.right() - width() - 20;
    int y = screenGeometry.bottom() - height() - 20;
    move(x, y);
}
