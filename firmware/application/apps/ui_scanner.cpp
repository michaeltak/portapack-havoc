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

ScanModeView::ScanModeView (
	NavigationView& nav, Rect parent_rect
): nav_ { nav }
{
	set_parent_rect(parent_rect);
	hidden(true);

	add_children({
		&labels,
		&field_mode
	});

}

ScanManualView::ScanManualView (
	NavigationView& nav, Rect parent_rect
): nav_ { nav }
{
	set_parent_rect(parent_rect);
	hidden(true);

	add_children({
		&labels,
		&button_manual_start,
		&button_manual_stop,
		&step_mode,
		&button_manual_execute
	});

	button_manual_start.on_select = [this, &nav](Button& button) {
		auto new_view = nav.push<FrequencyKeypadView>(frequency_range.min);
		new_view->on_changed = [this, &button](rf::Frequency f) {
			frequency_range.min = f;
			button_manual_start.set_text(to_string_short_freq(f));
		};
	};
	
	button_manual_stop.on_select = [this, &nav](Button& button) {
		auto new_view = nav.push<FrequencyKeypadView>(frequency_range.max);
		new_view->on_changed = [this, &button](rf::Frequency f) {
			frequency_range.max = f;
			button_manual_stop.set_text(to_string_short_freq(f));
		};
	};
}

/* void ScanManualView::focus() {
	
}

void ScanManualView::on_show() {

} */

ScanStoredView::ScanStoredView(
	NavigationView&, Rect parent_rect
) {
	set_parent_rect(parent_rect);
	hidden(true);
	add_children({
		&labels,
		&text_cycle,
		&text_max,
		&desc_cycle,
	});
}

/* void ScanStoredView::focus() {
	
}

void ScanStoredView::on_show() {

} */

void ScanStoredView::text_set(std::string index_text) {
	text_cycle.set(index_text);
}

void ScanStoredView::max_set(std::string max_text) {
	text_max.set(max_text);
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
	stop();
}

void ScannerThread::stop() {
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

void ScannerThread::set_userpause(const bool v) {
	_userpause = v;
}

bool ScannerThread::is_userpause() {
	return _userpause;
}

msg_t ScannerThread::static_fn(void* arg) {
	auto obj = static_cast<ScannerThread*>(arg);
	obj->run();
	return 0;
}

void ScannerThread::run() {
	if (frequency_list_.size())	{				//IF THERE IS A FREQUENCY LIST ...	
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
			chThdSleepMilliseconds(100);  //Was 50, searching for more precise scanning
		}
	}
}

