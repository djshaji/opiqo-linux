// src/gtk4/ParameterPanel.cpp

#include "ParameterPanel.h"

#include <cstdio>
#include <cstring>

using PortInfo = LV2Plugin::PortInfo;

// ── ParameterPanel ────────────────────────────────────────────────────────────

ParameterPanel::ParameterPanel(GtkWidget* parent_window) : parent_(parent_window) {
    scroll_ = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll_, TRUE);
    gtk_widget_set_hexpand(scroll_, TRUE);

    box_ = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_), box_);
}

void ParameterPanel::clear() {
    // Remove all rows from the box
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(box_)) != nullptr)
        gtk_box_remove(GTK_BOX(box_), child);

    // Free heap-allocated callback data
    for (auto* cd : controlDataList_)
        delete cd;
    controlDataList_.clear();
}

void ParameterPanel::build(const std::vector<PortInfo>& ports) {
    clear();

    for (const auto& port : ports) {
        // ── Row: label + control ─────────────────────────────────────────
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_margin_start(row, 4);
        gtk_widget_set_margin_end (row, 4);
        gtk_widget_set_margin_top (row, 2);

        // Label
        GtkWidget* lbl = gtk_label_new(port.label.c_str());
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_set_size_request(lbl, 150, -1);
        gtk_box_append(GTK_BOX(row), lbl);

        // Control widget
        GtkWidget* ctrl = nullptr;

        switch (port.type) {
        // ── Float / Enum ──────────────────────────────────────────────────
        case PortInfo::ControlType::Float: {
            if (port.isEnum && !port.scalePoints.empty()) {
                // Enumeration → GtkDropDown
                std::vector<const char*> labels;
                for (const auto& sp : port.scalePoints)
                    labels.push_back(sp.second.c_str());
                labels.push_back(nullptr);

                GtkStringList* sl = gtk_string_list_new(labels.data());
                ctrl = gtk_drop_down_new(G_LIST_MODEL(sl), nullptr);

                // Set initial selected item matching currentVal
                guint sel = 0;
                for (guint i = 0; i < port.scalePoints.size(); ++i) {
                    if (std::abs(port.scalePoints[i].first - port.currentVal) < 1e-5f) {
                        sel = i;
                        break;
                    }
                }
                gtk_drop_down_set_selected(GTK_DROP_DOWN(ctrl), sel);

                auto* cd = new ControlData{this, port.portIndex, {}};
                controlDataList_.push_back(cd);

                // Store scale-point values alongside the widget for the callback
                // We need the panel and the PortInfo data; capture via a per-port closure
                struct EnumData {
                    ParameterPanel* panel;
                    uint32_t portIndex;
                    std::vector<std::pair<float, std::string>> scalePoints;
                };
                auto* ed = new EnumData{this, port.portIndex, port.scalePoints};
                controlDataList_.push_back(reinterpret_cast<ControlData*>(ed)); // tracked for deletion

                g_signal_connect_data(ctrl, "notify::selected",
                    G_CALLBACK(+[](GObject* dd, GParamSpec*, gpointer data) {
                        auto* ed2 = static_cast<EnumData*>(data);
                        guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd));
                        if (idx < ed2->scalePoints.size() && ed2->panel->valueCb_)
                            ed2->panel->valueCb_(ed2->portIndex,
                                                  ed2->scalePoints[idx].first);
                    }),
                    ed,
                    [](gpointer /*data*/, GClosure*) { /* ed freed via controlDataList_ */ },
                    G_CONNECT_DEFAULT);

            } else {
                // Continuous range → GtkScale
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
            gtk_check_button_set_active(GTK_CHECK_BUTTON(ctrl),
                                        port.currentVal > 0.5f ? TRUE : FALSE);
            auto* cd = new ControlData{this, port.portIndex, {}};
            controlDataList_.push_back(cd);
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
            gtk_box_append(GTK_BOX(row), ctrl);
        }

        gtk_box_append(GTK_BOX(box_), row);
        gtk_box_append(GTK_BOX(box_), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    }
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
void ParameterPanel::onToggleChanged(GtkCheckButton* btn, gpointer data) {
    auto* cd = static_cast<ControlData*>(data);
    if (cd->panel->valueCb_)
        cd->panel->valueCb_(cd->portIndex,
                             gtk_check_button_get_active(btn) ? 1.0f : 0.0f);
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

    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open file");

    struct BrowseCtx {
        ParameterPanel* panel;
        std::string     writableUri;
    };
    auto* ctx = new BrowseCtx{self, cd->writableUri};

    gtk_file_dialog_open(dialog,
                         GTK_WINDOW(self->parent_),
                         nullptr,
                         [](GObject* src, GAsyncResult* res, gpointer user_data) {
                             auto* ctx2 = static_cast<BrowseCtx*>(user_data);
                             GError* err = nullptr;
                             GFile* file = gtk_file_dialog_open_finish(
                                               GTK_FILE_DIALOG(src), res, &err);
                             if (file) {
                                 char* path = g_file_get_path(file);
                                 if (path && ctx2->panel->fileCb_)
                                     ctx2->panel->fileCb_(ctx2->writableUri,
                                                           std::string(path));
                                 g_free(path);
                                 g_object_unref(file);
                             }
                             if (err) g_error_free(err);
                             delete ctx2;
                         },
                         ctx);
    g_object_unref(dialog);
}
