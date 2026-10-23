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

#include <harmattan/main_window.h>

#include <harmattan/display_widget.h>
#include <harmattan/emulator.h>
#include <harmattan/install_dialog.h>

#include <common/cvt.h>
#include <common/path.h>

#include <kernel/kernel.h>
#include <services/applist/applist.h>
#include <services/applist/icons.h>
#include <services/window/screen.h>
#include <services/window/window.h>
#include <system/devices.h>
#include <system/epoc.h>

#include <utils/apacmd.h>

#include <QApplication>
#include <QCloseEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#if defined(Q_WS_X11)
#include <QX11Info>
#include <X11/Xlib.h>
#endif

namespace eka2l1 {
namespace harmattan {
    static constexpr int WARE_APP_UID_START = 0x10300000;
    static constexpr int ICON_SIZE = 72;

    static QImage icon_to_image(const app_icon_image &icon) {
        QImage image(icon.size.x, icon.size.y, QImage::Format_ARGB32_Premultiplied);

        for (int y = 0; y < icon.size.y; y++) {
            QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            const std::uint8_t *src = icon.pixels.data() + static_cast<std::size_t>(y) * icon.size.x * 4;

            for (int x = 0; x < icon.size.x; x++) {
                line[x] = qRgba(src[0], src[1], src[2], src[3]);
                src += 4;
            }
        }

        return image;
    }

    static bool is_hidden_system_app(const apa_app_registry &reg) {
        return (reg.land_drive == drive_z) && (reg.mandatory_info.uid < WARE_APP_UID_START);
    }

    main_window::main_window(emulator &emu, QWidget *parent)
        : QMainWindow(parent)
        , emu_(emu)
        , stack_(nullptr)
        , launcher_page_(nullptr)
        , emulator_page_(nullptr)
        , app_list_(nullptr)
        , status_label_(nullptr)
        , displayer_(nullptr) {
        build_ui();

        emu_.boot_finished_callback = [this](bool success) {
            emit boot_finished(success);
        };

        connect(this, SIGNAL(boot_finished(bool)), this, SLOT(on_boot_finished(bool)), Qt::QueuedConnection);
        connect(this, SIGNAL(guest_app_exited()), this, SLOT(on_guest_app_exited()), Qt::QueuedConnection);
    }

    main_window::~main_window() {
        emu_.boot_finished_callback = nullptr;
        emu_.shutdown();
    }

