// src/gtk3/ParameterPanel.cpp (GTK3 port)

#include "ParameterPanel.h"

#include <cstdio>
#include <cstring>

using PortInfo = LV2Plugin::PortInfo;

// ── ParameterPanel ────────────────────────────────────────────────────────────

ParameterPanel::ParameterPanel(GtkWidget* parent_window) : parent_(parent_window) {
    // GTK3: gtk_scrolled_window_new takes hadjustment, vadjustment args
    scroll_ = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll_, TRUE);
    gtk_widget_set_hexpand(scroll_, TRUE);

    box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    // GTK3: gtk_container_add instead of gtk_scrolled_window_set_child
    gtk_container_add(GTK_CONTAINER(scroll_), box_);
}

void ParameterPanel::clear() {
    // GTK3: iterate via gtk_container_get_children instead of gtk_widget_get_first_child
    GList* children = gtk_container_get_children(GTK_CONTAINER(box_));
    for (GList* l = children; l != nullptr; l = l->next)
        gtk_container_remove(GTK_CONTAINER(box_), GTK_WIDGET(l->data));
    g_list_free(children);

    for (auto* cd : controlDataList_)
        delete cd;
    controlDataList_.clear();
}

void ParameterPanel::build(const std::vector<PortInfo>& ports) {
    clear();

    for (const auto& port : ports) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_margin_start(row, 5);
        gtk_widget_set_margin_end  (row, 5);
        gtk_widget_set_margin_top  (row, 5);
        gtk_widget_set_margin_bottom(row, 5);

        // Label
        GtkWidget* lbl = gtk_label_new(port.label.c_str());
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_size_request(lbl, 150, -1);
        gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);

        GtkWidget* ctrl = nullptr;

        switch (port.type) {
        // ── Float / Enum ──────────────────────────────────────────────────
        case PortInfo::ControlType::Float: {
            if (port.isEnum && !port.scalePoints.empty()) {
                // GTK3: GtkComboBoxText instead of GtkDropDown
                ctrl = gtk_combo_box_text_new();
                for (const auto& sp : port.scalePoints)
                    gtk_combo_box_text_append_text(
                        GTK_COMBO_BOX_TEXT(ctrl), sp.second.c_str());

                // Set initial selected item matching currentVal
                int sel = 0;
                for (int i = 0; i < (int)port.scalePoints.size(); ++i) {
                    if (std::abs(port.scalePoints[i].first - port.currentVal) < 1e-5f) {
                        sel = i;
                        break;
                    }
                }
                gtk_combo_box_set_active(GTK_COMBO_BOX(ctrl), sel);

                // EnumData: holds scale points for the changed callback
                struct EnumData {
                    ParameterPanel* panel;
                    uint32_t portIndex;
                    std::vector<std::pair<float, std::string>> scalePoints;
                };
                auto* ed = new EnumData{this, port.portIndex, port.scalePoints};
                controlDataList_.push_back(reinterpret_cast<ControlData*>(ed));

                // GTK3: "changed" signal instead of "notify::selected"
                g_signal_connect_data(ctrl, "changed",
                    G_CALLBACK(+[](GtkComboBox* combo, gpointer data) {
                        auto* ed2 = static_cast<EnumData*>(data);
                        int idx = gtk_combo_box_get_active(combo);
                        if (idx >= 0 && idx < (int)ed2->scalePoints.size()
                                && ed2->panel->valueCb_)
                            ed2->panel->valueCb_(ed2->portIndex,
                                                  ed2->scalePoints[idx].first);
                    }),
                    ed,
                    [](gpointer /*data*/, GClosure*) { /* freed via controlDataList_ */ },
                    G_CONNECT_DEFAULT);

            } else {
                // Continuous range → GtkScale (identical in GTK3)
                const double step = (port.maxVal - port.minVal) / 200.0;
                ctrl = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
                                                (double)port.minVal,
                                                (double)port.maxVal,
                                                step > 0 ? step : 0.01);
                gtk_range_set_value(GTK_RANGE(ctrl), (double)port.currentVal);
                gtk_scale_set_draw_value(GTK_SCALE(ctrl), TRUE);
                gtk_scale_set_value_pos(GTK_SCALE(ctrl), GTK_POS_RIGHT);
                gtk_widget_set_hexpand(ctrl, TRUE);

                auto* cd = new ControlData{this, port.portIndex, {}};
                controlDataList_.push_back(cd);
                g_signal_connect(ctrl, "value-changed",
                                 G_CALLBACK(onScaleChanged), cd);
            }
            break;
        }

        // ── Toggle ────────────────────────────────────────────────────────
        case PortInfo::ControlType::Toggle: {
            ctrl = gtk_check_button_new();
            // GTK3: GtkCheckButton inherits from GtkToggleButton; use GTK_TOGGLE_BUTTON cast
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctrl),
                                         port.currentVal > 0.5f ? TRUE : FALSE);
            auto* cd = new ControlData{this, port.portIndex, {}};
            controlDataList_.push_back(cd);
            // GTK3: "toggled" signal on GtkToggleButton
            g_signal_connect(ctrl, "toggled",
                             G_CALLBACK(onToggleChanged), cd);
            break;
        }

        // ── Trigger ───────────────────────────────────────────────────────
        case PortInfo::ControlType::Trigger: {
            ctrl = gtk_button_new_with_label("Fire");
            auto* cd = new ControlData{this, port.portIndex, {}};
            controlDataList_.push_back(cd);
            g_signal_connect(ctrl, "clicked",
                             G_CALLBACK(onTriggerClicked), cd);
            break;
        }

        // ── Atom file path ────────────────────────────────────────────────
        case PortInfo::ControlType::AtomFilePath: {
            ctrl = gtk_button_new_with_label("Browse…");
            auto* cd = new ControlData{this, port.portIndex, port.writableUri};
            controlDataList_.push_back(cd);
            g_signal_connect(ctrl, "clicked",
                             G_CALLBACK(onBrowseClicked), cd);
            break;
        }
        } // switch

        if (ctrl) {
            if (port.type == PortInfo::ControlType::Float && !port.isEnum)
                gtk_widget_set_hexpand(ctrl, TRUE);
            gtk_box_pack_start(GTK_BOX(row), ctrl, TRUE, TRUE, 0);
            gtk_widget_set_margin_start(ctrl, 5);
            gtk_widget_set_margin_end(ctrl, 5);
            gtk_widget_set_margin_top(ctrl, 5);
            gtk_widget_set_margin_bottom(ctrl, 5);
        }

        gtk_box_pack_start(GTK_BOX(box_), row, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box_),
            gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    }

    // GTK3: widgets added after the initial show_all are hidden by default
    gtk_widget_show_all(scroll_);
}

