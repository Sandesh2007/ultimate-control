/**
 * @file PowerTab.cpp
 * @brief Implementation of the power management tab
 *
 * This file implements the PowerTab class which provides a user interface
 * for system power operations like shutdown, reboot, suspend, and hibernate,
 * as well as power profile management.
 */

#include "PowerTab.hpp"
#include <iostream>

namespace Power
{

    /**
     * @brief Constructor for the power tab
     *
     * Initializes the power manager and creates the UI components.
     */
    PowerTab::PowerTab()
        : manager_(std::make_shared<PowerManager>()), // Initialize power manager
          main_box_(Gtk::ORIENTATION_VERTICAL, 15)    // Main container with 15px spacing
    {
        // Set up the main container orientation
        set_orientation(Gtk::ORIENTATION_VERTICAL);

        // Create and setup accelerator group for shortcuts
        accel_group_ = Gtk::AccelGroup::create();

        // We'll add the accel group to the parent toplevel window when realized (widget shown)
        signal_realize().connect([this]()
                                 {
        Gtk::Window* parent = dynamic_cast<Gtk::Window*>(get_toplevel());
        if (parent) {
            parent->add_accel_group(accel_group_);

            // Add a key press event handler to the parent window
            parent->add_events(Gdk::KEY_PRESS_MASK);
            parent->signal_key_press_event().connect(
                [this](GdkEventKey* event) -> bool {
                    // Only handle keybinds when the Power tab is active
                    Gtk::Notebook* notebook = dynamic_cast<Gtk::Notebook*>(get_parent());
                    if (!notebook || notebook->get_nth_page(notebook->get_current_page()) != this) {
                        return false; // Not on the Power tab, let other handlers process the event
                    }

                    // Handle single-key shortcuts (without modifiers)
                    if (event->state == 0) { // No modifiers
                        auto settings = manager_->get_settings();

                        // Check each action's keybind
                        if (gdk_keyval_to_unicode(event->keyval) == 's' ||
                            gdk_keyval_to_unicode(event->keyval) == 'S') {
                            if (settings->get_keybind("shutdown") == "S") {
                                manager_->shutdown();
                                return true;
                            }
                        }
                        else if (gdk_keyval_to_unicode(event->keyval) == 'r' ||
                                 gdk_keyval_to_unicode(event->keyval) == 'R') {
                            if (settings->get_keybind("reboot") == "R") {
                                manager_->reboot();
                                return true;
                            }
                        }
                        else if (gdk_keyval_to_unicode(event->keyval) == 'u' ||
                                 gdk_keyval_to_unicode(event->keyval) == 'U') {
                            if (settings->get_keybind("suspend") == "U") {
                                manager_->suspend();
                                return true;
                            }
                        }
                        else if (gdk_keyval_to_unicode(event->keyval) == 'h' ||
                                 gdk_keyval_to_unicode(event->keyval) == 'H') {
                            if (settings->get_keybind("hibernate") == "H") {
                                manager_->hibernate();
                                return true;
                            }
                        }
                        else if (gdk_keyval_to_unicode(event->keyval) == 'l' ||
                                 gdk_keyval_to_unicode(event->keyval) == 'L') {
                            if (settings->get_keybind("lock") == "L") {
                                std::system(manager_->get_settings()->get_command("lock").c_str());
                                return true;
                            }
                        }
                    }
                    return false; // Let other keys pass through
                }, false);
        } });

        // We'll set up the keybinds after the widget is realized
        signal_realize().connect([this]()
                                 {
            // Set up keybinds after the widget is realized
            setup_action_keybinds(); });

        // Add a scrolled window to contain all sections
        scrolled_window_.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        pack_start(scrolled_window_, Gtk::PACK_EXPAND_WIDGET);

        // Set up the main box inside the scrolled window with margins
        main_box_.set_margin_start(20);
        main_box_.set_margin_end(20);
        main_box_.set_margin_top(20);
        main_box_.set_margin_bottom(20);
        scrolled_window_.add(main_box_);

        // Create the three main sections of the power tab
        create_system_section();
        create_session_section();
        create_power_profiles_section();

        // Add all section frames to the main box
        main_box_.pack_start(system_frame_, Gtk::PACK_SHRINK);
        main_box_.pack_start(session_frame_, Gtk::PACK_SHRINK);
        main_box_.pack_start(profiles_frame_, Gtk::PACK_SHRINK);

        show_all_children();
        std::cout << "Power tab loaded!" << std::endl;
    }