    void main_window::build_ui() {
        setWindowTitle("EKA2L1");

        stack_ = new QStackedWidget(this);

        launcher_page_ = new QWidget(stack_);
        QVBoxLayout *launcher_layout = new QVBoxLayout(launcher_page_);
        launcher_layout->setContentsMargins(12, 8, 12, 8);
        launcher_layout->setSpacing(8);

        QHBoxLayout *header_layout = new QHBoxLayout();
        QLabel *title_label = new QLabel("EKA2L1", launcher_page_);
        QFont title_font = title_label->font();
        title_font.setPointSizeF(title_font.pointSizeF() * 1.6);
        title_font.setBold(true);
        title_label->setFont(title_font);

        status_label_ = new QLabel(launcher_page_);
        status_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        header_layout->addWidget(title_label);
        header_layout->addStretch(1);
        header_layout->addWidget(status_label_);
        launcher_layout->addLayout(header_layout);

        app_list_ = new QListWidget(launcher_page_);
        app_list_->setViewMode(QListView::IconMode);
        app_list_->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
        app_list_->setGridSize(QSize(ICON_SIZE + 44, ICON_SIZE + 56));
        app_list_->setResizeMode(QListView::Adjust);
        app_list_->setMovement(QListView::Static);
        app_list_->setWordWrap(true);
        app_list_->setUniformItemSizes(true);
        app_list_->setSpacing(2);
        launcher_layout->addWidget(app_list_, 1);

        QHBoxLayout *button_layout = new QHBoxLayout();
        button_layout->setSpacing(8);

        QPushButton *launch_button = new QPushButton(tr("Launch"), launcher_page_);
        QPushButton *install_app_button = new QPushButton(tr("Install app"), launcher_page_);
        QPushButton *install_device_button = new QPushButton(tr("Install device"), launcher_page_);
        QPushButton *quit_button = new QPushButton(tr("Quit"), launcher_page_);

        button_layout->addWidget(launch_button, 2);
        button_layout->addWidget(install_app_button, 2);
        button_layout->addWidget(install_device_button, 2);
        button_layout->addWidget(quit_button, 1);
        launcher_layout->addLayout(button_layout);

        emulator_page_ = new QWidget(stack_);
        QVBoxLayout *emulator_layout = new QVBoxLayout(emulator_page_);
        emulator_layout->setContentsMargins(0, 0, 0, 0);
        emulator_layout->setSpacing(0);

        displayer_ = new display_widget(emu_, emulator_page_);
        emulator_layout->addWidget(displayer_, 1);

        QHBoxLayout *toolbar_layout = new QHBoxLayout();
        toolbar_layout->setContentsMargins(8, 4, 8, 4);
        toolbar_layout->setSpacing(8);

        QPushButton *back_button = new QPushButton(tr("< Apps"), emulator_page_);
        back_button->setMinimumHeight(44);
        toolbar_layout->addWidget(back_button);
        toolbar_layout->addStretch(1);

        emulator_layout->addLayout(toolbar_layout);

        stack_->addWidget(launcher_page_);
        stack_->addWidget(emulator_page_);
        stack_->setCurrentWidget(launcher_page_);
        setCentralWidget(stack_);

        setStyleSheet(
            "QListWidget { background: #101010; color: #f0f0f0; }"
            "QListWidget::item { border-radius: 6px; padding: 4px; }"
            "QListWidget::item:selected { background: #2d5f8b; }"
            "QPushButton { min-height: 44px; padding: 4px 12px; }"
            "QLabel { color: #d0d0d0; }");

        connect(launch_button, SIGNAL(clicked()), this, SLOT(on_launch_clicked()));
        connect(back_button, SIGNAL(clicked()), this, SLOT(on_back_clicked()));
        connect(install_app_button, SIGNAL(clicked()), this, SLOT(on_install_app_clicked()));
        connect(install_device_button, SIGNAL(clicked()), this, SLOT(on_install_device_clicked()));
        connect(quit_button, SIGNAL(clicked()), qApp, SLOT(quit()));
        connect(app_list_, SIGNAL(itemActivated(QListWidgetItem *)), this, SLOT(on_item_activated()));
    }

    void main_window::initialise() {
        start_emulator();
    }

    void main_window::show_status(const QString &text) {
        if (status_label_) {
            status_label_->setText(text);
        }
    }

    void main_window::start_emulator() {
        if (emu_.is_running()) {
            return;
        }

        app_list_->clear();
        apps_.clear();

        if (!emu_.boot_ui_side()) {
            show_status(tr("No device installed"));
            QMessageBox::information(this, tr("No device installed"),
                tr("Install a device firmware (VPL or an archive) to start using EKA2L1."));
            return;
        }

        show_status(tr("Starting..."));

        // Force the native window to exist before the EGL surface is created.
        WId native_id = displayer_->winId();

        void *display_connection = nullptr;
        std::uint32_t visual_id = 0;

#if defined(Q_WS_X11)
        QX11Info x11_info = displayer_->x11Info();
        display_connection = x11_info.display();

        if (x11_info.visual()) {
            visual_id = static_cast<std::uint32_t>(
                XVisualIDFromVisual(reinterpret_cast<Visual *>(x11_info.visual())));
        }
#endif

        if (!emu_.start_threads(reinterpret_cast<void *>(static_cast<std::uintptr_t>(native_id)),
                display_connection, visual_id, displayer_->width(), displayer_->height())) {
            emu_.shutdown();
            show_status(tr("Failed to start"));
            QMessageBox::critical(this, tr("Failed to start"),
                tr("The graphics driver could not be initialised."));
        }
    }

    void main_window::restart_emulator() {
        emu_.shutdown();
        start_emulator();
    }

