/**
 * @file WifiTab.cpp
 * @brief Implementation of the WiFi tab for the Ultimate Control application
 *
 * This file implements the WifiTab class which provides a user interface
 * for scanning, viewing, and connecting to WiFi networks.
 */

#include "WifiTab.hpp"
#include <iostream>

namespace Wifi
{

    /**
     * @brief Constructor for the WiFi tab
     *
     * Initializes the WiFi manager and creates the UI components.
     * Does not perform an initial network scan to improve loading time.
     */
    WifiTab::WifiTab()
        : manager_(std::make_shared<WifiManager>()), // Create WiFi manager
          container_(Gtk::ORIENTATION_VERTICAL, 10)  // Vertical container for network widgets
    {
        // Set scrolling policy for the main window
        set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);

        // Create a main vertical box with padding to hold all components
        Gtk::Box *main_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10));
        main_box->set_margin_start(10);
        main_box->set_margin_end(10);
        main_box->set_margin_top(10);
        main_box->set_margin_bottom(10);
        add(*main_box);

        // Create a horizontal header box for title and controls
        Gtk::Box *header_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));

        // Add WiFi status icon on the left
        wifi_status_icon_.set_from_icon_name("network-wireless-symbolic", Gtk::ICON_SIZE_DIALOG);
        header_box->pack_start(wifi_status_icon_, Gtk::PACK_SHRINK);

        // Add title label with large bold text
        Gtk::Label *title = Gtk::manage(new Gtk::Label());
        title->set_markup("<span size='large' weight='bold'>Available Networks</span>");
        title->set_halign(Gtk::ALIGN_START);
        title->set_valign(Gtk::ALIGN_CENTER);
        header_box->pack_start(*title, Gtk::PACK_EXPAND_WIDGET);

        // Create a vertical box for controls (toggle and scan button) on the right side
        Gtk::Box *controls_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));

        // Create horizontal box for WiFi toggle switch and labels
        Gtk::Box *toggle_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));

        // Set up the WiFi toggle switch with labels and initial state
        Gtk::Label *toggle_label = Gtk::manage(new Gtk::Label("WiFi:"));
        wifi_status_label_.set_text("Enabled");
        wifi_switch_.set_active(manager_->is_wifi_enabled());
        wifi_switch_.set_tooltip_text("Enable/Disable WiFi");
        wifi_switch_.set_can_focus(false); // Prevent tab navigation to this switch

        toggle_box->pack_start(*toggle_label, Gtk::PACK_SHRINK);
        toggle_box->pack_start(wifi_switch_, Gtk::PACK_SHRINK);
        toggle_box->pack_start(wifi_status_label_, Gtk::PACK_SHRINK);

        // Create scan button with refresh icon
        scan_button_.set_image_from_icon_name("view-refresh-symbolic", Gtk::ICON_SIZE_BUTTON);
        scan_button_.set_label("Scan");
        scan_button_.set_always_show_image(true);
        scan_button_.set_sensitive(manager_->is_wifi_enabled());
        scan_button_.set_can_focus(false); // Prevent tab navigation to this button

        // Add toggle box and scan button to the controls container
        controls_box->pack_start(*toggle_box, Gtk::PACK_SHRINK);
        controls_box->pack_start(scan_button_, Gtk::PACK_SHRINK);

        // Add controls box to the right side of the header
        header_box->pack_end(*controls_box, Gtk::PACK_SHRINK);

        // Add a horizontal separator below the header
        Gtk::Separator *separator = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));

        // Create ethernet status box but don't add it to the UI yet
        // It will only be added if Ethernet is actually connected
        ethernet_box_ = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
        ethernet_box_->set_margin_bottom(10);

        // Set up ethernet icon
        ethernet_status_icon_.set_from_icon_name("network-wired-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
        ethernet_box_->pack_start(ethernet_status_icon_, Gtk::PACK_SHRINK);

        // Set up ethernet status label
        ethernet_status_label_.set_text("You are connected to ethernet");
        ethernet_status_label_.set_halign(Gtk::ALIGN_START);
        ethernet_box_->pack_start(ethernet_status_label_, Gtk::PACK_SHRINK);

        // Add header and separator to the top of the main box
        main_box->pack_start(*header_box, Gtk::PACK_SHRINK);
        main_box->pack_start(*separator, Gtk::PACK_SHRINK);

        // Store main_box for later use when adding ethernet status
        main_box_ = main_box;

        // Create a scrolled window to contain the network list
        Gtk::ScrolledWindow *networks_scroll = Gtk::manage(new Gtk::ScrolledWindow());
        networks_scroll->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
        networks_scroll->add(container_);
        main_box->pack_start(*networks_scroll, Gtk::PACK_EXPAND_WIDGET);

        // Set up scan button click handler
        scan_button_.signal_clicked().connect([this]()
                                              {
        scan_button_.set_sensitive(false);
        scan_button_.set_label("Scanning...");

        // Use asynchronous scanning to prevent UI freezing
        manager_->scan_networks_async();

        // Re-enable the scan button after a short delay (2 seconds)
        Glib::signal_timeout().connect_once([this]() {
            scan_button_.set_sensitive(true);
            scan_button_.set_label("Scan");
            // Update ethernet status when scan completes
            update_ethernet_status();
        }, 2000); });

        // Connect WiFi switch toggle handler
        wifi_switch_.property_active().signal_changed().connect(sigc::mem_fun(*this, &WifiTab::on_wifi_switch_toggled));

        // Register callback for network list updates from the WiFi manager
        manager_->set_update_callback([this](const std::vector<Network> &networks)
                                      { update_network_list(networks); });

        // Register callback for WiFi state changes from the WiFi manager
        manager_->set_state_callback([this](bool enabled)
                                     { update_wifi_state(enabled); });

        // Initialize UI based on current WiFi state
        update_wifi_state(manager_->is_wifi_enabled());

        // Initialize ethernet status
        update_ethernet_status();

        // Show a loading message initially instead of scanning immediately
        loading_label_ = Gtk::manage(new Gtk::Label("Loading networks..."));
        loading_label_->set_margin_top(20);
        loading_label_->set_margin_bottom(20);
        container_.pack_start(*loading_label_, Gtk::PACK_SHRINK);

        show_all_children();
        std::cout << "WiFi tab loaded!" << std::endl;

        // Schedule a delayed scan after the tab is visible
        Glib::signal_timeout().connect_once(
            sigc::mem_fun(*this, &WifiTab::perform_delayed_scan), 100);
    }

    /**
     * @brief Destructor for the WiFi tab
     */
    WifiTab::~WifiTab() = default;

    /**
     * @brief Update the UI based on WiFi state
     * @param enabled Whether WiFi is enabled
     *
     * Updates the WiFi icon, status label, and scan button sensitivity
     * based on the current WiFi state.
     */
    void WifiTab::update_wifi_state(bool enabled)
    {
        // Update switch, label, and button state
        wifi_switch_.set_active(enabled);
        wifi_status_label_.set_text(enabled ? "Enabled" : "Disabled");
        scan_button_.set_sensitive(enabled);

        // Set appropriate WiFi icon based on enabled state
        if (enabled)
        {
            wifi_status_icon_.set_from_icon_name("network-wireless-symbolic", Gtk::ICON_SIZE_DIALOG);
        }
        else
        {
            wifi_status_icon_.set_from_icon_name("network-wireless-disabled-symbolic", Gtk::ICON_SIZE_DIALOG);
        }
    }

    /**
     * @brief Update the ethernet connection status display
     *
     * Checks if ethernet is connected and updates the status message and icon accordingly.
     */
    void WifiTab::update_ethernet_status()
    {
        // Check Ethernet status asynchronously to avoid UI freezing
        Glib::Thread::create(
            [this]()
            {
                bool ethernet_connected = manager_->is_ethernet_connected();

                // Use Glib::signal_idle to update UI from the main thread
                Glib::signal_idle().connect_once(
                    [this, ethernet_connected]()
                    {
                        if (ethernet_connected)
                        {
                            // Only add the ethernet box to the UI if it's not already there
                            if (!ethernet_box_added_)
                            {
                                // Add ethernet box after the separator but before the network list
                                main_box_->pack_start(*ethernet_box_, Gtk::PACK_SHRINK);
                                main_box_->reorder_child(*ethernet_box_, 2); // Position after header and separator
                                ethernet_box_->show_all();
                                ethernet_box_added_ = true;
                            }
                        }
                        else
                        {
                            // Remove ethernet box from UI if it was previously added
                            if (ethernet_box_added_)
                            {
                                main_box_->remove(*ethernet_box_);
                                ethernet_box_added_ = false;
                            }
                        }
                    });
            },
            false); // non-joinable thread
    }

    /**
     * @brief Handler for WiFi switch toggle events
     *
     * Enables or disables WiFi based on the switch state.
     * Temporarily disables the switch during state change to prevent recursive calls.
     */
    void WifiTab::on_wifi_switch_toggled()
    {
        bool enabled = wifi_switch_.get_active();

        // Temporarily disable the switch to prevent recursive calls
        wifi_switch_.set_sensitive(false);

        if (enabled)
        {
            manager_->enable_wifi();
        }
        else
        {
            manager_->disable_wifi();
        }

        // Re-enable the switch after a short delay (1 second)
        Glib::signal_timeout().connect_once([this]()
                                            { wifi_switch_.set_sensitive(true); }, 1000);
    }

    /**
     * @brief Update the list of displayed WiFi networks
     * @param networks Vector of Network objects to display
     *
     * Clears the current list of network widgets and creates new ones
     * for each network in the provided vector.
     */
    void WifiTab::update_network_list(const std::vector<Network> &networks)
    {
        // Remove all existing network widgets
        for (auto &widget : widgets_)
        {
            container_.remove(*widget);
        }
        widgets_.clear();

        // Remove the loading label if it exists
        if (loading_label_ != nullptr)
        {
            container_.remove(*loading_label_);
            loading_label_ = nullptr;
        }

        // Remove the "no networks" label if it exists
        if (no_networks_label_ != nullptr)
        {
            container_.remove(*no_networks_label_);
            no_networks_label_ = nullptr;
        }

        // If WiFi is enabled but no networks were found, show a message
        if (networks.empty() && manager_->is_wifi_enabled())
        {
            no_networks_label_ = Gtk::manage(new Gtk::Label("No wireless networks found"));
            no_networks_label_->set_margin_top(20);
            no_networks_label_->set_margin_bottom(20);
            container_.pack_start(*no_networks_label_, Gtk::PACK_SHRINK);
        }
        else
        { // Create widgets for each detected network
            // Keep the connected wifi at the top, then sort by signal strength
            std::vector<Network> sorted_networks = networks;
            std::sort(sorted_networks.begin(), sorted_networks.end(), [](const Network &a, const Network &b)
                      {
                // Always prioritize connected networks
                if(a.connected != b.connected)
                    return a.connected;
                // For non-connected networks, sort by signal strength (higher first)
                return a.signal_strength > b.signal_strength; });
            for (const auto &net : sorted_networks)
            {
                auto widget = std::make_unique<WifiNetworkWidget>(net, manager_);
                container_.pack_start(*widget, Gtk::PACK_SHRINK);
                widgets_.push_back(std::move(widget));
            }
        }

        // Update ethernet status when network list is refreshed
        update_ethernet_status();

        show_all_children();
    }

    /**
     * @brief Perform a delayed network scan
     *
     * Called after the tab is fully loaded and visible to the user.
     * Delays the scan by a short time to ensure UI responsiveness.
     * Uses asynchronous scanning to prevent UI freezing.
     */
    void WifiTab::perform_delayed_scan()
    {
        if (initial_scan_performed_)
        {
            return; // Prevent duplicate scans
        }

        initial_scan_performed_ = true;

        if (manager_->is_wifi_enabled())
        {
            // Show scanning indicator
            scan_button_.set_sensitive(false);
            scan_button_.set_label("Scanning...");

            // Perform the scan asynchronously to prevent UI freezing
            manager_->scan_networks_async();

            // Re-enable the scan button after a short delay
            Glib::signal_timeout().connect_once([this]()
                                                {
            scan_button_.set_sensitive(true);
            scan_button_.set_label("Scan");
            // Update ethernet status
            update_ethernet_status(); }, 2000);
        }
    }

} // namespace Wifi
