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

#include "ui.hpp"
#include "ui_tabview.hpp"
#include "receiver_model.hpp"

#include "ui_receiver.hpp"
#include "ui_font_fixed_8x16.hpp"
#include "ui_spectrum.hpp"
#include "freqman.hpp"
#include "log_file.hpp"


#define MAX_DB_ENTRY 400

namespace ui {
	
enum modulation_type {
	AM = 0,
	FM,
	NFM,
	ANY
};
 
string const mod_name[4] = {	"AM", "FM", "NFM", "ANY" };
size_t const mod_step[4] = {	9000, 100000, 12500, 10000 };

class ScanManualView : public View {
public:
	ScanManualView(NavigationView& nav, Rect parent_rect);
	
	void focus() override;
	void on_show() override;

private:
	Labels labels {
		{ { 1 * 8, 9 * 8 }, "MANUAL SCAN TAB...", Color::light_grey() }
	};
};




class ScanStoredView : public View {
public:
	ScanStoredView(NavigationView& nav, Rect parent_rect);
	
	void focus() override;
	void on_show() override;
	void text_set(std::string index_text);
	void desc_set(std::string description);

private:

	Text text_cycle {
		{ 0, 2 * 16, 240, 16 },  
	};
	
	Text desc_cycle {
		{0, 3 * 16, 240, 16 },	   
	};



};



class ScannerThread {
public:
	ScannerThread(std::vector<rf::Frequency> frequency_list);
	~ScannerThread();
	
	void set_scanning(const bool v);
	bool is_scanning();

	ScannerThread(const ScannerThread&) = delete;
	ScannerThread(ScannerThread&&) = delete;
	ScannerThread& operator=(const ScannerThread&) = delete;
	ScannerThread& operator=(ScannerThread&&) = delete;

private:
	std::vector<rf::Frequency> frequency_list_ { };
	Thread* thread { nullptr };
	
	bool _scanning { true };
	static msg_t static_fn(void* arg);
	void run();
};


class ScannerView : public View {
public:
	ScannerView(
		NavigationView& nav, 
		int32_t mod_type
	);
	~ScannerView();
	
	void focus() override;

	void big_display_lock();
	void big_display_unlock();
	void big_display_freq(rf::Frequency f);

	const Style style_grey {		// scanning
		.font = font::fixed_8x16,
		.background = Color::black(),
		.foreground = Color::grey(),
	};
	
	const Style style_green {		//Found signal
		.font = font::fixed_8x16,
		.background = Color::black(),
		.foreground = Color::green(),
	};

std::string title() const override { return  title_; }

//void set_parent_rect(const Rect new_parent_rect) override;

private:
	NavigationView& nav_;
	Rect view_rect = { 0, 10 * 16, 240, 100 };
	
	ScanStoredView view_stored { nav_, view_rect };
	ScanManualView view_manual { nav_, view_rect };
	
	TabView tab_view {
		{ "Stored", Color::white(), &view_stored },
		{ "Manual", Color::white(), &view_manual },
	};

	const std::string title_;

	void scan_pause();
	void scan_continue();

	void on_statistics_update(const ChannelStatistics& statistics);
	void on_headphone_volume_changed(int32_t v);
	void handle_retune(uint32_t i);
	
	std::vector<rf::Frequency> frequency_list{ };
	std::vector<string> description_list { };

	int32_t squelch { 0 };
	uint32_t timer { 0 };
	uint32_t wait { 0 };
	freqman_db database { };
	
	Labels labels {
		{ { 0 * 8, 2 * 16 }, "LNA:   VGA:   AMP:  VOL:", Color::light_grey() },
		{ { 0 * 8, 3* 16 }, "BW:    SQUELCH:  /99 WAIT:", Color::light_grey() },
	};
	
	LNAGainField field_lna {
		{ 4 * 8, 2 * 16 }
	};

	VGAGainField field_vga {
		{ 11 * 8, 2 * 16 }
	};
	
	RFAmpField field_rf_amp {
		{ 18 * 8, 2 * 16 }
	};
	
	NumberField field_volume {
		{ 24 * 8, 2 * 16 },
		2,
		{ 0, 99 },
		1,
		' ',
	};
	
	OptionsField field_bw_NFM {
		{ 3 * 8, 3 * 16 },
		4,
		{
			{ "8k5", 0 },
			{ "11k", 0 },
			{ "16k", 0 },
			
		}
	};

	OptionsField field_bw_AM {
		{ 3 * 8, 3 * 16 },
		4,
		{
			{ "DSB ", 0 },
			{ "USB ", 0 },
			{ "LSB ", 0 },
		}
	};	

	OptionsField field_bw_FM {
		{ 3 * 8, 3 * 16 },
		4,
		{
	/*		{ "3k", 0 },
			{ "6k", 0 },
			{ "8k5", 0 },
			{ "11k", 0 },  */
			{ "16k", 0 },
		}
	};

	OptionsField field_bw_ANY {
		{ 3 * 8, 3 * 16 },
		4,
		{
			{ "DSB ", 0 },
			{ "USB ", 0 },
			{ "LSB ", 0 },
		}
	};		

	NumberField field_squelch {
		{ 15 * 8, 3 * 16 },
		2,
		{ 0, 99 },
		1,
		' ',
	};

	NumberField field_wait {
		{ 26 * 8, 3 * 16 },
		2,
		{ 0, 99 },
		1,
		' ',
	};

	RSSI rssi {
		{ 0 * 16, 5 * 16, 15 * 16, 8 },
	}; 

	BigFrequency big_display {		//Show frequency in glamour
		{ 4, 6 * 16, 28 * 8, 52 },
		0
	};

	Button button_pause {
		{ 72, 264, 96, 24 },
		"PAUSE"
	};
	
	std::unique_ptr<ScannerThread> scan_thread { };
	
	MessageHandlerRegistration message_handler_retune {
		Message::ID::Retune,
		[this](const Message* const p) {
			const auto message = *reinterpret_cast<const RetuneMessage*>(p);
			this->handle_retune(message.range);
		}
	};
	
	MessageHandlerRegistration message_handler_stats {
		Message::ID::ChannelStatistics,
		[this](const Message* const p) {
			this->on_statistics_update(static_cast<const ChannelStatisticsMessage*>(p)->statistics);
		}
	};
};
												 

} /* namespace ui */