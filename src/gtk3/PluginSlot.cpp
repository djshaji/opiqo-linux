// src/gtk3/PluginSlot.cpp (GTK3 port)

#include "PluginSlot.h"

#include <cstdio>

// ── PluginSlot ────────────────────────────────────────────────────────────────

PluginSlot::PluginSlot(int slot, GtkWidget* parent_window) : slot_(slot) {
    paramPanel_ = new ParameterPanel(parent_window);
    buildWidgets();
}

void PluginSlot::buildWidgets() {
    char label[32];
    std::snprintf(label, sizeof(label), "Slot %d", slot_);

    frame_ = gtk_frame_new(nullptr);
    gtk_widget_set_hexpand(frame_, TRUE);
    gtk_widget_set_vexpand(frame_, TRUE);
    // GTK3: gtk_style_context_add_class instead of gtk_widget_add_css_class
    gtk_style_context_add_class(
        gtk_widget_get_style_context(frame_), "plugin-slot");

    GtkWidget* outerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // GTK3: gtk_container_add instead of gtk_frame_set_child
    gtk_container_add(GTK_CONTAINER(frame_), outerBox);

    // ── Header row ────────────────────────────────────────────────────────
    headerBox_ = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start(headerBox_, 6);
    gtk_widget_set_margin_end  (headerBox_, 6);
    gtk_widget_set_margin_top  (headerBox_, 4);
    gtk_widget_set_margin_bottom(headerBox_, 4);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(headerBox_), "slot-header");

    // Plugin name label
    nameLabel_ = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(nameLabel_), 0.0f);
    gtk_widget_set_hexpand(nameLabel_, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(nameLabel_), "slot-name");
    gtk_box_pack_start(GTK_BOX(headerBox_), nameLabel_, TRUE, TRUE, 0);

    // Add (+) button
    addButton_ = gtk_button_new_with_label("+ Add");
    gtk_widget_set_tooltip_text(addButton_, "Load plugin into this slot");
    g_signal_connect_swapped(addButton_, "clicked",
        G_CALLBACK(+[](PluginSlot* self) {
            if (self->addCb_) self->addCb_(self->slot_);
        }),
        this);
    gtk_box_pack_start(GTK_BOX(headerBox_), addButton_, FALSE, FALSE, 0);

    // Bypass toggle
    bypassButton_ = gtk_toggle_button_new_with_label("Bypass");
    gtk_widget_set_tooltip_text(bypassButton_, "Bypass this plugin");
    gtk_widget_set_sensitive(bypassButton_, FALSE);
    g_signal_connect_swapped(bypassButton_, "toggled",
        G_CALLBACK(+[](PluginSlot* self) {
            const bool bypassed =
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->bypassButton_));
            if (self->bypassCb_) self->bypassCb_(self->slot_, bypassed);
        }),
        this);
    gtk_box_pack_start(GTK_BOX(headerBox_), bypassButton_, FALSE, FALSE, 0);

    // Delete (×) button
    deleteButton_ = gtk_button_new_with_label("× Remove");
    gtk_widget_set_tooltip_text(deleteButton_, "Remove plugin from this slot");
    gtk_widget_set_sensitive(deleteButton_, FALSE);
    // GTK3: gtk_style_context_add_class
    gtk_style_context_add_class(
        gtk_widget_get_style_context(deleteButton_), "destructive-action");
    g_signal_connect_swapped(deleteButton_, "clicked",
        G_CALLBACK(+[](PluginSlot* self) {
            if (self->deleteCb_) self->deleteCb_(self->slot_);
        }),
        this);
    gtk_box_pack_start(GTK_BOX(headerBox_), deleteButton_, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outerBox), headerBox_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outerBox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    // ── Parameter panel ───────────────────────────────────────────────────
    paramPanel_->setValueCallback([this](uint32_t portIndex, float value) {
        if (valueCb_) valueCb_(slot_, portIndex, value);
    });
    paramPanel_->setFileCallback([this](const std::string& uri,
                                         const std::string& path) {
        if (fileCb_) fileCb_(slot_, uri, path);
    });

    gtk_box_pack_start(GTK_BOX(outerBox), paramPanel_->widget(), TRUE, TRUE, 0);
}

void PluginSlot::onPluginAdded(const std::string& pluginName,
                               const std::vector<LV2Plugin::PortInfo>& ports) {
    gtk_label_set_text(GTK_LABEL(nameLabel_), pluginName.c_str());
    gtk_widget_set_sensitive(bypassButton_, TRUE);
    gtk_widget_set_sensitive(deleteButton_, TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bypassButton_), FALSE);
    paramPanel_->build(ports);
}

void PluginSlot::onPluginCleared() {
    char label[32];
    std::snprintf(label, sizeof(label), "Slot %d", slot_);
    gtk_label_set_text(GTK_LABEL(nameLabel_), label);
    gtk_widget_set_sensitive(bypassButton_, FALSE);
    gtk_widget_set_sensitive(deleteButton_, FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bypassButton_), FALSE);
    paramPanel_->clear();
}
