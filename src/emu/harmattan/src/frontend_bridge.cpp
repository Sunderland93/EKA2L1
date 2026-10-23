/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <harmattan/dialog_host.h>
#include <harmattan/frontend_bridge.h>

#include <common/applauncher.h>
#include <common/cvt.h>
#include <common/log.h>
#include <drivers/ui/input_dialog.h>

#include <QDesktopServices>
#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QWidget>

#include <mutex>
#include <utility>

namespace eka2l1 {
namespace harmattan {
    // Qt 4's moc cannot parse this translation unit, so dialog_host lives in
    // its own header. Everything below is bridge state referenced from the
    // drivers::ui entry points at the bottom of this file.
    static std::mutex bridge_mutex;
    static QWidget *dialog_parent = nullptr;

    static drivers::ui::input_dialog_complete_callback pending_input_callback;
    static drivers::ui::yes_no_dialog_complete_callback pending_question_callback;

    static std::u16string pending_input_text;
    static std::u16string pending_question_text;
    static std::u16string pending_question_button1;
    static std::u16string pending_question_button2;
    static int pending_input_max_length = 0;

    static QPointer<QInputDialog> active_input_dialog;
    static dialog_host *host = nullptr;

    static QString to_qstring(const std::u16string &str) {
        if (str.empty()) {
            return QString();
        }

        const std::string utf8 = common::ucs2_to_utf8(str);
        return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size()));
    }

    static std::u16string from_qstring(const QString &str) {
        const ushort *data = str.utf16();
        return std::u16string(reinterpret_cast<const char16_t *>(data),
            static_cast<std::size_t>(str.length()));
    }

    dialog_host::dialog_host(QWidget *parent_widget)
        : QObject(nullptr)
        , parent_widget_(parent_widget) {
    }

    void dialog_host::show_input_dialog() {
        drivers::ui::input_dialog_complete_callback callback;
        std::u16string initial_text;
        int max_length = 0;

        {
            std::lock_guard<std::mutex> guard(bridge_mutex);
            callback = std::move(pending_input_callback);
            pending_input_callback = nullptr;
            initial_text = std::move(pending_input_text);
            pending_input_text.clear();
            max_length = pending_input_max_length;
            pending_input_max_length = 0;
        }

        if (!callback) {
            return;
        }

        QInputDialog *dialog = new QInputDialog(parent_widget_);
        dialog->setInputMode(QInputDialog::TextInput);
        dialog->setWindowTitle(QObject::tr("Input"));
        dialog->setLabelText(QObject::tr("Enter text:"));
        dialog->setTextValue(to_qstring(initial_text));

        if (max_length > 0) {
            if (QLineEdit *edit = dialog->findChild<QLineEdit *>()) {
                edit->setMaxLength(max_length);
            }
        }

        {
            std::lock_guard<std::mutex> guard(bridge_mutex);
            active_input_dialog = dialog;
        }

        const int result = dialog->exec();

        {
            std::lock_guard<std::mutex> guard(bridge_mutex);
            active_input_dialog = nullptr;
        }

        callback(result == QDialog::Accepted ? from_qstring(dialog->textValue())
                                             : std::u16string());
        delete dialog;
    }

    void dialog_host::show_question_dialog() {
        drivers::ui::yes_no_dialog_complete_callback callback;
        std::u16string text;
        std::u16string button1;
        std::u16string button2;

        {
            std::lock_guard<std::mutex> guard(bridge_mutex);
            callback = std::move(pending_question_callback);
            pending_question_callback = nullptr;
            text = std::move(pending_question_text);
            pending_question_text.clear();
            button1 = std::move(pending_question_button1);
            pending_question_button1.clear();
            button2 = std::move(pending_question_button2);
            pending_question_button2.clear();
        }

        if (!callback) {
            return;
        }

        const int result = QMessageBox::question(parent_widget_, QObject::tr("Question"),
            to_qstring(text), to_qstring(button1), to_qstring(button2), 0, 1);

        callback(result == 0 ? 0 : 1);
    }

    void initialise_frontend_bridge(QWidget *dialog_parent_widget) {
        if (host) {
            return;
        }

        dialog_parent = dialog_parent_widget;
        host = new dialog_host(dialog_parent_widget);
    }

    void shutdown_frontend_bridge() {
        delete host;
        host = nullptr;
        dialog_parent = nullptr;

        std::lock_guard<std::mutex> guard(bridge_mutex);
        active_input_dialog = nullptr;
        pending_input_callback = nullptr;
        pending_question_callback = nullptr;
    }
}
}

namespace eka2l1 {
namespace drivers {
namespace ui {
    bool open_input_view(const std::u16string &initial_text, const int max_len,
        input_dialog_complete_callback complete_callback) {
        if (!harmattan::host || !complete_callback) {
            return true;
        }

        {
            std::lock_guard<std::mutex> guard(harmattan::bridge_mutex);
            harmattan::pending_input_callback = std::move(complete_callback);
            harmattan::pending_input_text = initial_text;
            harmattan::pending_input_max_length = max_len;
        }

        QMetaObject::invokeMethod(harmattan::host, "show_input_dialog", Qt::QueuedConnection);
        return true;
    }

    void close_input_view() {
        QInputDialog *dialog = nullptr;

        {
            std::lock_guard<std::mutex> guard(harmattan::bridge_mutex);
            dialog = harmattan::active_input_dialog.data();
        }

        if (dialog) {
            QMetaObject::invokeMethod(dialog, "reject", Qt::QueuedConnection);
        }
    }

    void show_yes_no_dialog(const std::u16string &text, const std::u16string &button1_text,
        const std::u16string &button2_text, yes_no_dialog_complete_callback complete_callback) {
        if (!harmattan::host || !complete_callback) {
            return;
        }

        {
            std::lock_guard<std::mutex> guard(harmattan::bridge_mutex);
            harmattan::pending_question_callback = std::move(complete_callback);
            harmattan::pending_question_text = text;
            harmattan::pending_question_button1 = button1_text;
            harmattan::pending_question_button2 = button2_text;
        }

        QMetaObject::invokeMethod(harmattan::host, "show_question_dialog", Qt::QueuedConnection);
    }
}
}
}

namespace eka2l1 {
namespace common {
    bool launch_browser(const std::string &url) {
        return QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
    }
}
}
