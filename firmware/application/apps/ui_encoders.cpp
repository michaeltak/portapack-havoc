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

#include "ui_encoders.hpp"

#include "baseband_api.hpp"
#include "string_format.hpp"

using namespace portapack;

namespace ui {

EncodersConfigView::EncodersConfigView(
	NavigationView&, Rect parent_rect
) {
	using option_t = std::pair<std::string, int32_t>;
	std::vector<option_t> enc_options;
	size_t i;
	
	set_parent_rect(parent_rect);
	hidden(true);
	
	// Default encoder def
	encoder_def = &encoder_defs[0];
	
	add_children({
		&labels,
		&options_enctype,
		&field_clk,
		&field_frameduration,
		&symfield_word,
		&text_format,
		&waveform
	});

	// Load encoder types in option field
	for (i = 0; i < ENC_TYPES_COUNT; i++)
		enc_options.emplace_back(std::make_pair(encoder_defs[i].name, i));
	
	options_enctype.on_change = [this](size_t index, int32_t) {
		on_type_change(index);
	};
	
	options_enctype.set_options(enc_options);
	options_enctype.set_selected_index(0);
	
	symfield_word.on_change = [this]() {
		generate_frame(false, 0);
	};
	
	// Selecting input clock changes symbol and word duration
	field_clk.on_change = [this](int32_t value) {
		// value is in kHz, new_value is in us
		int32_t new_value = (encoder_def->clk_per_symbol * 1000000) / (value * 1000);
		if (new_value != field_frameduration.value())
			field_frameduration.set_value(new_value * encoder_def->word_length, false);
	};
	
	// Selecting word duration changes input clock and symbol duration
	field_frameduration.on_change = [this](int32_t value) {
		// value is in us, new_value is in kHz
		int32_t new_value = (value * 1000) / (encoder_def->word_length * encoder_def->clk_per_symbol);
		if (new_value != field_clk.value())
			field_clk.set_value(1000000 / new_value, false);
	};
}

void EncodersConfigView::focus() {
	options_enctype.focus();
}

void EncodersConfigView::on_type_change(size_t index) {
	std::string format_string = "";
	size_t word_length;
	char symbol_type;

	encoder_def = &encoder_defs[index];

	field_clk.set_value(encoder_def->default_speed / 1000);
	
	// SymField setup
	word_length = encoder_def->word_length;
	symfield_word.set_length(word_length);
	size_t n = 0, i = 0;
	while (n < word_length) {
		symbol_type = encoder_def->word_format[i++];
		if (symbol_type == 'A') {
			symfield_word.set_symbol_list(n++, encoder_def->address_symbols);
			format_string += 'A';
		} else if (symbol_type == 'D') {
			symfield_word.set_symbol_list(n++, encoder_def->data_symbols);
			format_string += 'D';
		}
	}
	
	// Ugly :( Pad to erase
	format_string.append(24 - format_string.size(), ' ');
	
	text_format.set(format_string);

	generate_frame(false, 0);
}

void EncodersConfigView::on_show() {
	options_enctype.set_selected_index(0);
	on_type_change(0);
}

void EncodersConfigView::draw_waveform() {
	size_t length = frame_fragments.length();

	for (size_t n = 0; n < length; n++)
		waveform_buffer[n] = (frame_fragments[n] == '0') ? 0 : 1;
	
	waveform.set_length(length);
	waveform.set_dirty();
}

void EncodersConfigView::generate_frame(bool is_debruijn, uint32_t debruijn_bits)
{
	uint8_t i = 0;
	frame_fragments.clear();

	if (!is_debruijn) //single tx
	{
		for (auto c : encoder_def->word_format)
		{
			if (c == 'S')
				frame_fragments += encoder_def->sync;
			else
				frame_fragments += encoder_def->bit_format[symfield_word.get_sym(i++)]; //Get_sym brings the index of the char chosen in the symfield, so 0, 1 or eventually 2
		}
	}
	else //Scaning on de_bruijn sequence:
	{
		uint8_t abit;
		uint8_t pos = bits_per_packet; //Only need the De Bruijn populated positions inside bits_per_packet (0 based!);

		for (auto c : encoder_def->word_format)
		{
			if (c == 'S')
				frame_fragments += encoder_def->sync;
			else if (c == 'D')		//Data, Take it from the symfield, as configured by user
				frame_fragments += encoder_def->bit_format[symfield_word.get_sym(i++)]; //Get_sym brings the index of the char chosen in the symfield, so 0, 1 or eventually 2
			else //Address: inject De Bruijn
			{		
				if ( debruijn_bits & (1 << (31 - pos)) )
				 	abit = 1;
				else
					abit = 0;

				pos--;
				//symfield_word.set_sym(i++,abit);
				symfield_word.set_sym(i,abit);
				frame_fragments += encoder_def->bit_format[symfield_word.get_sym(i++)]; 
				//i++; //Even while grabbing this address bit from debruijn, must move forward on the symfield, in case there is a 'D' further ahead
				//frame_fragments += encoder_def->bit_format[abit];
			}
			
		}
	}
	draw_waveform();
}

