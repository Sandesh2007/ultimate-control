/**
 * @file VolumeWidget.cpp
 * @brief Implementation of the audio device control widget
 *
 * This file implements the VolumeWidget class which displays a single
 * audio device with volume slider and mute button controls.
 */

#include "VolumeWidget.hpp"

namespace Volume
{

    /**
     * @brief Constructor for the volume widget
     * @param sink The audio device to display
     * @param manager Shared pointer to the volume manager
     *
     * Creates a widget that displays information about an audio device and
     * provides controls for adjusting its volume and mute state.
     */
    VolumeWidget::VolumeWidget(const AudioSink &sink, std::shared_ptr<VolumeManager> manager)
        : Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5),                                                                          // Vertical layout with 5px spacing
          manager_(std::move(manager)),                                                                                    // Store volume manager
          sink_name_(sink.name),                                                                                           // Store device name
          is_input_device_(sink.name.find("input") != std::string::npos || sink.name.find("source") != std::string::npos), // Determine device type
          is_default_(sink.is_default),                                                                                    // Store default device status
          device_box_(Gtk::ORIENTATION_HORIZONTAL, 5),                                                                     // Horizontal box for device info
          control_box_(Gtk::ORIENTATION_HORIZONTAL, 10),                                                                   // Horizontal box for controls
          default_box_(Gtk::ORIENTATION_HORIZONTAL, 5),                                                                    // Horizontal box for default device checkbox
          label_(sink.description),                                                                                        // Device description label
          volume_scale_(Gtk::ORIENTATION_HORIZONTAL),                                                                      // Horizontal volume slider
          mute_button_(),
          default_check_("Set as default")
    {
        // Set up the main container with margins for better spacing
        set_margin_start(10);
        set_margin_end(10);
        set_margin_top(8);
        set_margin_bottom(8);

        // Add a frame around the widget for visual separation between devices
        Gtk::Frame *frame = Gtk::manage(new Gtk::Frame());
        frame->set_shadow_type(Gtk::SHADOW_ETCHED_IN);
        pack_start(*frame, Gtk::PACK_EXPAND_WIDGET);

        // Create a vertical box inside the frame with padding
        Gtk::Box *inner_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 5));
        inner_box->set_margin_start(10);
        inner_box->set_margin_end(10);
        inner_box->set_margin_top(10);
        inner_box->set_margin_bottom(10);
        frame->add(*inner_box);

        // Set appropriate device icon based on whether it's an input or output device
        if (is_input_device_)
        {
            device_icon_.set_from_icon_name("audio-input-microphone-symbolic", Gtk::ICON_SIZE_LARGE_TOOLBAR);
        }
        else
        {
            device_icon_.set_from_icon_name("audio-speakers-symbolic", Gtk::ICON_SIZE_LARGE_TOOLBAR);
        }

        // Set up the device name box with icon and label
        device_box_.pack_start(device_icon_, Gtk::PACK_SHRINK);
        device_box_.pack_start(label_, Gtk::PACK_SHRINK);
        inner_box->pack_start(device_box_, Gtk::PACK_SHRINK);

        // Set up the volume slider with range 0-100
        volume_scale_.set_range(0, 100);
        volume_scale_.set_value(sink.volume);
        volume_scale_.set_size_request(200, -1);
        volume_scale_.set_draw_value(true);
        volume_scale_.set_has_origin(true);
        volume_scale_.set_can_focus(false); // Prevent tab navigation to this slider

        // Add preset marks at 0%, 25%, 50%, 75%, and 100%
        volume_scale_.add_mark(0, Gtk::POS_BOTTOM, "0%");
        volume_scale_.add_mark(25, Gtk::POS_BOTTOM, "25%");
        volume_scale_.add_mark(50, Gtk::POS_BOTTOM, "50%");
        volume_scale_.add_mark(75, Gtk::POS_BOTTOM, "75%");
        volume_scale_.add_mark(100, Gtk::POS_BOTTOM, "100%");

        // Set up the mute toggle button with initial state
        mute_button_.set_active(sink.muted);
        mute_button_.set_relief(Gtk::RELIEF_NONE);
        mute_button_.set_can_focus(false); // Prevent tab navigation to this button

        // Initialize the volume icon based on current volume and mute state
        update_volume_icon(sink.volume, sink.muted);

        // Arrange volume icon, slider, and mute button in the control box
        control_box_.pack_start(volume_icon_, Gtk::PACK_SHRINK);
        control_box_.pack_start(volume_scale_, Gtk::PACK_EXPAND_WIDGET);
        control_box_.pack_start(mute_button_, Gtk::PACK_SHRINK);
        inner_box->pack_start(control_box_, Gtk::PACK_SHRINK);

        // Set up the default device checkbox
        default_check_.set_active(sink.is_default);
        default_check_.set_can_focus(false); // Prevent tab navigation to this checkbox
        default_box_.pack_start(default_check_, Gtk::PACK_SHRINK);
        inner_box->pack_start(default_box_, Gtk::PACK_SHRINK);

        // Connect signal handlers for volume and mute controls
        volume_scale_.signal_value_changed().connect([this]()
                                                     {
        int vol = static_cast<int>(volume_scale_.get_value());
        manager_->set_volume(sink_name_, vol);
        update_volume_icon(vol, mute_button_.get_active()); });

        mute_button_.signal_toggled().connect([this]()
                                              {
        manager_->toggle_mute(sink_name_);
        update_volume_icon(static_cast<int>(volume_scale_.get_value()), mute_button_.get_active()); });

        // Connect signal handler for default device checkbox
        default_check_.signal_toggled().connect(sigc::mem_fun(*this, &VolumeWidget::on_default_toggled));

        show_all_children();
    }

    /**
     * @brief Handler for volume slider changes
     *
     * Updates the device volume and icon when the slider is moved.
     */
    void VolumeWidget::on_volume_changed()
    {
        int vol = static_cast<int>(volume_scale_.get_value());
        manager_->set_volume(sink_name_, vol);
        update_volume_icon(vol, mute_button_.get_active());
    }

    /**
     * @brief Handler for mute button toggles
     *
     * Toggles the device mute state and updates the icon.
     */
    void VolumeWidget::on_mute_toggled()
    {
        manager_->toggle_mute(sink_name_);
        update_volume_icon(static_cast<int>(volume_scale_.get_value()), mute_button_.get_active());
    }

    /**
     * @brief Update the volume icon based on level and mute state
     * @param volume The current volume level (0-100)
     * @param muted Whether the device is muted
     *
     * Sets appropriate icons for both the volume indicator and mute button
     * based on the current volume level and mute state.
     */
    void VolumeWidget::update_volume_icon(int volume, bool muted)
    {
        std::string icon_name;

        // Select appropriate icons based on mute state and device type
        if (muted)
        {
            if (is_input_device_)
            {
                icon_name = "microphone-sensitivity-muted-symbolic";
                mute_button_.set_image_from_icon_name("microphone-sensitivity-muted-symbolic", Gtk::ICON_SIZE_BUTTON);
            }
            else
            {
                icon_name = "audio-volume-muted-symbolic";
                mute_button_.set_image_from_icon_name("audio-volume-muted-symbolic", Gtk::ICON_SIZE_BUTTON);
            }
        }
        else
        { // Not muted - select icon based on volume level
            if (is_input_device_)
            {
                if (volume < 30)
                {
                    icon_name = "microphone-sensitivity-low-symbolic";
                    mute_button_.set_image_from_icon_name("microphone-sensitivity-high-symbolic", Gtk::ICON_SIZE_BUTTON);
                }
                else if (volume < 70)
                {
                    icon_name = "microphone-sensitivity-medium-symbolic";
                    mute_button_.set_image_from_icon_name("microphone-sensitivity-high-symbolic", Gtk::ICON_SIZE_BUTTON);
                }
                else
                {
                    icon_name = "microphone-sensitivity-high-symbolic";
                    mute_button_.set_image_from_icon_name("microphone-sensitivity-high-symbolic", Gtk::ICON_SIZE_BUTTON);
                }
            }
            else
            {
                if (volume < 30)
                {
                    icon_name = "audio-volume-low-symbolic";
                    mute_button_.set_image_from_icon_name("audio-volume-high-symbolic", Gtk::ICON_SIZE_BUTTON);
                }
                else if (volume < 70)
                {
                    icon_name = "audio-volume-medium-symbolic";
                    mute_button_.set_image_from_icon_name("audio-volume-high-symbolic", Gtk::ICON_SIZE_BUTTON);
                }
                else
                {
                    icon_name = "audio-volume-high-symbolic";
                    mute_button_.set_image_from_icon_name("audio-volume-high-symbolic", Gtk::ICON_SIZE_BUTTON);
                }
            }
        }

        volume_icon_.set_from_icon_name(icon_name, Gtk::ICON_SIZE_BUTTON);
    }

    /**
     * @brief Handler for default device check button toggles
     *
     * Sets this device as the default device for its type when checked.
     * If the user tries to uncheck it, we prevent that since a default device
     * must always be selected.
     */
    void VolumeWidget::on_default_toggled()
    {
        // If this is already the default device and user tries to uncheck it,
        // prevent that by setting it back to checked
        if (!default_check_.get_active() && is_default_)
        {
            // Block the signal handler temporarily to avoid recursion
            sigc::connection conn = default_check_.signal_toggled().connect(
                sigc::slot<void>([this]()
                                 {
                                     // Do nothing, just blocking the normal handler
                                 }));

            // Set it back to checked
            default_check_.set_active(true);

            // Unblock the signal handler
            conn.disconnect();

            return;
        }

        // If user is setting this as default
        if (default_check_.get_active())
        {
            manager_->set_default_device(sink_name_);
            is_default_ = true;
        }
    }

    /**
     * @brief Destructor for the volume widget
     */
    VolumeWidget::~VolumeWidget() = default;

} // namespace Volume
