#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "statuswindow.h"
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QMetaObject>
#include <QStringList>
#include <windows.h>
#include <string>
#include <vector>
#include <sstream>

std::vector<std::string> parts;
size_t currentIndex = 0;
HHOOK keyboardHook;

// Static pointer to main window for cross-callback signaling
MainWindow* g_mainWindow = nullptr;
StatusWindow* g_statusWindow = nullptr;

std::string GetClipboardText() {
    std::string result;
    if (OpenClipboard(NULL)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText) {
                result = pszText;
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
    return result;
}

void SetClipboardText(const std::string& text) {
    const size_t len = text.length() + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    memcpy(GlobalLock(hMem), text.c_str(), len);
    GlobalUnlock(hMem);
    OpenClipboard(0);
    EmptyClipboard();
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
}

std::vector<std::string> SplitInput(const std::string& input, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;
    while (getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && pKey->vkCode == 'V') {
            if (!parts.empty() && currentIndex < parts.size()) {
                SetClipboardText(parts[currentIndex]);
                currentIndex++;

                if (g_statusWindow) {
                    QMetaObject::invokeMethod(
                        g_statusWindow,
                        [index = (int)currentIndex]() { g_statusWindow->updateStatus(index); },
                        Qt::QueuedConnection
                    );
                }
                // ❗ Only finish AFTER last part pasted
                if (currentIndex >= parts.size()) {
                    if (g_statusWindow) {
                        QMetaObject::invokeMethod(g_statusWindow, "close", Qt::QueuedConnection);
                        QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
                    }
                    PostQuitMessage(0);
                }
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void StartHookThread() {
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (!keyboardHook) {
        qDebug() << "Failed to install keyboard hook!";
        return;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(keyboardHook);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    g_mainWindow = this;

    setWindowFlags(Qt::FramelessWindowHint | Qt::MSWindowsFixedSizeDialogHint);
    setAttribute(Qt::WA_TranslucentBackground);

    connect(ui->delimiter, &QLineEdit::returnPressed, this, &MainWindow::processDelimiter);
}

MainWindow::~MainWindow()
{
    g_mainWindow = nullptr;
    delete ui;
}
void MainWindow::processDelimiter()
{
    QString delimiterText = ui->delimiter->text();
    if (delimiterText.isEmpty()) {
        this->close();
        return;
    }

    std::string clipboardContent = GetClipboardText();
    if (clipboardContent.empty()) {
        this->close();
        return;
    }

    // Handle \n input as newline
    char delimiterChar;
    if (delimiterText == "\\n" || delimiterText == "\n")
        delimiterChar = '\n';
    else delimiterChar = delimiterText.toStdString()[0];

    std::vector<std::string> vecParts = SplitInput(clipboardContent, delimiterChar);
    if (vecParts.empty()) return;

    QStringList parts;
    for (const auto& part : vecParts)
        parts << QString::fromStdString(part);

    // Create and show StatusWindow
    StatusWindow* statusWindow = new StatusWindow();
    statusWindow->setParts(parts);
    statusWindow->show();

    g_statusWindow = statusWindow;

    connect(statusWindow, &StatusWindow::finished, qApp, &QApplication::quit);

    // Set up global clipboard cycling state
    ::parts.clear();
    for (const auto& part : vecParts) ::parts.push_back(part);
    currentIndex = 0;
    SetClipboardText(::parts[0]);

    QThread* hookThread = QThread::create([] {
        StartHookThread();
    });
    hookThread->start();
    this->hide();
}
