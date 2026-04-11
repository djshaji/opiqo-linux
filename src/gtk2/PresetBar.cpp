// src/gtk2/PresetBar.cpp (GTK2 port)

#include "PresetBar.h"

// ── PresetBar ─────────────────────────────────────────────────────────────────

PresetBar::PresetBar(GtkWidget* parent_box) {
    buildWidgets(parent_box);
}

void PresetBar::buildWidgets(GtkWidget* parent_box) {
    // GTK2: gtk_hbox_new instead of gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6)
    bar_ = gtk_hbox_new(FALSE, 6);

    // ── "Preset:" label ───────────────────────────────────────────────────
    GtkWidget* label = gtk_label_new("Preset:");
    gtk_box_pack_start(GTK_BOX(bar_), label, FALSE, FALSE, 0);

    // ── Preset combo box with editable entry (GTK 2.24+) ─────────────────
    presetDrop_ = gtk_combo_box_text_new_with_entry();
    gtk_widget_set_size_request(presetDrop_, 200, -1);
    gtk_widget_set_tooltip_text(presetDrop_, "Select a saved preset");
    g_signal_connect_swapped(presetDrop_, "changed",
        G_CALLBACK(+[](PresetBar* self) { self->onDropdownChanged(); }), this);
    gtk_box_pack_start(GTK_BOX(bar_), presetDrop_, FALSE, FALSE, 0);

    // GTK2: gtk_vseparator_new instead of gtk_separator_new(VERTICAL)
    gtk_box_pack_start(GTK_BOX(bar_), gtk_vseparator_new(), FALSE, FALSE, 0);

    // ── Name entry ────────────────────────────────────────────────────────
    GtkWidget* nameLabel = gtk_label_new("Name:");
    gtk_box_pack_start(GTK_BOX(bar_), nameLabel, FALSE, FALSE, 0);

    nameEntry_ = gtk_entry_new();
    // Note: gtk_entry_set_placeholder_text is not available in GTK2; we use the label instead
    gtk_widget_set_size_request(nameEntry_, 160, -1);
    gtk_widget_set_tooltip_text(nameEntry_, "Name for saving the current state");
    gtk_box_pack_start(GTK_BOX(bar_), nameEntry_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(bar_), gtk_vseparator_new(), FALSE, FALSE, 0);

    // ── Load button ───────────────────────────────────────────────────────
    loadBtn_ = gtk_button_new_with_label("Load");
    gtk_widget_set_tooltip_text(loadBtn_, "Apply the selected preset to all slots");
    g_signal_connect_swapped(loadBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) { if (self->loadCb_) self->loadCb_(); }), this);
    gtk_box_pack_start(GTK_BOX(bar_), loadBtn_, FALSE, FALSE, 0);

    // ── Save button ───────────────────────────────────────────────────────
    saveBtn_ = gtk_button_new_with_label("Save");
    gtk_widget_set_tooltip_text(saveBtn_, "Save current plugin state as a named preset");
    g_signal_connect_swapped(saveBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) {
            if (self->saveCb_) self->saveCb_(self->getCurrentName());
        }), this);
    gtk_box_pack_start(GTK_BOX(bar_), saveBtn_, FALSE, FALSE, 0);

    // ── Delete button ─────────────────────────────────────────────────────
    deleteBtn_ = gtk_button_new_with_label("Delete");
    gtk_widget_set_tooltip_text(deleteBtn_, "Delete the selected preset");
    g_signal_connect_swapped(deleteBtn_, "clicked",
        G_CALLBACK(+[](PresetBar* self) { if (self->deleteCb_) self->deleteCb_(); }), this);
    gtk_box_pack_start(GTK_BOX(bar_), deleteBtn_, FALSE, FALSE, 0);

    if (parent_box)
        gtk_box_pack_start(GTK_BOX(parent_box), bar_, FALSE, FALSE, 0);
}

// ── Internal handlers ─────────────────────────────────────────────────────────

void PresetBar::onDropdownChanged() {
    const gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(presetDrop_));
    if (active >= 0) {
        gchar* txt = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(presetDrop_));
        if (txt) {
            setCurrentName(txt);
            g_free(txt);
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void PresetBar::setPresetNames(const std::vector<std::string>& names) {
    // GTK2: gtk_combo_box_text_remove_all not available; clear the backing model directly
    GtkListStore* store = GTK_LIST_STORE(
        gtk_combo_box_get_model(GTK_COMBO_BOX(presetDrop_)));
    gtk_list_store_clear(store);
    for (const auto& name : names)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presetDrop_), name.c_str());
}

void PresetBar::setCurrentName(const std::string& name) {
    gtk_entry_set_text(GTK_ENTRY(nameEntry_), name.c_str());
}

std::string PresetBar::getCurrentName() const {
    return gtk_entry_get_text(GTK_ENTRY(nameEntry_));
}

int PresetBar::getSelectedIndex() const {
    return static_cast<int>(gtk_combo_box_get_active(GTK_COMBO_BOX(presetDrop_)));
}
