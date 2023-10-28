/*
 * Copyright 2023 Birte Kristina Friesel
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#ifndef __SYSTEM_H__
#define __SYSTEM_H__

class System {
	private:
		uint8_t btn_debounce;

		void idle(void);
		void sleep(void);

		uint8_t warmwhite;
		uint8_t red;
		uint8_t green;
		uint8_t blue;
		uint8_t mode_changed;

		// 1 Ah @ 3.7V = 13 kJ
		// adjusted for battery wear&tear and increased cut-off voltage: 5 kJ
		int16_t const energy_full = 5000;
		int16_t energy_j;

	public:

		uint8_t anim_step_fine;
		uint8_t anim_step_coarse;
		void initialize(void);

		void loop(void);
		uint8_t is_charging;
		uint8_t tick_18s;

		// Battery: 1000 mAh @ 3.7V -> ~13 kJ / 13000 J
		// 15 mA * 3.7 V * 18s -> 1 J
		enum BCMode : uint8_t {
			OFF = 0,   //  0 mA
			WARMWHITE, //  32 mA @ 4 V   : 2 J per 18 s
			SLOWRGB,   // ~45 mA @ 4 V   : 3
			SUN,       // 134 mA @ 4 V   : 9
			RED,       //  45 mA @ 4 V   : 3
			GREEN,     //  29 mA @ 4 V   : 2
			BLUE,      //  30 mA @ 4 V   : 2
			YELLOW,    //  74 mA @ 4 V   : 5
			MAGENTA,   //  76 mA @ 4 V   : 5
			CYAN,      //  59 mA @ 4 V   : 4
			FASTRGB,   // ~45 mA @ 4 V   : 3
			SLOWRGB2,  // ~17 mA @ 4 V   : 1
			FASTRGB2,  // ~17 mA @ 4 V   : 1
			MODE_ENUM_MAX,
		};

		BCMode mode;

		void next_mode(void);

		void debounce_done(void);

		void debounce_start(void);

		inline void setEnergyFull(void) { energy_j = energy_full; }

		System() : btn_debounce(0), mode_changed(0), energy_j(energy_full), is_charging(0), tick_18s(0), mode(OFF) {}
};

extern System blinkencat;

#endif
