// src/gtk4/PresetBar.cpp

#include "PresetBar.h"

// ── PresetBar ─────────────────────────────────────────────────────────────────

PresetBar::PresetBar(GtkWidget* parent_box) {
    buildWidgets(parent_box);
}

void PresetBar::buildWidgets(GtkWidget* parent_box) {
    bar_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(bar_, 8);
    gtk_widget_set_margin_end(bar_, 8);
    gtk_widget_set_margin_top(bar_, 4);
    gtk_widget_set_margin_bottom(bar_, 4);

    // ── "Preset:" label ───────────────────────────────────────────────────
    GtkWidget* label = gtk_label_new("Preset:");
    gtk_box_append(GTK_BOX(bar_), label);

    // ── Preset dropdown ───────────────────────────────────────────────────
    const char* empty[] = {nullptr};
    stringList_ = gtk_string_list_new(empty);
    presetDrop_ = gtk_drop_down_new(G_LIST_MODEL(stringList_), nullptr);
    gtk_widget_set_size_request(presetDrop_, 200, -1);
    gtk_widget_set_tooltip_text(presetDrop_, "Select a saved preset");
#if GTK_CHECK_VERSION(4, 12, 0)
    gtk_drop_down_set_enable_search(GTK_DROP_DOWN(presetDrop_), TRUE);
#endif
    g_signal_connect_swapped(presetDrop_, "notify::selected",
        G_CALLBACK(+[](PresetBar* self, GParamSpec*) { self->onDropdownChanged(); }),
        this);
    gtk_box_append(GTK_BOX(bar_), presetDrop_);

    gtk_box_append(GTK_BOX(bar_), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    // ── Name entry ────────────────────────────────────────────────────────
    GtkWidget* nameLabel = gtk_label_new("Name:");
    gtk_box_append(GTK_BOX(bar_), nameLabel);

    nameEntry_ = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(nameEntry_), "Preset name");
    gtk_widget_set_size_request(nameEntry_, 160, -1);
    gtk_widget_set_tooltip_text(nameEntry_, "Name for saving the current state");
    gtk_box_append(GTK_BOX(bar_), nameEntry_);

    gtk_box_append(GTK_BOX(bar_), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    // ── Load button ───────────────────────────────────────────────────────
    loadBtn_ = gtk_button_new_with_label("Load");
    gtk_widget_set_tooltip_text(loadBtn_, "Apply the selected preset to all slots");
    g_signal_connect_swapped(loadBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) { if (self->loadCb_) self->loadCb_(); }),
        this);
    gtk_box_append(GTK_BOX(bar_), loadBtn_);

    // ── Save button ───────────────────────────────────────────────────────
    saveBtn_ = gtk_button_new_with_label("Save");
    gtk_widget_set_tooltip_text(saveBtn_, "Save current plugin state as a named preset");
    g_signal_connect_swapped(saveBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) {
            if (self->saveCb_) self->saveCb_(self->getCurrentName());
        }),
        this);
    gtk_box_append(GTK_BOX(bar_), saveBtn_);

    // ── Delete button ─────────────────────────────────────────────────────
    deleteBtn_ = gtk_button_new_with_label("Delete");
    gtk_widget_set_tooltip_text(deleteBtn_, "Delete the selected preset");
    g_signal_connect_swapped(deleteBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) { if (self->deleteCb_) self->deleteCb_(); }),
        this);
    gtk_box_append(GTK_BOX(bar_), deleteBtn_);

    if (parent_box)
        gtk_box_append(GTK_BOX(parent_box), bar_);
}

// ── Internal handlers ─────────────────────────────────────────────────────────

void PresetBar::onDropdownChanged() {
    const int idx = getSelectedIndex();
    if (idx < 0) return;
    auto* obj = GTK_STRING_OBJECT(
        g_list_model_get_item(G_LIST_MODEL(stringList_), static_cast<guint>(idx)));
    if (obj) {
        setCurrentName(gtk_string_object_get_string(obj));
        g_object_unref(obj);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void PresetBar::setPresetNames(const std::vector<std::string>& names) {
    const guint n = g_list_model_get_n_items(G_LIST_MODEL(stringList_));
    if (n > 0)
        gtk_string_list_splice(stringList_, 0, n, nullptr);
    for (const auto& name : names)
        gtk_string_list_append(stringList_, name.c_str());
}

void PresetBar::setCurrentName(const std::string& name) {
    gtk_editable_set_text(GTK_EDITABLE(nameEntry_), name.c_str());
}

std::string PresetBar::getCurrentName() const {
    const char* txt = gtk_editable_get_text(GTK_EDITABLE(nameEntry_));
    return txt ? txt : "";
}

int PresetBar::getSelectedIndex() const {
    const guint sel = gtk_drop_down_get_selected(GTK_DROP_DOWN(presetDrop_));
    return (sel == GTK_INVALID_LIST_POSITION) ? -1 : static_cast<int>(sel);
}
