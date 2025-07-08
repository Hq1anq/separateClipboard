# ClipboardCycler

🚀 Minimal clipboard split-and-paste tool for Windows (C++ / Qt)

## ✨ Features

- Split clipboard content using any delimiter (e.g. `:` or `\n`)
- Automatically updates clipboard part when you press `Ctrl+V`
- Optional: Add newline `\n` at end of each part
- Shows remaining parts in a floating overlay
- Keyboard-only: just type, press Enter, and paste
- Closes automatically after last part or when pressing `ESC`

## 🛠 Built With

- C++17
- Qt 6.x (Widgets)
- WinAPI (Clipboard & Keyboard hook)

## 📦 How to Use

1. Copy text to clipboard
2. Run `ClipboardCycler.exe`
3. Type delimiter (e.g. `:` or `\n`) and press Enter
4. Paste anywhere using `Ctrl+V` — it cycles each part

## 🔐 Note

This tool uses a **global keyboard hook** (to listen for Ctrl+V).
It's safe and only active while running.

## 🧠 Example

Clipboard:
email1@example.com:email2@example.com:email3@example.com

Delimiter: `:`

Then each `Ctrl+V` pastes one email.

---

Created by TrinhHoangGiang – free & open ✨
