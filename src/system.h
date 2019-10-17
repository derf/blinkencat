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

	public:

		uint8_t anim_step_fine;
		uint8_t anim_step_coarse;
		void initialize(void);

		void loop(void);

		uint8_t is_charging;

		enum BCMode : uint8_t {
			OFF = 0,
			WARMWHITE,
			SLOWRGB,
			SUN,
			RED,
			GREEN,
			BLUE,
			YELLOW,
			MAGENTA,
			CYAN,
			FASTRGB,
			SLOWRGB2,
			FASTRGB2,
			MODE_ENUM_MAX,
		};

		BCMode mode;

		void next_mode(void);

		void debounce_done(void);

		void debounce_start(void);

		System() { btn_debounce = 0; mode = OFF; is_charging = 0; mode_changed = 0; };
};

extern System blinkencat;

#endif
