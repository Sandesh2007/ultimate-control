#include "WifiNetworkWidget.hpp"
#include <gtkmm/messagedialog.h>
#include <gtkmm/spinner.h>
#include <glibmm/thread.h>
#include <iostream>

namespace Wifi {

WifiNetworkWidget::WifiNetworkWidget(const Network& network, std::shared_ptr<WifiManager> manager)
: Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5),
  manager_(std::move(manager)),
  network_(network),
  network_info_box_(Gtk::ORIENTATION_HORIZONTAL, 8),
  controls_box_(Gtk::ORIENTATION_HORIZONTAL, 8),
  ssid_label_(network.ssid),
  signal_label_(std::to_string(network.signal_strength) + "%"),
  status_label_(network.connected ? "Connected" : (network.secured ? "Secured" : "Open")),
  connect_button_(),
  forget_button_(),
  share_button_()
{
    // Set up the main container
    set_margin_start(10);
    set_margin_end(10);
    set_margin_top(8);
    set_margin_bottom(8);

    // Add a frame around the widget for better visual separation
    Gtk::Frame* frame = Gtk::manage(new Gtk::Frame());
    frame->set_shadow_type(Gtk::SHADOW_ETCHED_IN);
    pack_start(*frame, Gtk::PACK_EXPAND_WIDGET);

    // Create a box inside the frame
    Gtk::Box* inner_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8));
    inner_box->set_margin_start(10);
    inner_box->set_margin_end(10);
    inner_box->set_margin_top(10);
    inner_box->set_margin_bottom(10);
    frame->add(*inner_box);

    // Set up the network info box
    update_signal_icon(network.signal_strength);
    update_security_icon(network.secured);
    update_connection_status(network.connected);

    // Make the SSID label bold and larger
    Pango::AttrList attrs;
    auto font_desc = Pango::FontDescription("Bold");
    auto attr = Pango::Attribute::create_attr_font_desc(font_desc);
    attrs.insert(attr);
    ssid_label_.set_attributes(attrs);

    // Add network info widgets to the info box
    network_info_box_.pack_start(signal_icon_, Gtk::PACK_SHRINK);
    network_info_box_.pack_start(ssid_label_, Gtk::PACK_SHRINK);
    network_info_box_.pack_start(security_icon_, Gtk::PACK_SHRINK);
    network_info_box_.pack_start(status_icon_, Gtk::PACK_SHRINK);

    // Add signal strength label
    Gtk::Box* signal_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
    Gtk::Label* signal_prefix = Gtk::manage(new Gtk::Label("Signal Strength:"));
    signal_box->pack_start(*signal_prefix, Gtk::PACK_SHRINK);

    // Set the signal label text directly
    signal_label_.set_text(std::to_string(network_.signal_strength) + "%");

    signal_box->pack_start(signal_label_, Gtk::PACK_SHRINK);

    // Set up the connect button with an icon
    if (network.connected) {
        connect_button_.set_image_from_icon_name("network-wireless-connected-symbolic", Gtk::ICON_SIZE_BUTTON);
        connect_button_.set_label("Disconnect");
    } else {
        connect_button_.set_image_from_icon_name("network-wireless-signal-excellent-symbolic", Gtk::ICON_SIZE_BUTTON);
        connect_button_.set_label("Connect");
    }

    // Set up the forget button with an icon
    forget_button_.set_image_from_icon_name("user-trash-symbolic", Gtk::ICON_SIZE_BUTTON);
    forget_button_.set_label("Forget");
    forget_button_.set_tooltip_text("Forget this network");

    // Set up the share button with a QR code icon
    share_button_.set_image_from_icon_name("emblem-shared-symbolic", Gtk::ICON_SIZE_BUTTON);
    share_button_.set_label("Share");
    share_button_.set_tooltip_text("Share network via QR code");

    // Add controls to the controls box
    controls_box_.pack_end(connect_button_, Gtk::PACK_SHRINK);
    controls_box_.pack_start(forget_button_, Gtk::PACK_SHRINK);
    controls_box_.pack_start(share_button_, Gtk::PACK_SHRINK);

    // Add all components to the inner box
    inner_box->pack_start(network_info_box_, Gtk::PACK_SHRINK);
    inner_box->pack_start(*signal_box, Gtk::PACK_SHRINK);
    inner_box->pack_start(controls_box_, Gtk::PACK_SHRINK);

    // Connect signals
    connect_button_.signal_clicked().connect(sigc::mem_fun(*this, &WifiNetworkWidget::on_connect_clicked));
    forget_button_.signal_clicked().connect(sigc::mem_fun(*this, &WifiNetworkWidget::on_forget_clicked));
    share_button_.signal_clicked().connect(sigc::mem_fun(*this, &WifiNetworkWidget::on_share_clicked));

    show_all_children();
}