    void main_window::refresh_app_list() {
        app_list_->clear();
        apps_.clear();

        emu_.rescan_apps();

        const std::lock_guard<std::recursive_mutex> guard(emu_.session_mutex);

        if (!emu_.applist) {
            show_status(tr("App list unavailable"));
            return;
        }

        std::vector<apa_app_registry> &registries = emu_.applist->get_registerations();
        const std::string icon_cache_root = eka2l1::add_path(emu_.conf.storage, "cache/icons");

        for (std::size_t i = 0; i < registries.size(); i++) {
            apa_app_registry &reg = registries[i];

            if (reg.caps.is_hidden || is_hidden_system_app(reg)) {
                continue;
            }

            const std::u16string caption = reg.mandatory_info.long_caption.to_std_string(nullptr);
            const QString name = QString::fromStdString(common::ucs2_to_utf8(caption));

            QIcon icon;
            if (auto icon_image = emu_.applist->get_host_icon(reg, icon_cache_root); icon_image.has_value()) {
                icon = QIcon(QPixmap::fromImage(icon_to_image(icon_image.value())));
            }

            QListWidgetItem *item = new QListWidgetItem(icon, name, app_list_);
            item->setData(Qt::UserRole, static_cast<int>(i));
            item->setSizeHint(QSize(ICON_SIZE + 44, ICON_SIZE + 56));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);

            apps_.append({ static_cast<int>(i), name });
        }

        if (apps_.isEmpty()) {
            show_status(tr("No applications installed"));
        } else {
            show_status(tr("%1 applications").arg(apps_.size()));
        }
    }

    void main_window::on_boot_finished(bool success) {
        if (!success) {
            show_status(tr("Boot failed"));
            QMessageBox::critical(this, tr("Boot failed"),
                tr("The device could not be initialised. Check the log for details."));
            return;
        }

        emu_.add_screen_draw_callback();
        refresh_app_list();
        stack_->setCurrentWidget(launcher_page_);
    }

    void main_window::on_launch_clicked() {
        QListWidgetItem *item = app_list_->currentItem();
        if (!item) {
            QMessageBox::information(this, tr("No selection"), tr("Select an application first."));
            return;
        }

        const int index = item->data(Qt::UserRole).toInt();
        bool launched = false;

        {
            const std::lock_guard<std::recursive_mutex> guard(emu_.session_mutex);

            if (emu_.applist) {
                std::vector<apa_app_registry> &registries = emu_.applist->get_registerations();

                if ((index >= 0) && (index < static_cast<int>(registries.size()))) {
                    epoc::apa::command_line command;
                    launched = emu_.applist->launch_app(registries[index], command, nullptr,
                        [this](kernel::process *) {
                            emit guest_app_exited();
                        });
                }
            }
        }

        if (!launched) {
            QMessageBox::warning(this, tr("Launch failed"),
                tr("The selected application could not be started."));
            return;
        }

        stack_->setCurrentWidget(emulator_page_);
        displayer_->setFocus();
    }

    void main_window::on_item_activated() {
        on_launch_clicked();
    }

    void main_window::on_back_clicked() {
        stack_->setCurrentWidget(launcher_page_);
    }

    void main_window::on_guest_app_exited() {
        stack_->setCurrentWidget(launcher_page_);
    }

    void main_window::on_install_device_clicked() {
        bool was_running = emu_.is_running();

        if (was_running) {
            const int answer = QMessageBox::question(this, tr("Install device"),
                tr("The emulator must be stopped to install a device. Continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

            if (answer != QMessageBox::Yes) {
                return;
            }

            emu_.shutdown();
        }

        const bool installed = install_device_dialog(this, emu_);

        if (installed) {
            emu_.conf.device = static_cast<int>(emu_.symsys->get_device_manager()->total()) - 1;
            emu_.conf.serialize(false);
            emu_.symsys->get_device_manager()->save_devices();
            start_emulator();
        } else if (was_running) {
            start_emulator();
        }
    }

    void main_window::on_install_app_clicked() {
        if (!emu_.is_running() || !emu_.symsys) {
            QMessageBox::information(this, tr("Not running"),
                tr("Boot a device before installing applications."));
            return;
        }

        if (install_package_dialog(this, emu_)) {
            refresh_app_list();
        }
    }

    void main_window::closeEvent(QCloseEvent *event) {
        emu_.boot_finished_callback = nullptr;
        emu_.shutdown();
        event->accept();
    }
}
}
