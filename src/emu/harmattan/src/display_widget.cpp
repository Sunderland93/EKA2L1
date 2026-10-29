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

#include <harmattan/display_widget.h>

#include <common/log.h>

#include <harmattan/emulator.h>

#include <services/window/keys.h>

#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>

namespace eka2l1 {
namespace harmattan {
    display_widget::display_widget(emulator &emu, QWidget *parent)
        : QWidget(parent)
        , emu_(emu)
        , pressed_button_(-1) {
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

    void display_widget::paintEvent(QPaintEvent *event) {
        Q_UNUSED(event);
    }

    void display_widget::resizeEvent(QResizeEvent *event) {
        QWidget::resizeEvent(event);
        LOG_INFO(FRONTEND_UI, "UI: display widget resized to {}x{}", width(), height());
        emu_.set_surface_size(width(), height());
    }

    void display_widget::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);
        LOG_INFO(FRONTEND_UI, "UI: display widget shown, {}x{}", width(), height());
        emu_.set_surface_visible(true);
    }

    void display_widget::hideEvent(QHideEvent *event) {
        QWidget::hideEvent(event);
        LOG_INFO(FRONTEND_UI, "UI: display widget hidden");
        emu_.set_surface_visible(false);
    }

    void display_widget::mousePressEvent(QMouseEvent *event) {
        if (pressed_button_ != -1) {
            QWidget::mousePressEvent(event);
            return;
        }

        pressed_button_ = 0;
        emu_.send_touch(event->x(), event->y(), 0, 0);
        event->accept();
    }

    void display_widget::mouseMoveEvent(QMouseEvent *event) {
        if (pressed_button_ == -1) {
            QWidget::mouseMoveEvent(event);
            return;
        }

        emu_.send_touch(event->x(), event->y(), 0, 1);
        event->accept();
    }

    void display_widget::mouseReleaseEvent(QMouseEvent *event) {
        if ((pressed_button_ == -1) || !(event->button() & Qt::LeftButton)) {
            QWidget::mouseReleaseEvent(event);
            return;
        }

        pressed_button_ = -1;
        emu_.send_touch(event->x(), event->y(), 0, 2);
        event->accept();
    }

    // The frontend owns the Qt key to epoc scan code translation; raw key
    // events bypass the host keybind map so no bindings file is needed.
    static int qt_key_to_epoc_scan_code(const int key) {
        switch (key) {
        case Qt::Key_Backspace:
            return epoc::std_key_backspace;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            return epoc::std_key_tab;
        case Qt::Key_Return:
            return epoc::std_key_enter;
        case Qt::Key_Enter:
            return epoc::std_key_nkp_enter;
        case Qt::Key_Escape:
            return epoc::std_key_escape;
        case Qt::Key_Space:
            return epoc::std_key_space;
        case Qt::Key_Print:
            return epoc::std_key_print_screen;
        case Qt::Key_Pause:
            return epoc::std_key_pause;
        case Qt::Key_Home:
            return epoc::std_key_home;
        case Qt::Key_End:
            return epoc::std_key_end;
        case Qt::Key_PageUp:
            return epoc::std_key_page_up;
        case Qt::Key_PageDown:
            return epoc::std_key_page_down;
        case Qt::Key_Insert:
            return epoc::std_key_insert;
        case Qt::Key_Delete:
            return epoc::std_key_delete;
        case Qt::Key_Left:
            return epoc::std_key_left_arrow;
        case Qt::Key_Right:
            return epoc::std_key_right_arrow;
        case Qt::Key_Up:
            return epoc::std_key_up_arrow;
        case Qt::Key_Down:
            return epoc::std_key_down_arrow;
        case Qt::Key_Shift:
            return epoc::std_key_left_shift;
        case Qt::Key_Alt:
            return epoc::std_key_left_alt;
        case Qt::Key_Control:
            return epoc::std_key_left_ctrl;
        case Qt::Key_CapsLock:
            return epoc::std_key_caps_lock;
        case Qt::Key_Comma:
            return epoc::std_key_comma;
        case Qt::Key_Period:
            return epoc::std_key_full_stop;
        case Qt::Key_Slash:
            return epoc::std_key_forward_slash;
        case Qt::Key_Backslash:
            return epoc::std_key_back_slash;
        case Qt::Key_Semicolon:
            return epoc::std_key_semicolon;
        case Qt::Key_Apostrophe:
            return epoc::std_key_single_quote;
        case Qt::Key_BracketLeft:
            return epoc::std_key_square_bracket_left;
        case Qt::Key_BracketRight:
            return epoc::std_key_square_bracket_right;
        case Qt::Key_Minus:
            return epoc::std_key_minus;
        case Qt::Key_Equal:
            return epoc::std_key_equals;
        default:
            break;
        }

        if ((key >= Qt::Key_F1) && (key <= Qt::Key_F12)) {
            return epoc::std_key_f1 + (key - Qt::Key_F1);
        }

        // Scan codes in (std_key_scroll_lock, std_key_f1) are passed through
        // as-is by the guest, which covers digits and letters.
        if ((key >= 0x20) && (key <= 0x7e)) {
            return key;
        }

        return 0;
    }

    void display_widget::keyPressEvent(QKeyEvent *event) {
        if (event->isAutoRepeat()) {
            event->accept();
            return;
        }

        const int scan_code = qt_key_to_epoc_scan_code(event->key());
        if (scan_code != 0) {
            LOG_INFO(FRONTEND_UI, "UI: key pressed: Qt key 0x{:X} -> epoc scan code 0x{:X}",
                event->key(), scan_code);
            emu_.send_key(scan_code, true);
            event->accept();
            return;
        }

        LOG_WARN(FRONTEND_UI, "UI: unhandled key pressed: Qt key 0x{:X}", event->key());
        QWidget::keyPressEvent(event);
    }

    void display_widget::keyReleaseEvent(QKeyEvent *event) {
        if (event->isAutoRepeat()) {
            event->accept();
            return;
        }

        const int scan_code = qt_key_to_epoc_scan_code(event->key());
        if (scan_code != 0) {
            emu_.send_key(scan_code, false);
            event->accept();
            return;
        }

        QWidget::keyReleaseEvent(event);
    }
}
}