WifiNetworkWidget::~WifiNetworkWidget() = default;

std::string WifiNetworkWidget::convert_signal_to_quality(int signal_strength) {
    // Simply return the signal strength as a percentage
    // This is the raw value from nmcli which is already a percentage
    return std::to_string(signal_strength) + "%";
}

void WifiNetworkWidget::update_signal_icon(int signal_strength) {
    std::string icon_name;

    // Use signal_strength directly to determine the icon
    if (signal_strength < 20) {
        icon_name = "network-wireless-signal-none-symbolic";
    } else if (signal_strength < 40) {
        icon_name = "network-wireless-signal-weak-symbolic";
    } else if (signal_strength < 60) {
        icon_name = "network-wireless-signal-ok-symbolic";
    } else if (signal_strength < 80) {
        icon_name = "network-wireless-signal-good-symbolic";
    } else {
        icon_name = "network-wireless-signal-excellent-symbolic";
    }

    signal_icon_.set_from_icon_name(icon_name, Gtk::ICON_SIZE_LARGE_TOOLBAR);
}

void WifiNetworkWidget::update_security_icon(bool secured) {
    if (secured) {
        security_icon_.set_from_icon_name("channel-secure-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
        security_icon_.set_tooltip_text("Secured Network");
    } else {
        security_icon_.set_from_icon_name("channel-insecure-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
        security_icon_.set_tooltip_text("Open Network");
    }
}

void WifiNetworkWidget::update_connection_status(bool connected) {
    if (connected) {
        status_icon_.set_from_icon_name("network-wireless-connected-symbolic", Gtk::ICON_SIZE_SMALL_TOOLBAR);
        status_icon_.set_tooltip_text("Connected");
    } else {
        status_icon_.clear();
    }
}

void WifiNetworkWidget::on_connect_clicked() {
    // Store the SSID we're trying to connect to
    std::string target_ssid = network_.ssid;

    if (network_.connected) {
        manager_->disconnect();
    } else {
        // First try to connect without a password (for saved networks)
        std::string security_type = network_.secured ? "wpa-psk" : "";

        // Try to connect with empty password first - this will use saved connections if available
        std::cout << "Trying to connect to " << target_ssid << " using saved credentials..." << std::endl;
        manager_->connect(target_ssid, "", security_type);

        // Wait a moment for the connection to establish
        Glib::usleep(1000000); // 1 second

        // Check if we're now connected to this network
        manager_->scan_networks();
        bool connected = false;
        for (const auto& net : manager_->get_networks()) {
            if (net.ssid == target_ssid && net.connected) {
                connected = true;
                break;
            }
        }

        if (connected) {
            // Show a success message
            Gtk::MessageDialog success_dialog(*dynamic_cast<Gtk::Window*>(get_toplevel()),
                                           "Successfully connected to " + target_ssid,
                                           false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
            success_dialog.set_secondary_text("Connected using saved credentials");
            success_dialog.run();
            return;
        }

        // If we're still not connected and it's a secured network, ask for password
        if (network_.secured) {
            // Create a styled dialog for password entry
            Gtk::Dialog dialog("Enter WiFi Password", true);
            dialog.set_default_size(300, -1);
            dialog.set_border_width(10);

            // Create a box with an icon for the dialog
            Gtk::Box* content_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
            Gtk::Image* lock_icon = Gtk::manage(new Gtk::Image());
            lock_icon->set_from_icon_name("channel-secure-symbolic", Gtk::ICON_SIZE_DIALOG);
            content_box->pack_start(*lock_icon, Gtk::PACK_SHRINK);

            // Create a vertical box for the network name and password entry
            Gtk::Box* entry_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10));

            // Add network name label
            Gtk::Label* network_label = Gtk::manage(new Gtk::Label());
            network_label->set_markup("<b>" + target_ssid + "</b>");
            network_label->set_halign(Gtk::ALIGN_START);
            entry_box->pack_start(*network_label, Gtk::PACK_SHRINK);

            // Add password entry field with label
            Gtk::Label* password_label = Gtk::manage(new Gtk::Label("Password:"));
            password_label->set_halign(Gtk::ALIGN_START);
            entry_box->pack_start(*password_label, Gtk::PACK_SHRINK);

            Gtk::Entry* entry = Gtk::manage(new Gtk::Entry());
            entry->set_visibility(false);
            entry->set_invisible_char('*');
            entry->set_activates_default(true);
            entry_box->pack_start(*entry, Gtk::PACK_SHRINK);

            content_box->pack_start(*entry_box, Gtk::PACK_EXPAND_WIDGET);

            dialog.get_content_area()->pack_start(*content_box, Gtk::PACK_EXPAND_WIDGET);
            dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
            dialog.add_button("Connect", Gtk::RESPONSE_OK);
            dialog.set_default_response(Gtk::RESPONSE_OK);

            dialog.show_all_children();

            int result = dialog.run();
            if (result == Gtk::RESPONSE_OK) {
                std::string password = entry->get_text();

                // Show a simple message
                std::cout << "Connecting to " << target_ssid << " with password..." << std::endl;

                // Connect directly
                manager_->connect(target_ssid, password, security_type);

                // Wait a moment for the connection to establish
                Glib::usleep(1000000); // 1 second

                // Check if connection was successful
                manager_->scan_networks();
                bool connected = false;
                for (const auto& net : manager_->get_networks()) {
                    if (net.ssid == target_ssid && net.connected) {
                        connected = true;
                        break;
                    }
                }

                if (connected) {
                    Gtk::MessageDialog success_dialog(*dynamic_cast<Gtk::Window*>(get_toplevel()),
                                                  "Successfully connected to " + target_ssid,
                                                  false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
                    success_dialog.run();
                } else {
                    Gtk::MessageDialog error_dialog(*dynamic_cast<Gtk::Window*>(get_toplevel()),
                                                "Failed to connect to " + target_ssid,
                                                false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
                    error_dialog.set_secondary_text("Please check your password and try again.");
                    error_dialog.run();
                }
            }
        }
    }
}

void WifiNetworkWidget::on_forget_clicked() {
    // Store the SSID we're trying to forget
    std::string target_ssid = network_.ssid;

    // Create a confirmation dialog
    Gtk::MessageDialog dialog(*dynamic_cast<Gtk::Window*>(get_toplevel()),
                             "Are you sure you want to forget this network?",
                             false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO, true);

    // Add network name to the dialog
    Gtk::Box* content_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
    Gtk::Image* wifi_icon = Gtk::manage(new Gtk::Image());
    wifi_icon->set_from_icon_name("network-wireless-symbolic", Gtk::ICON_SIZE_DIALOG);
    content_box->pack_start(*wifi_icon, Gtk::PACK_SHRINK);

    Gtk::Label* network_label = Gtk::manage(new Gtk::Label());
    network_label->set_markup("<b>" + target_ssid + "</b>");
    network_label->set_halign(Gtk::ALIGN_START);
    content_box->pack_start(*network_label, Gtk::PACK_SHRINK);

    dialog.get_content_area()->pack_start(*content_box, Gtk::PACK_SHRINK);
    dialog.show_all_children();

    int result = dialog.run();
    if (result == Gtk::RESPONSE_YES) {
        manager_->forget_network(target_ssid);

        // Show a success message
        Gtk::MessageDialog success_dialog(*dynamic_cast<Gtk::Window*>(get_toplevel()),
                                       "Network forgotten",
                                       false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
        success_dialog.set_secondary_text("Successfully removed all saved connections for " + target_ssid);
        success_dialog.run();
    }
}

void WifiNetworkWidget::on_share_clicked() {
    // Store the SSID we're trying to share
    std::string target_ssid = network_.ssid;

    // Create a dialog to display the QR code
    Gtk::Dialog dialog("Share WiFi Network", *dynamic_cast<Gtk::Window*>(get_toplevel()), true);
    dialog.set_default_size(350, 400);
    dialog.set_border_width(10);

    // Create a box for the dialog content
    Gtk::Box* content_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10));
    content_box->set_border_width(10);

    // Add network name with icon
    Gtk::Box* header_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 10));
    Gtk::Image* wifi_icon = Gtk::manage(new Gtk::Image());
    wifi_icon->set_from_icon_name("network-wireless-symbolic", Gtk::ICON_SIZE_DIALOG);
    header_box->pack_start(*wifi_icon, Gtk::PACK_SHRINK);

    Gtk::Label* network_label = Gtk::manage(new Gtk::Label());
    network_label->set_markup("<span size='large'><b>" + target_ssid + "</b></span>");
    network_label->set_halign(Gtk::ALIGN_START);
    header_box->pack_start(*network_label, Gtk::PACK_SHRINK);

    content_box->pack_start(*header_box, Gtk::PACK_SHRINK);

    // Add a separator
    Gtk::Separator* separator = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
    content_box->pack_start(*separator, Gtk::PACK_SHRINK);

    // Create a drawing area for the QR code
    Gtk::DrawingArea* drawing_area = Gtk::manage(new Gtk::DrawingArea());
    drawing_area->set_size_request(300, 300);

    // Create the QR code
    Utils::QRCode qrcode(Utils::QRCode::Version::V3, Utils::QRCode::ErrorCorrection::M);

    // Get password if needed
    std::string password;
    if (network_.secured && !network_.connected) {
        // Ask for password if the network is secured and we're not connected
        Gtk::Dialog pwd_dialog("Enter WiFi Password", *dynamic_cast<Gtk::Window*>(get_toplevel()), true);
        pwd_dialog.set_default_size(300, -1);
        pwd_dialog.set_border_width(10);

        Gtk::Box* pwd_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 10));
        pwd_box->set_border_width(10);

        Gtk::Label* pwd_label = Gtk::manage(new Gtk::Label("Enter the password to include in the QR code:"));
        pwd_box->pack_start(*pwd_label, Gtk::PACK_SHRINK);

        Gtk::Entry* pwd_entry = Gtk::manage(new Gtk::Entry());
        pwd_entry->set_visibility(false);
        pwd_entry->set_invisible_char('*');
        pwd_box->pack_start(*pwd_entry, Gtk::PACK_SHRINK);

        pwd_dialog.get_content_area()->pack_start(*pwd_box, Gtk::PACK_SHRINK);
        pwd_dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        pwd_dialog.add_button("OK", Gtk::RESPONSE_OK);
        pwd_dialog.set_default_response(Gtk::RESPONSE_OK);

        pwd_dialog.show_all_children();

        int result = pwd_dialog.run();
        if (result == Gtk::RESPONSE_OK) {
            password = pwd_entry->get_text();
        } else {
            return; // User cancelled
        }
    }

    // Format the WiFi network information
    std::string auth_type = network_.secured ? "WPA" : "nopass";
    std::string qr_data = Utils::QRCode::formatWifiNetwork(target_ssid, password, false, auth_type);

    // Encode the data
    qrcode.encode(qr_data);

    // Draw the QR code when the drawing area is exposed
    drawing_area->signal_draw().connect([&qrcode, drawing_area](const Cairo::RefPtr<Cairo::Context>& cr) -> bool {
        Gtk::Allocation allocation = drawing_area->get_allocation();
        const int width = allocation.get_width();
        const int height = allocation.get_height();
        const int size = std::min(width, height);

        // Center the QR code
        double x = (width - size) / 2.0;
        double y = (height - size) / 2.0;

        // Clear the background
        cr->set_source_rgb(1.0, 1.0, 1.0); // White
        cr->paint();

        // Draw the QR code
        cr->set_source_rgb(0.0, 0.0, 0.0); // Black
        qrcode.draw(cr, x, y, size);

        return true;
    });

    content_box->pack_start(*drawing_area, Gtk::PACK_EXPAND_WIDGET);

    // Add instructions
    Gtk::Label* instructions = Gtk::manage(new Gtk::Label("Scan this QR code with a phone camera\nor WiFi configuration app to connect"));
    instructions->set_line_wrap(true);
    content_box->pack_start(*instructions, Gtk::PACK_SHRINK);

    // Add the content box to the dialog
    dialog.get_content_area()->pack_start(*content_box, Gtk::PACK_EXPAND_WIDGET);

    // Add a close button
    dialog.add_button("Close", Gtk::RESPONSE_CLOSE);
    dialog.set_default_response(Gtk::RESPONSE_CLOSE);

    // Show all children and run the dialog
    dialog.show_all_children();
    dialog.run();
}

} // namespace Wifi
