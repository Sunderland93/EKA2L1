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

#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>

class QLabel;
class QListWidget;
class QStackedWidget;

namespace eka2l1 {
namespace harmattan {
    struct emulator;
    class display_widget;

    class main_window : public QMainWindow {
        Q_OBJECT

    public:
        explicit main_window(emulator &emu, QWidget *parent = nullptr);
        ~main_window();

        /**
         * @brief Boot the emulator if a device is installed and populate the list.
         */
        void initialise();

    signals:
        void boot_finished(bool success);
        void guest_app_exited();

    protected:
        void closeEvent(QCloseEvent *event) override;

    private slots:
        void on_boot_finished(bool success);
        void on_guest_app_exited();
        void on_launch_clicked();
        void on_item_activated();
        void on_back_clicked();
        void on_install_device_clicked();
        void on_install_app_clicked();

    private:
        void build_ui();
        void start_emulator();
        void restart_emulator();
        void refresh_app_list();
        void show_status(const QString &text);

        emulator &emu_;

        QStackedWidget *stack_;
        QWidget *launcher_page_;
        QWidget *emulator_page_;

        QListWidget *app_list_;
        QLabel *status_label_;
        display_widget *displayer_;

        struct app_entry {
            int registry_index;
            QString name;
        };

        QVector<app_entry> apps_;
    };
}
}
