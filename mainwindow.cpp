#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "statuswindow.h"
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QMetaObject>
#include <QStringList>
#include <QKeyEvent>
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

std::string CleanPart(const std::string& input, bool addNewline)
{
    // Only trim newline characters (not spaces or tabs)
    size_t start = 0;
    while (start < input.size() && (input[start] == '\n' || input[start] == '\r'))
        ++start;

    size_t end = input.size();
    while (end > start && (input[end - 1] == '\n' || input[end - 1] == '\r'))
        --end;

    std::string trimmed = input.substr(start, end - start);

    // Skip if trimmed is empty
    if (trimmed.empty())
        return "";

    if (addNewline) {
        trimmed += "\n";
    }

    return trimmed;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

        // 🔐 ESC = immediate exit
        if (pKey->vkCode == VK_ESCAPE) {
            qDebug() << "ESC detected. Quitting...";
            QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
            if (g_statusWindow)
                QMetaObject::invokeMethod(g_statusWindow, "close", Qt::QueuedConnection);
            PostQuitMessage(0);
            return 1;
        }

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

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        qDebug() << "ESC pressed. Closing MainWindow.";
        this->close();          // ✅ Closes MainWindow
        // or: QApplication::quit(); // if you want to quit entire app
    } else {
        QMainWindow::keyPressEvent(event); // pass to base class
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    g_mainWindow = this;

    ui->addNewLine->raise();

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

    bool addNewline = ui->addNewLine->isChecked();
    std::vector<std::string> rawParts = SplitInput(clipboardContent, delimiterChar);
    if (rawParts.empty()) return;

    QStringList parts;
    for (const auto& part : rawParts)
        parts << QString::fromStdString(part);

    // Create and show StatusWindow
    StatusWindow* statusWindow = new StatusWindow();
    statusWindow->setParts(parts);
    statusWindow->show();

    g_statusWindow = statusWindow;

    connect(statusWindow, &StatusWindow::finished, qApp, &QApplication::quit);

    // Set up global clipboard cycling state
    ::parts.clear();
    for (const auto& raw : rawParts) {
        std::string cleaned = CleanPart(raw, addNewline);
        if (!cleaned.empty()) {
            ::parts.push_back(cleaned);
        }
    }
    currentIndex = 0;
    SetClipboardText(::parts[0]);

    QThread* hookThread = QThread::create([] {
        StartHookThread();
    });
    hookThread->start();
    this->hide();
}