    /**
     * @brief Destructor for the power tab
     */
    PowerTab::~PowerTab() = default;

    /**
     * @brief Create the system power section
     *
     * Creates the UI components for system power operations
     * (shutdown and reboot).
     */
    void PowerTab::create_system_section()
    {
        // Configure the frame and container for the system power section
        system_frame_.set_shadow_type(Gtk::SHADOW_ETCHED_IN);
        system_box_.set_orientation(Gtk::ORIENTATION_VERTICAL);
        system_box_.set_spacing(10);
        system_box_.set_margin_start(15);
        system_box_.set_margin_end(15);
        system_box_.set_margin_top(15);
        system_box_.set_margin_bottom(15);

        // Configure the header for the system power section
        system_header_box_.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        system_header_box_.set_spacing(10);

        system_icon_.set_from_icon_name("system-shutdown-symbolic", Gtk::ICON_SIZE_DIALOG);
        system_label_.set_markup("<span size='large' weight='bold'>System Power</span>");
        system_label_.set_halign(Gtk::ALIGN_START);
        system_label_.set_valign(Gtk::ALIGN_CENTER);

        system_header_box_.pack_start(system_icon_, Gtk::PACK_SHRINK);
        system_header_box_.pack_start(system_label_, Gtk::PACK_EXPAND_WIDGET);

        // Add settings button to the header for configuring power commands
        add_settings_button_to_header(system_header_box_);

        // Configure the container for system power buttons
        system_buttons_box_.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        system_buttons_box_.set_spacing(15);
        system_buttons_box_.set_homogeneous(true);

        // Configure the shutdown button with icon and click handler
        {
            auto settings = manager_->get_settings();
            std::string shutdown_label = "Shutdown";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("shutdown");
                if (!key.empty())
                    shutdown_label += " [" + key + "]";
            }
            shutdown_button_.set_label(shutdown_label);
        }
        shutdown_button_.set_image_from_icon_name("system-shutdown-symbolic", Gtk::ICON_SIZE_BUTTON);
        shutdown_button_.set_always_show_image(true);
        shutdown_button_.set_tooltip_text("Power off the system");
        shutdown_button_.signal_clicked().connect([this]()
                                                  { manager_->shutdown(); });
        // Prevent tab navigation
        shutdown_button_.property_can_default() = false;
        shutdown_button_.property_can_focus() = false;
        // Accelerator will be set up dynamically

