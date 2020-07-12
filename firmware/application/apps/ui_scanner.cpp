/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2018 Furrtek
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
 // AD V 1.1 9/7/2020


#include "ui_scanner.hpp"

#include "baseband_api.hpp"
#include "string_format.hpp"
#include "audio.hpp"


using namespace portapack;

namespace ui {



ScanManualView::ScanManualView(
	NavigationView&, Rect parent_rect
) {
	set_parent_rect(parent_rect);
	hidden(true);

	add_children({
		&labels
	});
}

void ScanManualView::focus() {
	
}

void ScanManualView::on_show() {

}



ScanStoredView::ScanStoredView(
	NavigationView&, Rect parent_rect
) {
	set_parent_rect(parent_rect);
	hidden(true);
	add_children({
		&text_cycle,
		&desc_cycle,
	});
}

void ScanStoredView::focus() {
	
}

void ScanStoredView::on_show() {

}

void ScanStoredView::text_set(std::string index_text) {
	text_cycle.set(index_text);
}

void ScanStoredView::desc_set(std::string description) {
	desc_cycle.set(description);
}

ScannerThread::ScannerThread(
	std::vector<rf::Frequency> frequency_list
) : frequency_list_ {  std::move(frequency_list) }
{
	thread = chThdCreateFromHeap(NULL, 1024, NORMALPRIO + 10, ScannerThread::static_fn, this);
}

ScannerThread::~ScannerThread() {
	if( thread ) {
		chThdTerminate(thread);
		chThdWait(thread);
		thread = nullptr;
	}
}

void ScannerThread::set_scanning(const bool v) {
	_scanning = v;
}

bool ScannerThread::is_scanning() {
	return _scanning;
}

msg_t ScannerThread::static_fn(void* arg) {
	auto obj = static_cast<ScannerThread*>(arg);
	obj->run();
	return 0;
}

void ScannerThread::run() {
	RetuneMessage message { };	
	uint32_t frequency_index = 0;
	while( !chThdShouldTerminate() ) {
		if (_scanning) {
			// Retune
			receiver_model.set_tuning_frequency(frequency_list_[frequency_index]);			
			message.range = frequency_index;
			EventDispatcher::send_message(message);
		
			frequency_index++;
			if (frequency_index >= frequency_list_.size())
				frequency_index = 0;
		}
		chThdSleepMilliseconds(50);  
	}
}

void ScannerView::handle_retune(uint32_t i) {
	big_display_freq(frequency_list[i]);	//Show the big Freq
	view_stored.text_set( to_string_dec_uint(i + 1) + "/" + to_string_dec_uint(frequency_list.size()) );
	if (description_list[i] != "#") view_stored.desc_set( description_list[i] );	//If this is a new description: show
}

void ScannerView::focus() {
	//tab_view.focus();
	field_lna.focus();
}

ScannerView::~ScannerView() {
	audio::output::stop();
	receiver_model.disable();
	baseband::shutdown();
}


ScannerView::ScannerView(
	NavigationView& nav,
	const int32_t mod_type
) : nav_ { nav }, 
	title_ { mod_name[mod_type] + " SCANNER" }
{
	add_children({
		&tab_view,
		&view_stored,		
		&view_manual,
		&rssi,
		&labels,
		&field_lna,
		&field_vga,
		&field_rf_amp,
		&field_volume,
		&field_squelch,
		&field_wait,
		&big_display,
		&button_pause
	});
	
	switch (mod_type) {
	case NFM: 
		add_children({&field_bw_NFM });
		break;
	case AM:
		add_children({&field_bw_AM});
		break;
	case FM:  
		add_children({&field_bw_FM});
		break;
	case ANY:
		add_children({&field_bw_ANY	});
		break;
	}

	button_pause.on_select = [this](Button&) {
		if (button_pause.text() == "SCAN") { 		//RESTART SCAN!
			timer = wait * 10;
			button_pause.set_text("PAUSE");
			scan_continue();
			
		} else {
			button_pause.set_text("SCAN");		//PAUSE!
			scan_pause();
		}
	};

	size_t	def_step = mod_step[mod_type];
	std::string scanner_file = "SCANNER_" + mod_name[mod_type];
	big_display_lock();		//Start with green color

	// LEARN FREQUENCIES
	if ( load_freqman_file(scanner_file, database)  ) {
		for(auto& entry : database) {									// READ LINE PER LINE
			if (frequency_list.size() < MAX_DB_ENTRY) {					//We got space!
				if (entry.type == RANGE)  {								//RANGE	
					switch (entry.step) {
					case AM_US:	def_step = 10000;  	break ;
					case AM_EUR:def_step = 9000;  	break ;
					case NFM_1: def_step = 12500;  	break ;
					case NFM_2: def_step = 6250;	break ;	
					case FM_1:	def_step = 100000; 	break ;
					case FM_2:	def_step = 50000; 	break ;
					case N_1:	def_step = 25000;  	break ;
					case N_2:	def_step = 250000; 	break ;
					case AIRBAND:def_step= 8330;  	break ;
					}
					frequency_list.push_back(entry.frequency_a);		//Store starting freq and description
					description_list.push_back("R:" + to_string_dec_uint(entry.frequency_a) + ">" + to_string_dec_uint(entry.frequency_b) + " S:" + to_string_dec_uint(def_step));
					while (frequency_list.size() < MAX_DB_ENTRY && entry.frequency_a <= entry.frequency_b) { //add the rest of the range
						entry.frequency_a+=def_step;
						frequency_list.push_back(entry.frequency_a);
						description_list.push_back("#");				//Token (keep showing the last description)
					}
				} else if ( entry.type == SINGLE)  {
					frequency_list.push_back(entry.frequency_a);
					description_list.push_back("S: " + entry.description);
				}
			}
			else
			{
				break; //No more space: Stop reading the txt file !
			}		
		}

		field_wait.on_change = [this](int32_t v) {	wait = v;	}; 	field_wait.set_value(5);
		field_squelch.on_change = [this](int32_t v) {	squelch = v;	}; 	field_squelch.set_value(30);
		field_volume.set_value((receiver_model.headphone_volume() - audio::headphone::volume_range().max).decibel() + 99);
		field_volume.on_change = [this](int32_t v) { this->on_headphone_volume_changed(v);	};
			
		switch (mod_type) {
				case NFM:
					baseband::run_image(portapack::spi_flash::image_tag_nfm_audio);
					receiver_model.set_modulation(ReceiverModel::Mode::NarrowbandFMAudio);
					field_bw_NFM.set_selected_index(2);
					receiver_model.set_nbfm_configuration(field_bw_NFM.selected_index());
					field_bw_NFM.on_change = [this](size_t n, OptionsField::value_t) { 	receiver_model.set_nbfm_configuration(n); };
					receiver_model.set_sampling_rate(3072000);	receiver_model.set_baseband_bandwidth(1750000);	
					break;
				case AM:
					baseband::run_image(portapack::spi_flash::image_tag_am_audio);
					receiver_model.set_modulation(ReceiverModel::Mode::AMAudio);
					field_bw_AM.set_selected_index(0);
					receiver_model.set_am_configuration(field_bw_AM.selected_index());
					field_bw_AM.on_change = [this](size_t n, OptionsField::value_t) { receiver_model.set_am_configuration(n);	};		
					receiver_model.set_sampling_rate(2000000);receiver_model.set_baseband_bandwidth(2000000); 
					break;
				case FM:
					baseband::run_image(portapack::spi_flash::image_tag_wfm_audio);
					receiver_model.set_modulation(ReceiverModel::Mode::WidebandFMAudio);
					field_bw_FM.set_selected_index(0);
					receiver_model.set_wfm_configuration(field_bw_FM.selected_index());
					field_bw_FM.on_change = [this](size_t n, OptionsField::value_t) {	receiver_model.set_wfm_configuration(n);};
					receiver_model.set_sampling_rate(3072000);	receiver_model.set_baseband_bandwidth(2000000);	
					break;
				case ANY:
					baseband::run_image(portapack::spi_flash::image_tag_am_audio);
					receiver_model.set_modulation(ReceiverModel::Mode::AMAudio);
					field_bw_ANY.set_selected_index(0);
					receiver_model.set_am_configuration(field_bw_ANY.selected_index());
					field_bw_ANY.on_change = [this](size_t n, OptionsField::value_t) { receiver_model.set_am_configuration(n);	};
					receiver_model.set_sampling_rate(1000000);receiver_model.set_baseband_bandwidth(1000000);
					break;
					
				default:	return;
		}
		// COMMON
		receiver_model.enable(); 
		receiver_model.set_squelch_level(0);
		//audio::output::start();
		scan_thread = std::make_unique<ScannerThread>(frequency_list);
	} 
	else 
	{
		view_stored.text_set("NO STORED SCANNER FILE"); // There is no file
	}
}

void ScannerView::big_display_lock() {
	big_display.set_style(&style_green);
}

void ScannerView::big_display_unlock() {
	big_display.set_style(&style_grey);
}

void ScannerView::big_display_freq(rf::Frequency f) {
	big_display.set(f);
}

void ScannerView::on_statistics_update(const ChannelStatistics& statistics) {
	if (statistics.max_db < -squelch || timer >= (wait * 10)) {    // BAD SIGNAL OR WAIT TIME IS UP
		if (!scan_thread->is_scanning() && button_pause.text() != "SCAN") //not scanning and not paused
			scan_continue();	//so, resume scanning!
	}
	else //GOOD SIGNAL
	{
		if (scan_thread->is_scanning())
			scan_pause();
		else
			timer++;
	}
}

void ScannerView::scan_pause() {
	scan_thread->set_scanning(false); // WE STOP SCANNING
	audio::output::start();
	//view_stored.big_display_lock();
	timer = 0;
}

void ScannerView::scan_continue() {
	audio::output::stop();
	//view_stored.big_display_unlock();
	scan_thread->set_scanning(true);   // WE RESCAN
}

void ScannerView::on_headphone_volume_changed(int32_t v) {
	const auto new_volume = volume_t::decibel(v - 99) + audio::headphone::volume_range().max;
	receiver_model.set_headphone_volume(new_volume);
}
	
} /* namespace ui */
