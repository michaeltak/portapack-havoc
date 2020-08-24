/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "ui_mictx.hpp"

#include "baseband_api.hpp"
#include "audio.hpp"
#include "tonesets.hpp"
#include "portapack_hal.hpp"
#include "string_format.hpp"
#include "irq_controls.hpp"

#include <cstring>

using namespace tonekey;
using namespace portapack;

namespace ui
{

	void MicTXView::focus()
	{
		field_frequency.focus();
	}

	void MicTXView::update_vumeter()
	{
		vumeter.set_value(audio_level);
	}

	void MicTXView::on_tx_progress(const bool done)
	{
		// Roger beep played, stop transmitting
		if (done)
			set_tx(false);
	}

	void MicTXView::configure_baseband()
	{
		baseband::set_audiotx_config(
			sampling_rate / 20, // Update vu-meter at 20Hz
			transmitting ? transmitter_model.channel_bandwidth() : 0,
			mic_gain,
			TONES_F2D(tone_key_frequency(tone_key_index), sampling_rate));
	}

	void MicTXView::set_tx(bool enable)
	{
		if (enable)
		{
			if (rx_enabled)		//If audio RX is enabled
				rxaudio(false); //Then turn off audio RX
			transmitting = true;
			configure_baseband();
			transmitter_model.set_tuning_frequency(tx_frequency);
			transmitter_model.set_tx_gain(tx_gain);
			transmitter_model.set_rf_amp(tx_amp);
			transmitter_model.enable();
			portapack::pin_i2s0_rx_sda.mode(3); // This is already done in audio::init but gets changed by the CPLD overlay reprogramming
		}
		else
		{
			if (transmitting && rogerbeep_enabled)
			{
				baseband::request_beep(); //Transmit the roger beep
				transmitting = false;	  //And flag the end of the transmission so ...
			}
			else
			{ // (if roger beep was enabled, this will be executed after the beep ends transmitting.
				transmitting = false;
				configure_baseband();
				transmitter_model.disable();
				if (rx_enabled)	   //If audio RX is enabled and we've been transmitting
					rxaudio(true); //Turn back on audio RX
			}
		}
	}

	void MicTXView::do_timing()
	{
		if (va_enabled)
		{
			if (!transmitting)
			{
				// Attack
				if (audio_level >= va_level)
				{
					if ((attack_timer >> 8) >= attack_ms)
					{
						decay_timer = 0;
						attack_timer = 0;
						set_tx(true);
					}
					else
					{
						attack_timer += lcd_frame_duration;
					}
				}
				else
				{
					attack_timer = 0;
				}
			}
			else
			{
				// Decay
				if (audio_level < va_level)
				{
					if ((decay_timer >> 8) >= decay_ms)
					{
						decay_timer = 0;
						attack_timer = 0;
						set_tx(false);
					}
					else
					{
						decay_timer += lcd_frame_duration;
					}
				}
				else
				{
					decay_timer = 0;
				}
			}
		}
		else
		{
			// Check for PTT release
			const auto switches_state = get_switches_state();
			if (!switches_state[0] && transmitting) // Right button
				set_tx(false);
		}
	}

	void MicTXView::rxaudio(bool is_on)
	{
		if (is_on)
		{
			audio::input::stop();
			baseband::shutdown();
			baseband::run_image(portapack::spi_flash::image_tag_nfm_audio);
			receiver_model.set_modulation(ReceiverModel::Mode::NarrowbandFMAudio);
			receiver_model.set_sampling_rate(3072000);
			receiver_model.set_baseband_bandwidth(1750000);
			receiver_model.set_lna(rx_lna);
			receiver_model.set_vga(rx_vga);
			receiver_model.set_rf_amp(rx_amp);
			receiver_model.set_tuning_frequency(rx_frequency); // Now with separate controls!
			transmitter_model.disable(); //euquiq This prevents TX led to be on while receiving
			receiver_model.enable();
			audio::output::start();
		}
		else
		{ //These incredibly convoluted steps are required for the vumeter to reappear when stopping RX.
			receiver_model.disable();
			baseband::shutdown();
			baseband::run_image(portapack::spi_flash::image_tag_mic_tx);
			audio::input::start();
			//transmitter_model.enable();
			portapack::pin_i2s0_rx_sda.mode(3);
			//transmitting = false;
			configure_baseband();
			//transmitter_model.disable();
		}
	}

	void MicTXView::on_headphone_volume_changed(int32_t v)
	{
		//if (rx_enabled) {
		const auto new_volume = volume_t::decibel(v - 99) + audio::headphone::volume_range().max;
		receiver_model.set_headphone_volume(new_volume);
		//}
	}