        // Configure the reboot button with icon and click handler
        {
            auto settings = manager_->get_settings();
            std::string reboot_label = "Reboot";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("reboot");
                if (!key.empty())
                    reboot_label += " [" + key + "]";
            }
            reboot_button_.set_label(reboot_label);
        }
        reboot_button_.set_image_from_icon_name("system-reboot-symbolic", Gtk::ICON_SIZE_BUTTON);
        reboot_button_.set_always_show_image(true);
        reboot_button_.set_tooltip_text("Restart the system");
        reboot_button_.signal_clicked().connect([this]()
                                                { manager_->reboot(); });
        // Prevent tab navigation
        reboot_button_.property_can_default() = false;
        reboot_button_.property_can_focus() = false;
        // Accelerator will be set up dynamically

        // Add both buttons to the buttons container
        system_buttons_box_.pack_start(shutdown_button_, Gtk::PACK_EXPAND_WIDGET);
        system_buttons_box_.pack_start(reboot_button_, Gtk::PACK_EXPAND_WIDGET);

        // Assemble the system section components
        system_box_.pack_start(system_header_box_, Gtk::PACK_SHRINK);
        system_box_.pack_start(system_buttons_box_, Gtk::PACK_SHRINK);

        // Add the assembled system box to the frame
        system_frame_.add(system_box_);
    }

    /**
     * @brief Create the session actions section
     *
     * Creates the UI components for session actions
     * (suspend, hibernate, and lock screen).
     */
    void PowerTab::create_session_section()
    {
        // Configure the frame and container for the session section
        session_frame_.set_shadow_type(Gtk::SHADOW_ETCHED_IN);
        session_box_.set_orientation(Gtk::ORIENTATION_VERTICAL);
        session_box_.set_spacing(10);
        session_box_.set_margin_start(15);
        session_box_.set_margin_end(15);
        session_box_.set_margin_top(15);
        session_box_.set_margin_bottom(15);

        // Configure the header for the session section
        session_header_box_.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        session_header_box_.set_spacing(10);

        session_icon_.set_from_icon_name("system-lock-screen-symbolic", Gtk::ICON_SIZE_DIALOG);
        session_label_.set_markup("<span size='large' weight='bold'>Session Actions</span>");
        session_label_.set_halign(Gtk::ALIGN_START);
        session_label_.set_valign(Gtk::ALIGN_CENTER);

        session_header_box_.pack_start(session_icon_, Gtk::PACK_SHRINK);
        session_header_box_.pack_start(session_label_, Gtk::PACK_EXPAND_WIDGET);

        // Configure the container for session action buttons
        session_buttons_box_.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        session_buttons_box_.set_spacing(15);
        session_buttons_box_.set_homogeneous(true);

        // Configure the suspend button with icon and click handler
        {
            auto settings = manager_->get_settings();
            std::string suspend_label = "Suspend";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("suspend");
                if (!key.empty())
                    suspend_label += " [" + key + "]";
            }
            suspend_button_.set_label(suspend_label);
        }
        suspend_button_.set_image_from_icon_name("system-suspend-symbolic", Gtk::ICON_SIZE_BUTTON);
        suspend_button_.set_always_show_image(true);
        suspend_button_.set_tooltip_text("Put the system to sleep");
        suspend_button_.signal_clicked().connect([this]()
                                                 { manager_->suspend(); });
        // Prevent tab navigation
        suspend_button_.property_can_default() = false;
        suspend_button_.property_can_focus() = false;
        // Accelerator will be set up dynamically

        // Configure the hibernate button with icon and click handler
        {
            auto settings = manager_->get_settings();
            std::string hibernate_label = "Hibernate";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("hibernate");
                if (!key.empty())
                    hibernate_label += " [" + key + "]";
            }
            hibernate_button_.set_label(hibernate_label);
        }
        hibernate_button_.set_image_from_icon_name("system-hibernate-symbolic", Gtk::ICON_SIZE_BUTTON);
        hibernate_button_.set_always_show_image(true);
        hibernate_button_.set_tooltip_text("Hibernate the system");
        hibernate_button_.signal_clicked().connect([this]()
                                                   { manager_->hibernate(); });
        // Prevent tab navigation
        hibernate_button_.property_can_default() = false;
        hibernate_button_.property_can_focus() = false;
        // Accelerator will be set up dynamically

        // Configure the lock screen button with icon and click handler
        {
            auto settings = manager_->get_settings();
            std::string lock_label = "Lock";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("lock");
                if (!key.empty())
                    lock_label += " [" + key + "]";
            }
            lock_button_.set_label(lock_label);
        }
        lock_button_.set_image_from_icon_name("system-lock-screen-symbolic", Gtk::ICON_SIZE_BUTTON);
        lock_button_.set_always_show_image(true);
        lock_button_.set_tooltip_text("Lock the screen");
        lock_button_.signal_clicked().connect([this]()
                                              { std::system(manager_->get_settings()->get_command("lock").c_str()); });
        // Prevent tab navigation
        lock_button_.property_can_default() = false;
        lock_button_.property_can_focus() = false;
        // Accelerator will be set up dynamically

        // Add all three buttons to the buttons container
        session_buttons_box_.pack_start(suspend_button_, Gtk::PACK_EXPAND_WIDGET);
        session_buttons_box_.pack_start(hibernate_button_, Gtk::PACK_EXPAND_WIDGET);
        session_buttons_box_.pack_start(lock_button_, Gtk::PACK_EXPAND_WIDGET);

        // Assemble the session section components
        session_box_.pack_start(session_header_box_, Gtk::PACK_SHRINK);
        session_box_.pack_start(session_buttons_box_, Gtk::PACK_SHRINK);

        // Add the assembled session box to the frame
        session_frame_.add(session_box_);
    }

    /**
     * @brief Create the power profiles section
     *
     * Creates the UI components for power profile management.
     */
    void PowerTab::create_power_profiles_section()
    {
        // Configure the frame and container for the power profiles section
        profiles_frame_.set_shadow_type(Gtk::SHADOW_ETCHED_IN);
        profiles_box_.set_orientation(Gtk::ORIENTATION_VERTICAL);
        profiles_box_.set_spacing(10);
        profiles_box_.set_margin_start(15);
        profiles_box_.set_margin_end(15);
        profiles_box_.set_margin_top(15);
        profiles_box_.set_margin_bottom(15);

        // Configure the header for the power profiles section
        profiles_header_box_.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
        profiles_header_box_.set_spacing(10);

        profiles_icon_.set_from_icon_name("power-profile-balanced-symbolic", Gtk::ICON_SIZE_DIALOG);
        profiles_label_.set_markup("<span size='large' weight='bold'>Power Profiles</span>");
        profiles_label_.set_halign(Gtk::ALIGN_START);
        profiles_label_.set_valign(Gtk::ALIGN_CENTER);

        profiles_header_box_.pack_start(profiles_icon_, Gtk::PACK_SHRINK);
        profiles_header_box_.pack_start(profiles_label_, Gtk::PACK_EXPAND_WIDGET);

        // Configure the container for power profiles content
        profiles_content_box_.set_orientation(Gtk::ORIENTATION_VERTICAL);
        profiles_content_box_.set_spacing(10);

        // Add descriptive text explaining power profiles
        Gtk::Label *description = Gtk::manage(new Gtk::Label());
        description->set_markup("Select a power profile to optimize battery life and performance:");
        description->set_halign(Gtk::ALIGN_START);
        profiles_content_box_.pack_start(*description, Gtk::PACK_SHRINK);

        // Configure the dropdown for selecting power profiles
        profile_combo_.set_hexpand(true);
        profile_combo_.set_can_focus(false); // Prevent tab navigation to this dropdown

        // Populate the dropdown with available power profiles
        auto profiles = manager_->list_power_profiles();
        profile_combo_.remove_all();
        for (const auto &profile : profiles)
        {
            profile_combo_.append(profile);
        }

        // Set the currently active profile in the dropdown
        if (!profiles.empty())
        {
            profile_combo_.set_sensitive(true);

            auto current = manager_->get_current_power_profile();
            bool found = false;
            for (const auto &profile : profiles)
            {
                if (profile == current)
                {
                    profile_combo_.set_active_text(profile);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                profile_combo_.set_active(0);
            }
        }
        else
        {
            profile_combo_.set_sensitive(false);
        }

        // Connect the change signal to update the power profile
        profile_combo_.signal_changed().connect([this]()
                                                {
        auto selected = profile_combo_.get_active_text();
        if (!selected.empty()) {
            manager_->set_power_profile(selected);
        } });

        // Add the dropdown to the content container
        profiles_content_box_.pack_start(profile_combo_, Gtk::PACK_SHRINK);

        // Assemble the power profiles section components
        profiles_box_.pack_start(profiles_header_box_, Gtk::PACK_SHRINK);
        profiles_box_.pack_start(profiles_content_box_, Gtk::PACK_SHRINK);

        // Add the assembled profiles box to the frame
        profiles_frame_.add(profiles_box_);
    }

    /**
     * @brief Add a settings button to a section header
     * @param header_box The header box to add the button to
     *
     * Adds a settings button with a cog icon to a section header.
     * When clicked, the button opens the power settings dialog.
     */
    void PowerTab::add_settings_button_to_header(Gtk::Box &header_box)
    {
        // Create a button with no relief (flat appearance)
        auto settings_button = Gtk::make_managed<Gtk::Button>();
        settings_button->set_relief(Gtk::RELIEF_NONE);
        settings_button->set_tooltip_text("Configure power commands");

        // Add a settings/cog icon to the button
        auto settings_icon = Gtk::make_managed<Gtk::Image>();
        settings_icon->set_from_icon_name("emblem-system-symbolic", Gtk::ICON_SIZE_BUTTON);
        settings_button->set_image(*settings_icon);
        // Prevent tab navigation
        settings_button->property_can_default() = false;
        settings_button->property_can_focus() = false;

        // Connect the button click to open settings dialog
        settings_button->signal_clicked().connect(sigc::mem_fun(*this, &PowerTab::on_settings_clicked));

        // Add the button to the right side of the header
        header_box.pack_end(*settings_button, Gtk::PACK_SHRINK);
    }

    /**
     * @brief Handler for settings button clicks
     *
     * Opens the power settings dialog to configure power commands.
     */
    void PowerTab::on_settings_clicked()
    {
        // Get the parent window for the dialog
        Gtk::Window *parent = dynamic_cast<Gtk::Window *>(get_toplevel());
        if (!parent)
            return;

        // Create and show the power settings dialog
        PowerSettingsDialog dialog(*parent, manager_->get_settings());
        int result = dialog.run();

        if (result == Gtk::RESPONSE_OK)
        {
            // Save the settings if OK was clicked
            dialog.save_settings();
            // Re-setup keybinds after settings are changed
            setup_action_keybinds();
            // Update button labels to reflect new settings (e.g., keybind hints)
            update_button_labels();
        }
    }

    // Helper to parse keybind string (e.g., "Ctrl+Alt+S") into keyval and modifier
    bool PowerTab::parse_keybind(const std::string &keybind, guint &keyval, Gdk::ModifierType &modifier)
    {
        keyval = 0;
        modifier = Gdk::ModifierType(0);
        if (keybind.empty())
            return false;

        std::string str = keybind;
        // Split by '+'
        size_t pos = 0;
        std::string token;
        std::vector<std::string> parts;
        while ((pos = str.find('+')) != std::string::npos)
        {
            parts.push_back(str.substr(0, pos));
            str.erase(0, pos + 1);
        }
        parts.push_back(str);

        // The last part is the key, the rest are modifiers
        std::string key_part = parts.back();
        parts.pop_back();

        // Map modifier strings to Gdk::ModifierType
        for (const auto &mod : parts)
        {
            if (mod == "Ctrl" || mod == "Control")
                modifier = static_cast<Gdk::ModifierType>(modifier | Gdk::CONTROL_MASK);
            else if (mod == "Alt")
                modifier = static_cast<Gdk::ModifierType>(modifier | GDK_MOD1_MASK);
            else if (mod == "Shift")
                modifier = static_cast<Gdk::ModifierType>(modifier | GDK_SHIFT_MASK);
            else if (mod == "Super" || mod == "Meta" || mod == "Win")
                modifier = static_cast<Gdk::ModifierType>(modifier | GDK_SUPER_MASK);
            // Add more if needed
        }

        // Convert key_part to keyval
        if (key_part.length() == 1)
        {
            keyval = gdk_unicode_to_keyval(key_part[0]);
        }
        else
        {
            // Try to map named keys (e.g., "F1", "Delete")
            keyval = gdk_keyval_from_name(key_part.c_str());
            if (keyval == 0)
                return false;
        }
        return true;
    }

    // Set up accelerators for all power actions based on user keybinds
    void PowerTab::setup_action_keybinds()
    {
        // Set up accelerators for all power actions based on user keybinds

        auto settings = manager_->get_settings();
        struct
        {
            Gtk::Button *button;
            std::string action;
        } actions[] = {
            {&shutdown_button_, "shutdown"},
            {&reboot_button_, "reboot"},
            {&suspend_button_, "suspend"},
            {&hibernate_button_, "hibernate"},
            {&lock_button_, "lock"},
        };

        for (const auto &act : actions)
        {
            std::string keybind = settings->get_keybind(act.action);
            if (!keybind.empty())
            {
                guint keyval;
                Gdk::ModifierType mod;
                if (parse_keybind(keybind, keyval, mod))
                {
                    act.button->add_accelerator("clicked", accel_group_, keyval, mod, Gtk::ACCEL_VISIBLE);
                }
            }
        }
    }

    // Update all power button labels according to current settings
    void PowerTab::update_button_labels()
    {
        auto settings = manager_->get_settings();

        // System buttons
        {
            std::string shutdown_label = "Shutdown";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("shutdown");
                if (!key.empty())
                    shutdown_label += " [" + key + "]";
            }
            shutdown_button_.set_label(shutdown_label);
        }
        {
            std::string reboot_label = "Reboot";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("reboot");
                if (!key.empty())
                    reboot_label += " [" + key + "]";
            }
            reboot_button_.set_label(reboot_label);
        }

        // Session buttons
        {
            std::string suspend_label = "Suspend";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("suspend");
                if (!key.empty())
                    suspend_label += " [" + key + "]";
            }
            suspend_button_.set_label(suspend_label);
        }
        {
            std::string hibernate_label = "Hibernate";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("hibernate");
                if (!key.empty())
                    hibernate_label += " [" + key + "]";
            }
            hibernate_button_.set_label(hibernate_label);
        }
        {
            std::string lock_label = "Lock";
            if (settings->get_show_keybind_hints())
            {
                std::string key = settings->get_keybind("lock");
                if (!key.empty())
                    lock_label += " [" + key + "]";
            }
            lock_button_.set_label(lock_label);
        }
    }

} // namespace Power