void ScannerView::handle_retune(uint32_t i) {
	big_display_freq(frequency_list[i]);	//Show the big Freq
	view_stored.text_set( to_string_dec_uint(i + 1,3));
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

void ScannerView::show_max() {		//show total number of freqs to scan inside stored tab
	if (frequency_list.size() == MAX_DB_ENTRY)
		view_stored.max_set( to_string_dec_uint(MAX_DB_ENTRY) + " (DB MAX!)");
	else
		view_stored.max_set( to_string_dec_uint(frequency_list.size()));
}

ScannerView::ScannerView(
	NavigationView& nav,
	const int32_t mod_type
) : nav_ { nav }, 
	title_ { mod_name[mod_type] + " SCANNER" },
	mod_type_ { mod_type }
{
	add_children({
		&tab_view,
		&view_mode,
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
		&button_pause,
		&button_audio_app
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

	def_step = mod_step[mod_type];
	std::string scanner_file = "SCANNER_" + mod_name[mod_type];
	big_display.set_style(&style_green);	//Start with green color

	button_pause.on_select = [this](Button&) {
		if (scan_thread->is_userpause()) { 
			timer = wait * 10;						//Unlock timer pause on_statistics_update
			button_pause.set_text("PAUSE");		//resume scanning (show button for pause)	
			scan_thread->set_userpause(false);
			//scan_resume();
		} else {
			scan_pause();
			scan_thread->set_userpause(true);
			button_pause.set_text("RESUME");		//PAUSED, show resume	
		}
	};

	button_audio_app.on_select = [this](Button&) {
		if (scan_thread->is_scanning())
		 	scan_thread->set_scanning(false);
		scan_thread->stop();
		nav_.pop();
		nav_.push<AnalogAudioView>();
	};

	//PRE-CONFIGURATION:
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

	view_manual.button_manual_execute.on_select = [this](Button&) {
		if (!view_manual.frequency_range.min || !view_manual.frequency_range.max)
			nav_.display_modal("Error", "Both START and STOP freqs\nneed a value");

		if (view_manual.frequency_range.min > view_manual.frequency_range.max)
			nav_.display_modal("Error", "STOP freq\nis lower than START");

		if (scan_thread->is_scanning()) 
			scan_thread->set_scanning(false);

		//STOP SCANNER THREAD
		//audio::output::stop();
		scan_thread->stop();

		frequency_list.clear(); //This shouldn't be necessary since it was moved inside scanner at beginning
		description_list.clear();

		def_step = view_manual.step_mode.selected_index_value();		//Use def_step from manual selector

		description_list.push_back(
			"M:" + to_string_short_freq(view_manual.frequency_range.min) + " > "
	 		+ to_string_short_freq(view_manual.frequency_range.max) + " S:" 
	 		+ to_string_short_freq(def_step)
		);

		rf::Frequency frequency = view_manual.frequency_range.min;
		while (frequency_list.size() < MAX_DB_ENTRY &&  frequency <= view_manual.frequency_range.max) { //add manual range				
			frequency_list.push_back(frequency);
			description_list.push_back("#");				//Token (keep showing the last description)
			frequency+=def_step;
		}

		show_max();

		//RESTART SCANNER THREAD
		receiver_model.enable(); 
		receiver_model.set_squelch_level(0);
		scan_thread = std::make_unique<ScannerThread>(frequency_list);
	};

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
					description_list.push_back("R:" + to_string_short_freq(entry.frequency_a) + " > " + to_string_short_freq(entry.frequency_b) + " S:" + to_string_short_freq(def_step));
					while (frequency_list.size() < MAX_DB_ENTRY && entry.frequency_a <= entry.frequency_b) { //add the rest of the range
						entry.frequency_a+=def_step;
						frequency_list.push_back(entry.frequency_a);
						description_list.push_back("#");				//Token (keep showing the last description)
					}
				} else if ( entry.type == SINGLE)  {
					frequency_list.push_back(entry.frequency_a);
					description_list.push_back("S: " + entry.description);
				}
				show_max();
			}
			else
			{
				break; //No more space: Stop reading the txt file !
			}		
		}
		tab_view.set_selected(1);	//Stored freqs, put focus on STORED scan tab
		view_manual.step_mode.set_by_value(def_step); //Impose the last def_step into the manual step selector
	} 
	else 
	{
		view_stored.desc_set(" NO SCANNER .TXT FILE ..." );
		tab_view.set_selected(2);	//Since no stored freqs, put focus on MANUAL scan tab
	}
	// COMMON
	receiver_model.enable(); 
	receiver_model.set_squelch_level(0);
	scan_thread = std::make_unique<ScannerThread>(frequency_list);
}

void ScannerView::big_display_freq(rf::Frequency f) {
	big_display.set(f);
}

void ScannerView::on_statistics_update(const ChannelStatistics& statistics) {
	if (!scan_thread->is_userpause()) 
	{
		if (timer >= (wait * 10) ) 
		{
			timer=0;
			scan_resume();
		} 
		else if (!timer) 
		{
			if (statistics.max_db > -squelch) {  //There is something on the air...
				scan_pause();
				timer++;
			} 
		} 
		else 
		{
				timer++;
		}
	}
}

void ScannerView::scan_pause() {
	if (scan_thread->is_scanning()) {
		scan_thread->set_scanning(false); // WE STOP SCANNING
		audio::output::start();
	}
}

void ScannerView::scan_resume() {
	if (!scan_thread->is_scanning()) {
		audio::output::stop();
		scan_thread->set_scanning(true);   // WE RESCAN
	}
}

void ScannerView::on_headphone_volume_changed(int32_t v) {
	const auto new_volume = volume_t::decibel(v - 99) + audio::headphone::volume_range().max;
	receiver_model.set_headphone_volume(new_volume);
}
	
} /* namespace ui */