	MicTXView::MicTXView(
		NavigationView &nav)
	{
		portapack::pin_i2s0_rx_sda.mode(3); // This is already done in audio::init but gets changed by the CPLD overlay reprogramming

		baseband::run_image(portapack::spi_flash::image_tag_mic_tx);

		add_children({&labels,
					  &vumeter,
					  &options_gain,
					  &check_va,
					  &field_va_level,
					  &field_va_attack,
					  &field_va_decay,
					  &field_bw,
					  &field_rfgain,
					  &option_txamp,
					  &field_frequency,
					  &options_tone_key,
					  &check_rogerbeep,
					  &check_rxactive,
					  &field_volume,
					  &field_squelch,
					  &field_rxfrequency,
					  &field_rxlna,
					  &field_rxvga,
					  &option_rxamp,
					  &text_ptt});

		tone_keys_populate(options_tone_key);
		options_tone_key.on_change = [this](size_t i, int32_t) {
			tone_key_index = i;
		};
		options_tone_key.set_selected_index(0);

		options_gain.on_change = [this](size_t, int32_t v) {
			mic_gain = v / 10.0;
			configure_baseband();
		};
		options_gain.set_selected_index(1); // x1.0

		tx_frequency = transmitter_model.tuning_frequency();
		field_frequency.set_value(tx_frequency);
		field_frequency.set_step(receiver_model.frequency_step());
		field_frequency.on_change = [this](rf::Frequency f) {
			tx_frequency = f;
		};
		field_frequency.on_edit = [this, &nav]() {
			// TODO: Provide separate modal method/scheme?
			auto new_view = nav.push<FrequencyKeypadView>(tx_frequency);
			new_view->on_changed = [this](rf::Frequency f) {
				tx_frequency = f;
				this->field_frequency.set_value(f);
				set_dirty();
			};
		};

		rx_frequency = receiver_model.tuning_frequency();
		field_rxfrequency.set_value(tx_frequency);
		field_rxfrequency.set_step(receiver_model.frequency_step());
		field_rxfrequency.on_change = [this](rf::Frequency f) {
			rx_frequency = f;
			if(rx_enabled)	//also dynamically retune if user is listening
				receiver_model.set_tuning_frequency(rx_frequency);			
		};
		field_rxfrequency.on_edit = [this, &nav]() {
			// TODO: Provide separate modal method/scheme?
			auto new_view = nav.push<FrequencyKeypadView>(rx_frequency);
			new_view->on_changed = [this](rf::Frequency f) {
				rx_frequency = f;
				this->field_rxfrequency.set_value(f);
				if(rx_enabled)	//also dynamically retune if user is listening
					receiver_model.set_tuning_frequency(rx_frequency);
				set_dirty();
			};
		};


		field_bw.on_change = [this](uint32_t v) {
			transmitter_model.set_channel_bandwidth(v * 1000);
		};
		field_bw.set_value(10);

		tx_gain = transmitter_model.tx_gain();
		field_rfgain.set_value(tx_gain);
		field_rfgain.on_change = [this](int32_t v) {
			tx_gain = v;
		};
		
		tx_amp = transmitter_model.rf_amp();
		option_txamp.set_by_value(tx_amp);
		option_txamp.on_change = [this](size_t, bool v) {
			tx_amp = v;
		};

		rx_lna = receiver_model.lna();
		field_rxlna.set_value(rx_lna);
 		field_rxlna.on_change = [this](int32_t v) {
 			rx_lna = v;
			if(rx_enabled)	//also dynamically adjust if user is listening
				receiver_model.set_lna(rx_lna);
		};

		rx_vga = receiver_model.vga();
		field_rxvga.set_value(rx_vga);
 		field_rxvga.on_change = [this](int32_t v) {
 			rx_vga = v;
			if(rx_enabled)	//also dynamically adjust if user is listening
				receiver_model.set_vga(rx_vga);
		};
 		
		rx_amp = receiver_model.rf_amp();
		option_rxamp.set_by_value(rx_amp);
 		option_rxamp.on_change = [this](size_t, bool v) {
 			rx_amp = v;
			if(rx_enabled)	//also dynamically adjust if user is listening
				receiver_model.set_rf_amp(rx_amp);
		};

		check_va.on_select = [this](Checkbox &, bool v) {
			va_enabled = v;
			text_ptt.hidden(v);		  //hide / show PTT text
			check_rxactive.hidden(v); //hide / show the RX AUDIO
			set_dirty();			  //Refresh display
		};

		check_rogerbeep.on_select = [this](Checkbox &, bool v) {
			rogerbeep_enabled = v;
		};

		field_va_level.on_change = [this](int32_t v) {
			va_level = v;
			vumeter.set_mark(v);
		};
		field_va_level.set_value(40);

		field_va_attack.on_change = [this](int32_t v) {
			attack_ms = v;
		};
		field_va_attack.set_value(500);

		field_va_decay.on_change = [this](int32_t v) {
			decay_ms = v;
		};
		field_va_decay.set_value(1000);

		check_rxactive.on_select = [this](Checkbox &, bool v) {
			rx_enabled = v;
			check_va.hidden(v); //Hide or show voice activation
			rxaudio(v);			//Activate-Deactivate audio rx accordingly
			set_dirty();		//Refresh interface
		};

		field_volume.set_value((receiver_model.headphone_volume() - audio::headphone::volume_range().max).decibel() + 99);
		field_volume.on_change = [this](int32_t v) { this->on_headphone_volume_changed(v); };

		field_squelch.on_change = [this](int32_t v) {
			receiver_model.set_squelch_level(100 - v);
		};
		field_squelch.set_value(0);
		receiver_model.set_squelch_level(0);

		transmitter_model.set_sampling_rate(sampling_rate);
		transmitter_model.set_baseband_bandwidth(1750000);

		set_tx(false);

		audio::set_rate(audio::Rate::Hz_24000);
		audio::input::start();
	}

	MicTXView::~MicTXView()
	{
		audio::input::stop();
		transmitter_model.disable();
		if (rx_enabled) //Also turn off audio rx if enabled
			rxaudio(false);
		baseband::shutdown();
	}

} // namespace ui