	// uint8_t EncodersConfigView::repeat_min()
	// {
	// 	return encoder_def->repeat_min;
	// }

	uint32_t EncodersConfigView::samples_per_bit()
	{
		return OOK_SAMPLERATE / ((field_clk.value() * 1000) / encoder_def->clk_per_fragment);
	}

	uint32_t EncodersConfigView::pause_symbols()
	{
		return encoder_def->pause_symbols;
	}

	EncodersScanView::EncodersScanView(
		NavigationView &, Rect parent_rect)
	{
		set_parent_rect(parent_rect);
		hidden(true);

		add_children({
			&labels,
			&text_debug_encoder,
			&text_debug_word_format,
			&text_debug_abits,
			&text_debug_afsk_repeats,
			&text_debug_de_bruijn_bits,
			&text_debug
		});
	}

	void EncodersView::focus()
	{
		tab_view.focus();
	}

	EncodersView::~EncodersView()
	{
		transmitter_model.disable();
		baseband::shutdown();
	}

	void EncodersView::update_progress()
	{
		text_status.set("            "); //euquiq: it was commented

		if (tx_mode == SINGLE)
		{
			std::string str_buffer = to_string_dec_uint(repeat_index) + "/" + to_string_dec_uint(afsk_repeats);
			text_status.set(str_buffer);
			progressbar.set_value(repeat_index);
		}
		else if (tx_mode == SCAN)
		{
			std::string str_buffer = to_string_dec_uint(repeat_index) + "/" +
									 to_string_dec_uint(afsk_repeats) + " " +
									 to_string_dec_uint(scan_index + 1) + "/" +
									 to_string_dec_uint(scan_count);
			text_status.set(str_buffer);
			progressbar.set_value(scan_progress);
		}
		else
		{
			text_status.set("Ready");
			progressbar.set_value(0);
		}
	}

	void EncodersView::on_tx_progress(const uint32_t progress, const bool done)
	{

		if (!done)
		{
			// Repeating...
			repeat_index = progress + 1;

			if (tx_mode == SCAN)
			{
				scan_progress++;
				update_progress();
			}
			else
			{
				update_progress();
			}
		}
		else
		{	// Done transmitting
			if ((tx_mode == SCAN) && (scan_index < (scan_count - 1)))
			{
				transmitter_model.disable();
				if (abort_scan)
				{	// Kill scan process	
					strcpy(str, "Abort");
					text_status.set(str);
					progressbar.set_value(0);
					tx_mode = IDLE;
					abort_scan = false;
					button_scan.set_style(&style_val);
					button_scan.set_text("SCAN");
				}
				else
				{
					// Next address
					scan_index++;
					scan_progress++;
					repeat_index = 1;
					update_progress();
					start_tx(true);
				}
			}
			else
			{
				transmitter_model.disable();
				tx_mode = IDLE;
				text_status.set("Done");
				progressbar.set_value(0);
				button_scan.set_text("SCAN"); //again ... if finished scan
				tx_view.set_transmitting(false);
			}
		}
	}