// ── Static signal handlers ────────────────────────────────────────────────────

/*static*/
void ParameterPanel::onScaleChanged(GtkRange* range, gpointer data) {
    auto* cd = static_cast<ControlData*>(data);
    if (cd->panel->valueCb_)
        cd->panel->valueCb_(cd->portIndex,
                             static_cast<float>(gtk_range_get_value(range)));
}

/*static*/
// GTK3: callback receives GtkToggleButton* instead of GtkCheckButton*
void ParameterPanel::onToggleChanged(GtkToggleButton* btn, gpointer data) {
    auto* cd = static_cast<ControlData*>(data);
    if (cd->panel->valueCb_)
        cd->panel->valueCb_(cd->portIndex,
                             gtk_toggle_button_get_active(btn) ? 1.0f : 0.0f);
}

/*static*/
void ParameterPanel::onTriggerClicked(GtkButton* /*btn*/, gpointer data) {
    auto* cd = static_cast<ControlData*>(data);
    if (cd->panel->valueCb_)
        cd->panel->valueCb_(cd->portIndex, 1.0f);
}

/*static*/
void ParameterPanel::onBrowseClicked(GtkButton* /*btn*/, gpointer data) {
    auto* cd = static_cast<ControlData*>(data);
    ParameterPanel* self = cd->panel;

    // GTK3: synchronous GtkFileChooserDialog instead of async GtkFileDialog
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Open file",
        GTK_WINDOW(self->parent_),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open",   GTK_RESPONSE_ACCEPT,
        nullptr);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (path && self->fileCb_)
            self->fileCb_(cd->writableUri, std::string(path));
        g_free(path);
    }
    gtk_widget_destroy(dialog);
}
