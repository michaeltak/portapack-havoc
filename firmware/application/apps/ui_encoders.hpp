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

#include "ui.hpp"
#include "ui_tabview.hpp"
#include "ui_transmitter.hpp"
#include "transmitter_model.hpp"
#include "encoders.hpp"
#include "de_bruijn.hpp"

using namespace encoders;

namespace ui {

class EncodersConfigView : public View {
public:
	EncodersConfigView(NavigationView& nav, Rect parent_rect);
	
	EncodersConfigView(const EncodersConfigView&) = delete;
	EncodersConfigView(EncodersConfigView&&) = delete;
	EncodersConfigView& operator=(const EncodersConfigView&) = delete;
	EncodersConfigView& operator=(EncodersConfigView&&) = delete;
	
	void focus() override;
	void on_show() override;
	
	//uint8_t repeat_min();
	uint32_t samples_per_bit();
	uint32_t pause_symbols();
	void generate_frame(bool is_debruijn, uint32_t debruijn_bits);
	
	std::string frame_fragments = "0";
	const encoder_def_t * encoder_def { };
	uint8_t bits_per_packet; 	//Euquiq: the number of bits needed from de_bruijn, depends on the encoder's needs

private:

	int16_t waveform_buffer[550];
	
	uint8_t enc_type = 0;

	void draw_waveform();
	void on_bitfield();
	void on_type_change(size_t index);
	
	Labels labels {
		{ { 1 * 8, 0 }, "Type:", Color::light_grey() },
		{ { 16 * 8, 0 }, "Clk:", Color::light_grey() },
		{ { 24 * 8, 0 }, "kHz", Color::light_grey() },
		{ { 14 * 8, 2 * 8 }, "Frame:", Color::light_grey() },
		{ { 26 * 8, 2 * 8 }, "us", Color::light_grey() },
		{ { 2 * 8, 4 * 8 }, "Symbols:", Color::light_grey() },
		{ { 1 * 8, 11 * 8 }, "Waveform:", Color::light_grey() }
	};

	OptionsField options_enctype {		// Options are loaded at runtime
		{ 6 * 8, 0 },
		7,
		{
		}
	};

	NumberField field_clk {
		{ 21 * 8, 0 },
		3,
		{ 1, 500 },
		1,
		' '
	};

	NumberField field_frameduration {
		{ 21 * 8, 2 * 8 },
		5,
		{ 300, 99999 },
		100,
		' '
	};
	
	SymField symfield_word {
		{ 2 * 8, 6 * 8 },
		20,
		SymField::SYMFIELD_DEF
	};
	
	Text text_format {
		{ 2 * 8, 8 * 8, 24 * 8, 16 },
		""
	};
	
	Waveform waveform {
		{ 0, 14 * 8, 240, 32 },
		waveform_buffer,
		0,
		0,
		true,
		Color::yellow()
	};
};


class EncodersScanView : public View {
public:
	EncodersScanView(NavigationView& nav, Rect parent_rect);

	Labels labels {
		{ { 6 * 8, 0 * 16 }, "Encoder:", Color::light_grey() },
		{ { 2 * 8, 1 * 16 }, "Word Format:", Color::light_grey() },
		{ { 1 * 8, 2 * 16 }, "Address bits:", Color::light_grey() },
		{ { 1 * 8, 3 * 16 }, "AFSK repeats:", Color::light_grey() },
		{ { 0 * 8, 4 * 16 }, "DeBruijn bits:", Color::light_grey() }
	};

	Text text_debug_encoder {
		{ 14 * 8, 0 * 16, 16 * 8, 16 },
		""
	};

	Text text_debug_word_format {
		{ 14 * 8, 1 * 16, 16 * 8, 16 },
		""
	};

	Text text_debug_abits {
		{ 14 * 8, 2 * 16, 2 * 8, 16 },
		""
	};
	
	Text text_debug_afsk_repeats {
		{ 14 * 8, 3 * 16, 2 * 8, 16 },
		""
	};

	Text text_debug_de_bruijn_bits {
		{ 14 * 8, 4 * 16, 5 * 8, 16 },
		""
	};

	Text text_debug {
		{ 0 * 8, 7 * 16, 24 * 8, 16 },
		""
	};
};

class EncodersView : public View {
public:
	EncodersView(NavigationView& nav);
	~EncodersView();
	
	void focus() override;
	
	std::string title() const override { return "OOK transmit"; };


private:
	NavigationView& nav_;

	enum tx_modes {
		IDLE = 0,
		SINGLE,
		SCAN
	};
	
	tx_modes tx_mode = IDLE;
	uint8_t repeat_index { 0 };
	uint32_t scan_count;
	uint32_t scan_index;
	double scan_progress;
	bool abort_scan = false;
	char str[16];

	uint8_t afsk_repeats;
	de_bruijn debruijn_seq;
	
	void update_progress();
	void start_tx(const bool scan);
	void on_tx_progress(const uint32_t progress, const bool done);
	
	const Style style_val {
		.font = font::fixed_8x16,
		.background = Color::green(),
		.foreground = Color::black(),
	};
	const Style style_cancel {
		.font = font::fixed_8x16,
		.background = Color::red(),
		.foreground = Color::black(),
	};
	
	Rect view_rect = { 0, 4 * 8, 240, 154 };
	
	EncodersConfigView view_config { nav_, view_rect };
	EncodersScanView view_scan { nav_, view_rect };
	
	TabView tab_view {
		{ "Config", Color::cyan(), &view_config },
		{ "De Bruijn", Color::green(), &view_scan },
	};

	Text text_status {
		{ 2 * 8, 13 * 16, 128, 16 },
		"Ready"
	};

	Button button_scan {
		{ 20 * 8, 12 * 16, 64, 28 },
		"SCAN"
	};
	
	ProgressBar progressbar {
		{ 2 * 8, 13 * 16 + 20, 208, 16 }
	};
	
	TransmitterView tx_view {
		16 * 16,
		50000,
		9
	};
	
	MessageHandlerRegistration message_handler_tx_progress {
		Message::ID::TXProgress,
		[this](const Message* const p) {
			const auto message = *reinterpret_cast<const TXProgressMessage*>(p);
			this->on_tx_progress(message.progress, message.done);
		}
	};
};

} /* namespace ui */