	void EncodersView::start_tx(const bool scan)
	{
		(void)scan;
		size_t bitstream_length = 0;

		//repeat_min = view_config.repeat_min();
		uint32_t debruijn_bits;

		if (scan)
		{
			if (tx_mode != SCAN)
			{ //Scanning, and this is first time
				scan_index = 0;

				//First, need to determine the A (Addresses) bit quantity
						//uint8_t word_length;					// Total # of symbols (not counting sync)
						//char word_format[32];					// A for Address, D for Data, S for sync

				view_config.bits_per_packet = 0;
				for (uint8_t c=0; c < view_config.encoder_def->word_length; c++)
					if (view_config.encoder_def->word_format[c] == 'A') //Address bit found
						view_config.bits_per_packet++;

				uint32_t debruijn_total = debruijn_seq.init(view_config.bits_per_packet);
				scan_count = (debruijn_total / view_config.bits_per_packet)  + 1 ; //get total number of packets to tx, plus one, be sure of sending whatever is left from division

				scan_progress = 1;
				repeat_index = 1;
				afsk_repeats = view_config.encoder_def->repeat_min; //Store the min afsk repeats for this scan in a safe place
				tx_mode = SCAN;
				progressbar.set_max(scan_count * afsk_repeats); 

				view_scan.text_debug_encoder.set(view_config.encoder_def->name);
				view_scan.text_debug_word_format.set(view_config.encoder_def->word_format);
				view_scan.text_debug_abits.set(to_string_dec_uint(view_config.bits_per_packet));
				view_scan.text_debug_afsk_repeats.set(to_string_dec_uint(afsk_repeats));
				view_scan.text_debug_de_bruijn_bits.set(to_string_dec_uint(debruijn_total));

			}
			//euquiq: maybe here goes an else, and get the debruijn bits above for the first time and do a debruijn_bits>>=1 to clear the last bit which apparently the debruijn function is placing
			//in extra, the first time it is called. On the else, again just issue the same command as below, loading the debruikn_bits normally.

			debruijn_bits = debruijn_seq.compute(view_config.bits_per_packet); //bits sequence for this step
			view_scan.text_debug.set(to_string_bin(debruijn_bits,view_config.bits_per_packet));
			update_progress();
			view_config.generate_frame(true, debruijn_bits);
		}
		else
		{
			tx_mode = SINGLE;
			repeat_index = 1;
			afsk_repeats = view_config.encoder_def->repeat_min; 
			progressbar.set_max(afsk_repeats);
			update_progress();
			view_config.generate_frame(false, 0);
		}

		bitstream_length = make_bitstream(view_config.frame_fragments);

		transmitter_model.set_sampling_rate(OOK_SAMPLERATE);
		transmitter_model.set_rf_amp(true);
		transmitter_model.set_baseband_bandwidth(1750000);
		transmitter_model.enable();

		baseband::set_ook_data(
			bitstream_length,
			view_config.samples_per_bit(),
			afsk_repeats,
			view_config.pause_symbols());
	}

	EncodersView::EncodersView(
		NavigationView & nav) : nav_{nav}
	{
		baseband::run_image(portapack::spi_flash::image_tag_ook);

		add_children({&tab_view,
					  &view_config,
					  &view_scan,
					  &text_status,
					  &button_scan,
					  &progressbar,
					  &tx_view});

		tx_view.on_edit_frequency = [this, &nav]() {
			auto new_view = nav.push<FrequencyKeypadView>(transmitter_model.tuning_frequency());
			new_view->on_changed = [this](rf::Frequency f) {
				transmitter_model.set_tuning_frequency(f);
			};
		};

		tx_view.on_start = [this]() {
			tx_view.set_transmitting(true);
			start_tx(false);
		};

		tx_view.on_stop = [this]() {
			tx_view.set_transmitting(false);
			if (tx_mode == SCAN)
			{ //Also stop ongoing scan!
				abort_scan = true;
				button_scan.set_text("SCAN");
			}
		};

		button_scan.on_select = [this](Button &) {
			if (tx_mode != SCAN)
			{
				button_scan.set_text("ABORT");
				tx_view.set_transmitting(true);
				start_tx(true);
			}
			else
			{
				abort_scan=true;
				tx_view.set_transmitting(false);
			}
		};
	}

} /* namespace ui */
