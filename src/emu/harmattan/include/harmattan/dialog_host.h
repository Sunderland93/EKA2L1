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

#include <QObject>

class QWidget;

namespace eka2l1 {
namespace harmattan {
    /**
     * @brief Lives on the Qt UI thread and runs the guest-requested dialogs.
     *
     * The emulator core calls eka2l1::drivers::ui from the OS thread; requests
     * are parked in the bridge state and marshalled here with queued
     * invocations.
     */
    class dialog_host : public QObject {
        Q_OBJECT

    public:
        explicit dialog_host(QWidget *parent_widget);

    public slots:
        void show_input_dialog();
        void show_question_dialog();

    private:
        QWidget *parent_widget_;
    };
}
}
