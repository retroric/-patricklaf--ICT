// ICT
// © 2020-2021 Patrick Lafarguette
//
// 03/03/2021   2.3.0
//
// 02/03/2021   Fix a bug in keyboard.
//              Add checkbox UI.
//              ROM test option.
//
// 28/02/2021   Fix a bug in address write.
//
// 25/02/2021   ROM support. 27128, 27256 families.
//              ROM support. Dump to file.
//              Add ini file for settings.
//
// 22/02/2021   Support more than 1 clock signal for logic test.
//
// 19/02/2021   ROM support. 2764 family.
//
// 08/02/2021   2.2.0
//
// 01/02/2021	High and low buses.
//              SRAM support. MCM6810.
//
// 30/01/2021	Read and write data bus functions.
//				Write address functions for 16 or 32 bits.
//
// 27/01/2021	Increase bus width to 32 bits.
//              SRAM support. KM684000.
//
// 25/01/2021	2.1.0
//				DRAM support. 4464 family.
//				RAM search case insensitive.
//				Add file not found error.
//				Add support for SdFat 2.x.x.
//				Add aliases support for RAM.
//
// 04/12/2020	2.0.0
//				Minor fixes.
//
// 19/11/2020	RAM support. ZX81-32K extension.
//
// 18/11/2020	DRAM support. 4116 family.
//
// 17/11/2020	Optimization (buttons).
//
// 16/11/2020	Keyboard support for alphanumeric input.
//
// 15/11/2020	SRAM support. 61256 family.
//
// 09/11/2020	Increase ZIF pins to 40.
//				SRAM support. 6116 family.
//				Add /OE support for SRAM.
//
// 02/11/2020	1.4.1
//				Minor modifications.
//
// 03/10/2020	1.4.0
//
// 21/09/2020	Minor modifications.
//
// 18/09/2020	Move RAM definition to file.
//
// 18/09/2020	Code cleanup.
//
// 14/09/2020	1.3.0
//				Remove DIO2 library.
//				Add function ic_package_idle.
//
// 12/09/2020	DRAM support. 4416 family.
//				Improved performances with ic_pin_xx functions.
//
// 11/09/2020	Code optimization.
//				Improved performances.
//				DIO2 library.
//
// 10/09/2020	Code rework to accommodate more RAMs.
//				SRAM support. 2114 family.
//				DRAM support. 44256 family.
//
// 09/09/2020	Increase ZIF pins to 20.
//				RAM content is now correctly displayed.
//
// 07/09/2020	Dynamic creation of package.
//				RAM content is now correctly displayed.
//
// 04/09/2020	Test function for identified IC.
//
// 01/09/2020	1.2.0
// 				Redo function for logic test.
//				Screen capture for documentation.
//
// 31/08/2020 	1.1.0
//				UI optimization.
//				Code optimization.
//				DRAM support. 4164 and 41256 family.
//
// 13/08/2020	1.0.0
//				Initial version.

#include <MCUFRIEND_kbv.h>
#include <SdFat.h>
#include <TouchScreen.h>

#include "IC.h"
#include "Checkbox.h"
#include "Keyboard.h"
#include "Memory.h"
#include "Stack.h"

// Set one of the language to 1.
// French UI, currently not viable as screen only supports
// unaccented US-ASCII character set.

#define ENGLISH 1
#define FRENCH 0

#include "Language.h"

// TFT calibration
// See TouchScreen_Calibr_native example in MCUFRIEND_kbv library
const int XP = 8, XM = A2, YP = A3, YM = 9;
const int TS_LEFT = 908, TS_RT = 122, TS_TOP = 85, TS_BOT = 905;

MCUFRIEND_kbv tft;
// XP (LCD_RS), XM (LCD_D0) resistance is 345 Ω
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 345);

// SD
#if SD_FAT_VERSION < 20000
// Modify SdFatConfig.h
//
//#define ENABLE_SOFTWARE_SPI_CLASS 1
SdFatSoftSpi<12, 11, 13> fat;
#else
// Modify SdFatConfig.h
//
// #define SPI_DRIVER_SELECT 2
const uint8_t SD_CS_PIN = 10;
const uint8_t SOFT_MISO_PIN = 12;
const uint8_t SOFT_MOSI_PIN = 11;
const uint8_t SOFT_SCK_PIN  = 13;

SoftSpiDriver<SOFT_MISO_PIN, SOFT_MOSI_PIN, SOFT_SCK_PIN> spi;

#define SDCONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(0), &spi)

SdFat fat;
#endif

Stack<IC*> ics;
Stack<String*> lines;

#define TEST 0 // 1 to enable test cases, 0 to disable
#define TIME 1 // 1 to enable time, 0 to disable
#define DEBUG 1 // 1 to enable serial debug messages, 0 to disable
#define MISMATCH 1 // 1 to enable serial mismatch messages, 0 to disable
#define CAPTURE 1 // 1 to enable screen captures, 0 to disable

#if DEBUG
#define Debug(...) Serial.print(__VA_ARGS__)
#define Debugln(...) Serial.println(__VA_ARGS__)
#else
#define Debug(...)
#define Debugln(...)
#endif

typedef enum states {
	state_startup,
	state_media,
	state_menu,
	state_identify_logic,
	state_test_logic,
	state_test_ram,
	state_test_rom,
	state_package_dip, // Only logic
	state_keyboard,
	state_option,
	state_identified,
	state_tested,
} states_t;

typedef enum actions {
	action_idle,
	action_identify_logic,
	action_test_logic,
	action_test_ram,
	action_test_rom,
} actions_t;

typedef enum buttons {
	button_identify_logic,
	button_test_logic,
	button_test_ram,
	button_test_rom,
	// Identify logic
	button_dip14 = 0,
	button_dip16,
	// Test
	button_continue = 0,
	// Test
	button_redo = 0,
	// Keyboard
	button_enter = 0,
	button_clear,
	button_del,
	// Identified
	button_test = 0,
	button_previous,
	button_next,
	button_escape,
	button_count,
} buttons_t;

typedef enum checkboxes {
	checkbox_option_blank,
	checkbox_option_serial,
	checkbox_option_sdcard,
	checkbox_count,
} checkboxes_t;

typedef enum flags {
	flag_rom_blank,
	flag_rom_serial,
	flag_rom_sdcard,
	flag_count,
} flags_t;

typedef struct Worker {
	uint8_t state;
	uint8_t action;
	bool interrupted;
	// TFT
	int x;
	int y;
	// IC
	Package package;
	String code;
	// Identify
	uint8_t index;
	// Test
	bool found;
	bool ok;
	unsigned int success;
	unsigned int failure;
	// UI
	uint16_t color;
	unsigned int indicator;
	uint8_t ram;
} Worker;

Worker worker;

// UI size
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

// UI areas
#define AREA_HEADER 0
#define AREA_CONTENT 80
#define AREA_FOOTER 180

// UI buttons
#define BUTTON_X 33
#define BUTTON_Y 90
#define BUTTON_W 52
#define BUTTON_H 48
#define BUTTON_SPACING_X 12
#define BUTTON_SPACING_Y 12
#define BUTTON_TEXTSIZE 2

// UI checkboxes
#define CHECKBOX_X 4
#define CHECKBOX_Y ( AREA_CONTENT - 14)
#define CHECKBOX_W 32
#define CHECKBOX_H 32
#define CHECKBOX_SPACING_X 4
#define CHECKBOX_SPACING_Y 4
#define CHECKBOX_TEXTSIZE 2

// UI input
#define INPUT_HEIGHT 54
#define INPUT_X 8
#define INPUT_Y 12
#define INPUT_SIZE 4
#define INPUT_LENGTH 12

// UI RAM
#define RAM_TOP 140
#define RAM_BOX 40
#define RAM_SIZE (RAM_BOX - 2 * 2)
#define RAM_HALF (RAM_SIZE / 2)
#define RAM_BOTTOM (RAM_TOP + RAM_SIZE)

// Color scheme
#define COLOR_BACKGROUND TFT_NAVY
#define COLOR_TEXT TFT_YELLOW

#define COLOR_OK TFT_GREEN
#define COLOR_KO TFT_ORANGE
#define COLOR_GOOD TFT_GREEN
#define COLOR_BAD TFT_RED
#define COLOR_MIXED TFT_ORANGE
#define COLOR_SKIP TFT_DARKGREY

#define COLOR_LABEL TFT_WHITE
#define COLOR_ENTER TFT_BLUE
#define COLOR_DELETE TFT_GREEN
#define COLOR_ESCAPE TFT_RED
#define COLOR_KEY TFT_BLACK
#define COLOR_CODE TFT_WHITE
#define COLOR_DESCRIPTION TFT_CYAN

Keyboard keyboard(&tft, INPUT_HEIGHT + 1);

Checkbox checkboxes[checkbox_count];

Adafruit_GFX_Button buttons[button_count];

// Filenames
#define INIFILE "ict.ini"
#define TMPFILE "ict.tmp"
#define LOGICFILE "logic.ict"
#define MEMORYFILE "memory.ict"

void hexa(uint32_t value, uint8_t count) {
	for (uint8_t index = count; index > 0;) {
		Serial.print(value >> (--index * 4) & 0x0F, HEX);
	}
}

/////////
// Bus //
/////////

const uint8_t BYTES[] = {
		(uint8_t)(1 << 0),
		(uint8_t)(1 << 1),
		(uint8_t)(1 << 2),
		(uint8_t)(1 << 3),
		(uint8_t)(1 << 4),
		(uint8_t)(1 << 5),
		(uint8_t)(1 << 6),
		(uint8_t)(1 << 7)
};

const uint16_t WORDS[] = {
		(uint16_t)(1 << 0),
		(uint16_t)(1 << 1),
		(uint16_t)(1 << 2),
		(uint16_t)(1 << 3),
		(uint16_t)(1 << 4),
		(uint16_t)(1 << 5),
		(uint16_t)(1 << 6),
		(uint16_t)(1 << 7),
		(uint16_t)(1 << 8),
		(uint16_t)(1 << 9),
		(uint16_t)(1 << 10),
		(uint16_t)(1 << 11),
		(uint16_t)(1 << 12),
		(uint16_t)(1 << 13),
		(uint16_t)(1 << 14),
		(uint16_t)(1 << 15)
};

void ic_bus_write_address_word(Bus& bus) {
	uint16_t lsb = bus.address;
	for (uint8_t index = 0; index < bus.width; ++index) {
		ic_pin_write(worker.package.pins[bus.pins[index]], lsb & WORDS[index] ? HIGH : LOW);
	}
}

void ic_bus_write_address_dword(Bus& bus) {
	uint16_t lsb = bus.address;
	uint16_t msb = bus.address >> 16;
	uint8_t index = 0;
	for (; index < 16; ++index) {
		ic_pin_write(worker.package.pins[bus.pins[index]], lsb & WORDS[index] ? HIGH : LOW);
	}
	for (; index < bus.width; ++index) {
		ic_pin_write(worker.package.pins[bus.pins[index]], msb & WORDS[index] ? HIGH : LOW);
	}
}

void ic_bus_write_data(Bus& bus) {
	for (uint8_t index = 0; index < bus.width; ++index) {
		ic_pin_write(worker.package.pins[bus.pins[index]], bus.data & BYTES[index]);
	}
}

void ic_bus_read_data(Bus& bus) {
	bus.data = 0;
	for (uint8_t index = 0; index < bus.width; ++index) {
		if (ic_pin_read(worker.package.pins[bus.pins[index]])) {
			bus.data |= BYTES[index];
		}
	}
}

void ic_bus_output(Bus& bus) {
	for (uint8_t index = 0; index < bus.width; ++index) {
		ic_pin_mode(worker.package.pins[bus.pins[index]], OUTPUT);
	}
}

void ic_bus_input(Bus& bus) {
	for (uint8_t index = 0; index < bus.width; ++index) {
		ic_pin_mode(worker.package.pins[bus.pins[index]], INPUT);
	}
}

void ic_bus_data(Bus& bus, uint8_t bit, const bool alternate) {
	uint8_t mask = alternate ? HIGH : LOW;
	bus.data = 0;
	for (uint8_t index = 0; index < bus.width; ++index) {
		if (bit) {
			bus.data |= BYTES[index];
		}
		bit ^= mask;
	}
	Debug(F("fill 0b"));
	for (uint8_t index = bus.width - 1 ; index != 0xFF; --index) {
		Debug(bus.data & BYTES[index] ? F("1") : F("0"));
	}
	Debugln();
}

////////////
// Memory //
////////////

bool ic_memory_signal(uint8_t signal) {
	return memory.signals[signal] > -1;
}

/////////
// RAM //
/////////

void ic_ram_loop() {
	// UI
	worker.ram = 0;
	worker.color = TFT_WHITE;
	for (uint8_t index = 0; index < 4; ++index) {
		ui_draw_ram();
	}
	worker.ram = 0;
#if TIME
	// Timer
	unsigned long start = millis();
#endif
	// Setup
	for (uint8_t index = 0; index < memory.bus[BUS_Q].width; ++index) {
		ic_pin_mode(worker.package.pins[memory.bus[BUS_Q].pins[index]], INPUT);
	}
	// VCC, GND
	if ((memory.signals[SIGNAL_VBB] == 0) && (memory.signals[SIGNAL_VDD] == 7)) {
		// Power rails to -5 and +12 V
		Debugln("Enable -5 and +12 V");
		digitalWrite(A7, HIGH);
	}
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_GND]], LOW);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_VCC]], HIGH);
	// At package creation all pins are output low
	// LOW bus is preset, so only set HIGH bus high
	for (uint8_t index = 0; index < memory.bus[BUS_HIGH].width; ++index) {
		ic_pin_write(worker.package.pins[memory.bus[BUS_HIGH].pins[index]], HIGH);
	}
	// Idle
	memory.idle();
	// Test
	worker.indicator = 0;
	for (uint8_t loop = 0; loop < 4; ++loop) {
		// 0 00 0 false true
		// 1 01 0 false true
		// 2 10 1 true false
		// 3 11 1 true false
		bool alternate = !(loop >> 1);
		// 0 00 0 LOW
		// 1 01 1 HIGH
		// 2 10 0 LOW
		// 3 11 1 HIGH
		ic_bus_data(memory.bus[BUS_D], loop & 0x01, alternate);
		memory.fill(alternate);
	}
	// Shutdown
	if ((memory.signals[SIGNAL_VBB] == 0) && (memory.signals[SIGNAL_VDD] == 7)) {
		Debugln("Disable -5 and +12 V");
		// Power rails to TTL
		digitalWrite(A7, LOW);
	}
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_VCC]], LOW);
#if TIME
	unsigned long stop = millis();
	Serial.print(F(TIME_ELAPSED));
	Serial.println((stop - start) / 1000.0);
#endif
}

//////////
// DRAM //
//////////

void ic_dram_idle() {
	// Idle
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_WE]], HIGH);
	if (ic_memory_signal(SIGNAL_OE)) {
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_OE]], HIGH);
	}
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_RAS]], HIGH);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CAS]], HIGH);
	for (uint8_t index = 0; index < memory.bus[BUS_RAS].width; ++index) {
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_RAS]], LOW);
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_RAS]], HIGH);
	}
}

void ic_dram_write() {
	// Row
	memory.write_address(memory.bus[BUS_RAS]);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_RAS]], LOW);
	// WE
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_WE]], LOW);
	// D
	ic_bus_write_data(memory.bus[BUS_D]);
	// Column
	memory.write_address(memory.bus[BUS_CAS]);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CAS]], LOW);
	// Idle
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_WE]], HIGH);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_RAS]], HIGH);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CAS]], HIGH);
}

void ic_dram_read() {
	// Row
	memory.write_address(memory.bus[BUS_RAS]);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_RAS]], LOW);
	// Column
	memory.write_address(memory.bus[BUS_CAS]);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CAS]], LOW);
	// OE
	bool oe = ic_memory_signal(SIGNAL_OE);
	if (oe) {
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_OE]], LOW);
	}
	// Q
	ic_bus_read_data(memory.bus[BUS_Q]);
	// Idle
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_RAS]], HIGH);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CAS]], HIGH);
	// OE
	if (oe) {
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_OE]], HIGH);
	}
}

void ic_dram_fill(const bool alternate = false) {
	uint8_t mask = (alternate && (memory.bus[BUS_D].width > 1)) ? memory.bus[BUS_D].high : 0;
	worker.color = COLOR_GOOD;
	for (memory.bus[BUS_CAS].address = 0; memory.bus[BUS_CAS].address < memory.bus[BUS_CAS].high; ++memory.bus[BUS_CAS].address) {
		worker.success = 0;
		worker.failure = 0;
		ic_bus_output(memory.bus[BUS_D]);
		// Inner loop is RAS to keep refreshing rows while writing a full column
		for (memory.bus[BUS_RAS].address = 0; memory.bus[BUS_RAS].address < memory.bus[BUS_RAS].high; ++memory.bus[BUS_RAS].address) {
			ic_dram_write();
			memory.bus[BUS_D].data ^= mask;
		}
		ic_bus_input(memory.bus[BUS_Q]);
		// Inner loop is RAS to keep refreshing rows while reading back a full column
		for (memory.bus[BUS_RAS].address = 0; memory.bus[BUS_RAS].address < memory.bus[BUS_RAS].high; ++memory.bus[BUS_RAS].address) {
			ic_dram_read();
			if (memory.bus[BUS_D].data != memory.bus[BUS_Q].data) {
#if MISMATCH
				Debug("RAS ");
				Debug(memory.bus[BUS_RAS].address, BIN);
				Debug(", CAS ");
				Debug(memory.bus[BUS_CAS].address, BIN);
				Debug(", D ");
				Debug(memory.bus[BUS_D].data, BIN);
				Debug(", Q ");
				Debug(memory.bus[BUS_Q].data, BIN);
				Debug(", delta ");
				Debugln(memory.bus[BUS_D].data ^ memory.bus[BUS_Q].data, BIN);
#endif
				worker.failure++;
				worker.color = COLOR_BAD;
			} else {
				worker.success++;
			}
			memory.bus[BUS_D].data ^= mask;
		}
		ui_draw_indicator((worker.success ? (worker.failure ? COLOR_MIXED : COLOR_GOOD) : COLOR_BAD));
	}
	ui_draw_ram();
}

//////////
// SRAM //
//////////

void ic_sram_idle() {
	// Idle
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_WE]], HIGH);
	if (ic_memory_signal(SIGNAL_OE)) {
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_OE]], HIGH);
	}
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CS]], HIGH);
}

void ic_sram_write() {
	// Address
	memory.write_address(memory.bus[BUS_ADDRESS]);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CS]], LOW);
	// WE
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_WE]], LOW);
	// D
	ic_bus_write_data(memory.bus[BUS_D]);
	// Idle
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_WE]], HIGH);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CS]], HIGH);
}

void ic_sram_read() {
	// Address
	memory.write_address(memory.bus[BUS_ADDRESS]);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CS]], LOW);
	// OE
	bool oe = ic_memory_signal(SIGNAL_OE);
	if (oe) {
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_OE]], LOW);
	}
	// Q
	ic_bus_read_data(memory.bus[BUS_Q]);
	// Idle
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CS]], HIGH);
	// OE
	if (oe) {
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_OE]], HIGH);
	}
}

void ic_sram_fill(const bool alternate = false) {
	uint8_t mask = (alternate && (memory.bus[BUS_D].width > 1)) ? memory.bus[BUS_D].high : 0;
	worker.color = COLOR_GOOD;
	// Write full address space
	ic_bus_output(memory.bus[BUS_D]);
	for (memory.bus[BUS_ADDRESS].address = 0; memory.bus[BUS_ADDRESS].address < memory.bus[BUS_ADDRESS].high; ++memory.bus[BUS_ADDRESS].address) {
		ic_sram_write();
		memory.bus[BUS_D].data ^= mask;
	}
	// Read full address space
	ic_bus_input(memory.bus[BUS_Q]);
	for (memory.bus[BUS_ADDRESS].address = 0; memory.bus[BUS_ADDRESS].address < memory.bus[BUS_ADDRESS].high; ++memory.bus[BUS_ADDRESS].address) {
		worker.success = 0;
		worker.failure = 0;
		ic_sram_read();
		if (memory.bus[BUS_D].data != memory.bus[BUS_Q].data) {
#if MISMATCH
			Debug("A ");
			Debug(memory.bus[BUS_ADDRESS].address, BIN);
			Debug(", D ");
			Debug(memory.bus[BUS_D].data, BIN);
			Debug(", Q ");
			Debug(memory.bus[BUS_Q].data, BIN);
			Debug(", delta ");
			Debugln(memory.bus[BUS_D].data ^ memory.bus[BUS_Q].data, BIN);
#endif
			worker.failure++;
			worker.color = COLOR_BAD;
		} else {
			worker.success++;
		}
		memory.bus[BUS_D].data ^= mask;
		ui_draw_indicator((worker.success ? (worker.failure ? COLOR_MIXED : COLOR_GOOD) : COLOR_BAD));
	}
	ui_draw_ram();
}

/////////
// ROM //
/////////

//String ic_rom_filename() {
//	uint32_t index = ini_read_key("ROM", "index");
//	ini_write_key("ROM", "index", ++index);
//	String filename ="rom-";
//	filename += index;
//	filename += ".bin";
//	return filename;
//}

void ic_rom_filename(String& filename) {
	char buffer[5];
	uint32_t index = ini_read_key("ROM", "index");
	ini_write_key("ROM", "index", ++index);
	filename ="ict-";
	snprintf(buffer, 5, "%04ld", index);
//	filename += index;
	filename += buffer;
	filename += ".bin";
}

void ic_rom_loop() {
	// Flags
	// TODO needs rework
	bool flags[flag_count];
	for (uint8_t index = 0; index < flag_count; ++index) {
		flags[index] = checkboxes[index].Checked();
	}
	// File
	String filename;
	File file;
	if (flags[flag_rom_sdcard]) {
		ic_rom_filename(filename);
		file = fat.open(filename.c_str(), O_CREAT | O_RDWR);
		tft.setTextColor(TFT_WHITE);
		tft.print(F("Dump to: "));
		tft.println(filename);
	}
	// UI
//	worker.ram = 0;
	uint16_t color = COLOR_OK;
	worker.color = COLOR_GOOD;
	ui_draw_rom();
	// Miscellaneous
	uint8_t count = (memory.bus[BUS_ADDRESS].width) / 4 + (memory.bus[BUS_ADDRESS].width % 4 ? 1 : 0);
	uint8_t data[16];
#if TIME
	// Timer
	unsigned long start = millis();
#endif
	// Setup
	for (uint8_t index = 0; index < memory.bus[BUS_Q].width; ++index) {
		ic_pin_mode(worker.package.pins[memory.bus[BUS_Q].pins[index]], INPUT);
	}
	// VCC, GND
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_GND]], LOW);
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_VCC]], HIGH);
	// At package creation all pins are output low
	// LOW bus is preset, so only set HIGH bus high
	for (uint8_t index = 0; index < memory.bus[BUS_HIGH].width; ++index) {
		ic_pin_write(worker.package.pins[memory.bus[BUS_HIGH].pins[index]], HIGH);
	}
	// Idle
	memory.idle();
	// Test
	worker.indicator = 0;
	uint8_t index = 0;
	bool blank = true;
	// Read full address space
	ic_bus_input(memory.bus[BUS_Q]);
	for (memory.bus[BUS_ADDRESS].address = 0; memory.bus[BUS_ADDRESS].address < memory.bus[BUS_ADDRESS].high; ++memory.bus[BUS_ADDRESS].address) {
		ic_sram_read();
		data[index] = memory.bus[BUS_Q].data;
		// Serial
		if (flags[flag_rom_serial]) {
			if (index == 0) {
				hexa(memory.bus[BUS_ADDRESS].address, count);
			}
			if (index == 8) {
				Serial.print(" ");
			}
			Serial.print(" ");
			hexa(memory.bus[BUS_Q].data, 2);
		}
		// Blank
		if (blank && (memory.bus[BUS_Q].data != memory.bus[BUS_Q].high)) {
			blank = false;
			color = COLOR_KO;
			worker.color = COLOR_BAD;
			ui_draw_rom();
			if (flags[flag_rom_blank]) {
				break;
			}
		}
		++index %= 16;
		if (index == 0) {
			// Serial
			if (flags[flag_rom_serial]) {
				Serial.print(" ");
				for (uint8_t index = 0; index < 16; ++index) {
					if (index == 8) {
						Serial.print("  ");
					}
					if (iscntrl(char(data[index]))) {
						Serial.print('.');
					} else {
						Debug(char(data[index]));
					}
				}
				Serial.println();
			}
			// File
			if (flags[flag_rom_sdcard]) {
				file.write(data, 16);
			}
		}
//		ui_draw_indicator(blank ? COLOR_OK : COLOR_KO);
		ui_draw_indicator(color);
	}
	// Shutdown
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_VCC]], LOW);
#if TIME
	unsigned long stop = millis();
	Serial.print(F(TIME_ELAPSED));
	Serial.println((stop - start) / 1000.0);
#endif
	// File
	if (flags[flag_rom_sdcard]) {
		file.close();
	}
}

void ic_rom_idle() {
	// Idle
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_WE]], HIGH);
	if (ic_memory_signal(SIGNAL_OE)) {
		ic_pin_write(worker.package.pins[memory.signals[SIGNAL_OE]], HIGH);
	}
	ic_pin_write(worker.package.pins[memory.signals[SIGNAL_CS]], HIGH);
}

//////////////////
// Touch screen //
//////////////////

#define TS_MIN 10
#define TS_MAX 1000
#define TS_DELAY 10

bool ts_touched() {
	TSPoint point = ts.getPoint();
	pinMode(YP, OUTPUT);
	pinMode(XM, OUTPUT);
	digitalWrite(YP, HIGH);
	digitalWrite(XM, HIGH);
	if (point.z != 0) {
		// TODO adapt code to your display
		worker.x = map(point.y, TS_LEFT, TS_RT, TFT_WIDTH, 0);
		worker.y = map(point.x, TS_TOP, TS_BOT, 0, TFT_HEIGHT);
		if ((point.z > TS_MIN) && (point.z < TS_MAX)) {
			return true;
		}
	}
	return false;
}

void ts_wait() {
	bool loop = true;
	while (loop) {
		TSPoint point = ts.getPoint();
		if (point.z > TS_MIN && point.z < TS_MAX) {
			loop = false;
		} else {
			delay(TS_DELAY);
		}
	}
	pinMode(YP, OUTPUT);
	pinMode(XM, OUTPUT);
	digitalWrite(YP, HIGH);
	digitalWrite(XM, HIGH);
}

/////////
// TFT //
/////////

void tft_init() {
	tft.reset();
	uint16_t identifier = tft.readID();
	tft.begin(identifier);
	Debug(F(TFT_INITIALIZED));
	Debugln(identifier, HEX);
	// TODO adapt code to your display
	tft.setRotation(1);
}

/////////////
// SD card //
/////////////

bool sd_begin() {
#if SD_FAT_VERSION < 20000
	return fat.begin(10);
#else
	return fat.begin(SDCONFIG);
#endif
}

#if CAPTURE
void sd_screen_capture() {
	static unsigned int index = 0;
	String filename("image.");
	filename += index++;
	filename += ".data";
	Serial.print("Save ");
	Serial.print(filename);
	uint16_t* pixels = (uint16_t*)malloc(sizeof(uint16_t) * TFT_WIDTH);
	if (pixels) {
		File file;
		if (file.open(filename.c_str(), O_CREAT | O_RDWR)) {
			for (unsigned int y = 0; y < TFT_HEIGHT; ++y) {
				tft.readGRAM(0, y, pixels, TFT_WIDTH, 1);
				file.write(pixels, sizeof(uint16_t) * TFT_WIDTH);
			}
		}
		file.close();
	} else {
		Serial.println(" failed");
	}
	free(pixels);
	Serial.println(" done");
}
#endif

uint32_t  ini_read_key(const char* section, const char* key) {
	uint32_t value = 0;
	File file = fat.open(INIFILE);
	if (file) {
		bool in = false;
		// Patterns
		String patterns[2];
		patterns[0] = "[";
		patterns[0].concat(section);
		patterns[0].concat("]");
		patterns[1].concat(key);
		patterns[1].concat("=");
		while (file.available()) {
			String line = file.readStringUntil('\n');
			line.trim();
			// Key
			if (in && line.startsWith(patterns[1])) {
				value = line.substring(line.indexOf("=") + 1).toInt();
				break;
			} else {
				// Section
				if (line.startsWith("[")) {
					in = line.equalsIgnoreCase(patterns[0]);
				}
			}
		}
		file.close();
	} else {
		Debug(INIFILE);
		Debugln(F(" not found"));
		worker.state = state_media;
	}
	return value;
}

void ini_write_key(const char* section, const char* key, uint32_t value) {
	File input = fat.open(INIFILE);
	File output = fat.open(TMPFILE, O_CREAT | O_RDWR);
	if (input && output) {
		bool in = false;
		// Patterns
		String patterns[2];
		patterns[0] = "[";
		patterns[0].concat(section);
		patterns[0].concat("]");
		patterns[1].concat(key);
		patterns[1].concat("=");
		while (input.available()) {
			String line = input.readStringUntil('\n');
			line.trim();
			// Key
			if (in && line.startsWith(patterns[1])) {
				output.print(key);
				output.print("=");
				output.println(value);
			} else {
				// Section
				if (line.startsWith("[")) {
					in = line.equalsIgnoreCase(patterns[0]);
				}
				output.println(line);
			}
		}
		input.close();
		fat.remove(INIFILE);
		output.rename(INIFILE);
		output.close();
	} else {
		Debug(INIFILE);
		Debugln(F(" not found"));
		worker.state = state_media;
	}
}

////////////////////
// User interface //
////////////////////

void ui_draw_center(const __FlashStringHelper* text, int16_t y) {
	int16_t x;
	uint16_t width, height;
	tft.getTextBounds(text, 0, y, &x, &y, &width, &height);
	tft.setCursor((TFT_WIDTH - width) / 2, y);
	tft.println(text);
}

void ui_draw_wrap(const String& string, const uint8_t size) {
	unsigned int start = 0;
	unsigned int index = start;
	tft.setTextSize(size);
	while (index < string.length()) {
		if (string[index] == ' ') {
			if ((tft.getCursorX() + (index - start - 1) * 6 * size) > TFT_WIDTH) {
				tft.println();
			}
			tft.print(string.substring(start, index++));
			if (tft.getCursorX() + 6 * size < TFT_WIDTH) {
				tft.setCursor(tft.getCursorX() + 6 * size, tft.getCursorY());
			}
			start = index;
		}
		++index;
	}
	if (index != start) {
		if ((tft.getCursorX() + (index - start) * 6 * size) > TFT_WIDTH) {
			tft.println();
		}
		tft.println(string.substring(start, index));
	}
}

void ui_draw_error(const __FlashStringHelper* text) {
	tft.setTextSize(2);
	tft.setTextColor(TFT_RED);
	ui_draw_center(text, AREA_CONTENT + 32);
}

void ui_draw_indicator(const uint16_t color) {
	tft.fillRect(worker.indicator * 20 + 2, 222, 16, 16, color);
	++worker.indicator %= 16;
	tft.fillRect(worker.indicator * 20 + 2, 222, 16, 16, COLOR_BACKGROUND);
}

void ui_draw_percentage() {
	int width = map(worker.success, 0, worker.success + worker.failure, 0, TFT_WIDTH - 2 * 2);
	tft.fillRect(2, 202, width, 16, COLOR_GOOD);
	tft.fillRect(2 + width, 202, TFT_WIDTH - 2 * 2 - width, 16, COLOR_BAD);
}

void ui_draw_ram() {
	unsigned int x = (worker.ram - 2) * RAM_BOX + (TFT_WIDTH / 2);
	tft.fillRect(x, RAM_TOP, RAM_SIZE, RAM_SIZE, worker.color);
	switch (worker.ram++) {
	case 0: // 0 1
		tft.drawFastHLine(x, RAM_BOTTOM - 4, RAM_HALF, TFT_BLACK);
		tft.drawFastHLine(x + RAM_HALF, RAM_TOP + 4, RAM_HALF, TFT_BLACK);
		tft.drawFastVLine(x + RAM_HALF, RAM_TOP + 4, RAM_SIZE - 8, TFT_BLACK);
		break;
	case 1: // 1 0
		tft.drawFastHLine(x, RAM_TOP + 4, RAM_HALF, TFT_BLACK);
		tft.drawFastHLine(x + RAM_HALF, RAM_BOTTOM - 4, RAM_HALF, TFT_BLACK);
		tft.drawFastVLine(x + RAM_HALF, RAM_TOP + 4, RAM_SIZE - 8, TFT_BLACK);
		break;
	case 2: // 0
		tft.drawFastHLine(x, RAM_BOTTOM - 4, RAM_SIZE, TFT_BLACK);
		break;
	case 3: // 1
		tft.drawFastHLine(x, RAM_TOP + 4, RAM_SIZE, TFT_BLACK);
		break;
	}
}

void ui_draw_rom() {
	unsigned int x = (TFT_WIDTH - RAM_BOX)/ 2;
	tft.fillRect(x, RAM_TOP, RAM_SIZE, RAM_SIZE, worker.color);
	tft.setTextSize(3);
	tft.setTextColor(TFT_BLACK);
	ui_draw_center(F("FF"), RAM_TOP + (RAM_BOX - 3 * 8) / 2);
}

void ui_draw_header(bool erase) {
	if (erase) {
		tft.fillScreen(COLOR_BACKGROUND);
	}
	tft.setTextColor(COLOR_TEXT);
	tft.setTextSize(5);
	ui_draw_center(F("ICT"), 2);
}

void ui_draw_button(char* label) {
	// ENTER, TEST or REDO, always button 0 and same shape and color
	buttons[0].initButton(&tft, BUTTON_X + (BUTTON_W + BUTTON_SPACING_X) * 0.5,
			BUTTON_Y + 2 * (BUTTON_H + BUTTON_SPACING_Y), 2 * BUTTON_W + BUTTON_SPACING_X, BUTTON_H,
			COLOR_ENTER, COLOR_ENTER, COLOR_LABEL, label, BUTTON_TEXTSIZE);
	buttons[0].drawButton();
}

void ui_draw_escape() {
	buttons[button_escape].initButton(&tft, BUTTON_X + (BUTTON_W + BUTTON_SPACING_X) * 4,
			BUTTON_Y + 2 * (BUTTON_H + BUTTON_SPACING_Y), BUTTON_W, BUTTON_H,
			COLOR_ESCAPE, COLOR_ESCAPE, COLOR_LABEL, (char*)"ESC", BUTTON_TEXTSIZE);
	buttons[button_escape].drawButton();
}

void ui_draw_menu(const char* items[], const unsigned int count) {
//	unsigned int y = AREA_CONTENT - 4;
	unsigned int y = AREA_CONTENT - 14;
	ui_draw_header(true);
#if 0
	for (uint8_t index = 0; index < count; ++index) {
		buttons[index].initButton(&tft,
		BUTTON_X,
		BUTTON_Y + index * (BUTTON_H + BUTTON_SPACING_Y),
		BUTTON_W, BUTTON_H, COLOR_KEY, COLOR_KEY, COLOR_LABEL,
				(char*) String(index + 1).c_str(), BUTTON_TEXTSIZE);
		buttons[index].drawButton();
		tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
		tft.setTextSize(3);
		tft.setCursor(2 * BUTTON_X, y);
		y += BUTTON_H + BUTTON_SPACING_Y;
		tft.print(items[index]);
	}
#else

	// UI buttons
	#define MENU_X 33
	#define MENU_Y 80
	#define MENU_W 52
	#define MENU_H 36
	#define MENU_SPACING_X 12
	#define MENU_SPACING_Y 6
	#define MENU_TEXTSIZE 2


	for (uint8_t index = 0; index < count; ++index) {
		buttons[index].initButton(&tft,
		MENU_X,
		MENU_Y + index * (MENU_H + MENU_SPACING_Y),
		MENU_W, MENU_H, COLOR_KEY, COLOR_KEY, COLOR_LABEL,
				(char*) String(index + 1).c_str(), MENU_TEXTSIZE);
		buttons[index].drawButton();
		tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
		tft.setTextSize(3);
		tft.setCursor(2 * MENU_X, y);
		y += MENU_H + MENU_SPACING_Y;
		tft.print(items[index]);
	}
#endif
}

void ui_draw_checkbox(const char* items[], const unsigned int count) {
	unsigned int y = CHECKBOX_Y;
	for (uint8_t index = 0; index < count; ++index) {
		checkboxes[index].Initialize(&tft, CHECKBOX_X, y);
		checkboxes[index].Draw();
		tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
		tft.setTextSize(CHECKBOX_TEXTSIZE);
		tft.setCursor(CHECKBOX_X + CHECKBOX_W + CHECKBOX_SPACING_X, y);
		y += CHECKBOX_H + CHECKBOX_SPACING_Y;
		tft.print(items[index]);
	}
}

void ui_draw_screen() {
	switch (worker.state) {
	case state_startup:
		ui_draw_header(true);
		// Version
		tft.setTextSize(2);
		ui_draw_center(F(ICT_VERSION), tft.getCursorY());
		ui_draw_center(F(ICT_DATE), tft.getCursorY());
		// Author
		tft.setTextColor(TFT_WHITE);
		ui_draw_center(F("Patrick Lafarguette"), AREA_CONTENT + 32);
		ui_draw_escape();
		break;
	case state_media:
		ui_draw_header(true);
		// Error
		ui_draw_error(F(INSERT_SD_CARD));
		ui_draw_escape();
		break;
	case state_menu: {
		worker.action = action_idle;
#if 0
		const char* items[] = { IDENTIFY_LOGIC, TEST_LOGIC, TEST_RAM };
		ui_draw_menu(items, 3);
#else
		const char* items[] = { IDENTIFY_LOGIC, TEST_LOGIC, TEST_RAM, TEST_ROM };
		ui_draw_menu(items, 4);
#endif
	}
		break;
	case state_identify_logic: {
		const char* items[] = { DIP14_PACKAGE, DIP16_PACKAGE };
		ui_draw_menu(items, 2);
		ui_draw_escape();
	}
		break;
	case state_package_dip:
		worker.action = action_identify_logic;
		worker.index = -1;
		ics.clear();
		ui_draw_header(true);
		// Action
		tft.setTextSize(2);
		ui_draw_center(F(IDENTIFY_LOGIC__), tft.getCursorY());
		ic_logic_identify();
		break;
	case state_keyboard:
		tft.fillScreen(TFT_NAVY);
		tft.drawRect(0, 0, TFT_WIDTH, INPUT_HEIGHT, TFT_YELLOW);
		tft.drawRect(1, 1, TFT_WIDTH - 2, INPUT_HEIGHT - 2, TFT_YELLOW);
		keyboard.draw();
		ui_draw_button((char*)"ENTER");
		buttons[button_clear].initButton(&tft, BUTTON_X + (BUTTON_W + BUTTON_SPACING_X) * 2.5,
				BUTTON_Y + 2 * (BUTTON_H + BUTTON_SPACING_Y), 2 * BUTTON_W + BUTTON_SPACING_X, BUTTON_H,
				COLOR_KEY, COLOR_KEY, COLOR_LABEL, (char*)"CLR", BUTTON_TEXTSIZE);
		buttons[button_clear].drawButton();
		buttons[button_del].initButton(&tft, BUTTON_X + (BUTTON_W + BUTTON_SPACING_X) * 4,
				BUTTON_Y + 2 * (BUTTON_H + BUTTON_SPACING_Y), BUTTON_W, BUTTON_H,
				COLOR_DELETE, COLOR_DELETE, COLOR_LABEL, (char*)"DEL", BUTTON_TEXTSIZE);
		buttons[button_del].drawButton();
		break;
	case state_option: {
		const char* items[] = { "break on blank fail", "serial dump", "SD card dump"};
		ui_draw_header(true);
		// Action
		tft.setTextSize(2);
		ui_draw_center(F(TEST_ROM_), tft.getCursorY());
		ui_draw_checkbox(items, 3);
		ui_draw_button((char*)"CONTINUE");
		ui_draw_escape();
	}
		break;
	case state_test_logic:
		ics.clear();
		lines.clear();
		ui_draw_header(true);
		// Action
		tft.setTextSize(2);
		ui_draw_center(F(TEST_LOGIC_), tft.getCursorY());
		ic_logic_test();
		break;
	case state_test_ram:
		ics.clear();
		lines.clear();
		ui_draw_header(true);
		// Action
		tft.setTextSize(2);
		ui_draw_center(F(TEST_RAM_), tft.getCursorY());
		ic_memory_test();
		break;
	case state_test_rom:
		ics.clear();
		lines.clear();
		ui_draw_header(true);
		// Action
		tft.setTextSize(2);
		ui_draw_center(F(TEST_ROM_), tft.getCursorY());
#if 0
		ic_test_rom();
#else
		ic_memory_test();
#endif
		break;
	case state_identified:
		ui_clear_footer();
		if (ics.count()) {
			ui_draw_button((char*)"TEST");
			if (ics.count() > 1) {
				// Add navigator
				buttons[button_previous].initButton(&tft, BUTTON_X + 2 * (BUTTON_W + BUTTON_SPACING_X),
						BUTTON_Y + 2 * (BUTTON_H + BUTTON_SPACING_Y), BUTTON_W, BUTTON_H,
						COLOR_KEY, COLOR_KEY, COLOR_LABEL, (char*)"<", BUTTON_TEXTSIZE);
				buttons[button_previous].drawButton();
				buttons[button_next].initButton(&tft, BUTTON_X + 3 * (BUTTON_W + BUTTON_SPACING_X),
						BUTTON_Y + 2 * (BUTTON_H + BUTTON_SPACING_Y),  BUTTON_W, BUTTON_H,
						COLOR_KEY, COLOR_KEY, COLOR_LABEL, (char*)">", BUTTON_TEXTSIZE);
				buttons[button_next].drawButton();
			}

		} else {
			ui_draw_content();
		}
		ui_draw_escape();
		break;
	case state_tested:
		ui_clear_footer();
		if (worker.found) {
			switch (worker.action) {
				case action_test_logic: {
					unsigned int total = worker.success + worker.failure;
					double percent = (100.0 * worker.success) / total;
					tft.print(total);
					tft.print(F(CYCLES));
					tft.print(percent);
					tft.println(F(PASSED));
					tft.setTextSize(2);
					tft.setTextColor(worker.success ? (worker.failure ? COLOR_MIXED : COLOR_GOOD) : COLOR_BAD);
					tft.println(worker.success ? (worker.failure ? F(UNRELIABLE) : F(GOOD)) : F(BAD));
				}
					break;
				case action_test_ram:
				case action_test_rom:
					break;
			}
			ui_draw_button((char*)"REDO");
		} else {
			ui_draw_content();
		}
		ui_draw_escape();
		break;
	}
}

void ui_clear_content() {
	tft.fillRect(0, AREA_CONTENT, TFT_WIDTH, AREA_FOOTER - AREA_CONTENT, COLOR_BACKGROUND);
	tft.setCursor(0, AREA_CONTENT);
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(2);
}

void ui_clear_footer() {
	tft.fillRect(0, AREA_FOOTER, TFT_WIDTH, TFT_HEIGHT - AREA_FOOTER, COLOR_BACKGROUND);
}

void ui_draw_content() {
	switch (worker.action) {
	case action_identify_logic:
		ui_clear_content();
		switch (ics.count()) {
		case 0: // No match found
			ui_draw_error(F(NO_MATCH_FOUND));
			return;
		case 1:
			tft.println(F(MATCH_FOUND));
			break;
		default:
			tft.print(F(MATCH_FOUND));
			tft.print(" ");
			tft.print(worker.index + 1);
			tft.print("/");
			tft.println(ics.count());
			break;
		}
		break;
	case action_test_logic:
	case action_test_ram:
	case action_test_rom:
		if (worker.found) {
		} else {
			ui_draw_error(F(NO_MATCH_FOUND));
			return;
		}
		break;
	}
	tft.setCursor(0, tft.getCursorY() + 4);
	tft.setTextColor(COLOR_CODE);
	tft.setTextSize(3);
	tft.println(ics[worker.index]->code);
	tft.setCursor(0, tft.getCursorY() + 4);
	tft.setTextColor(COLOR_DESCRIPTION);
	ui_draw_wrap(ics[worker.index]->description, 2);
}

void ui_draw_ic(int count, bool wide) {
	tft.fillScreen(COLOR_BACKGROUND);
	int width = (count / 2) * 12 + 6;
	int height = wide ? 80 : 40;
	int left = (tft.width() - width) / 2;
	int top = (tft.height() - height) / 2;
	int bottom = top + height;
	// Body
	tft.fillRect(left, top, width, height, TFT_BLACK);
	tft.fillCircleHelper(left + width, tft.height() / 2, 6, 3, 0, COLOR_BACKGROUND);
	// Pins
	int x = left + 6;
	for (int index = 0; index < (count / 2); ++index) {
		tft.fillRect(x, top - 6, 6, 6, TFT_DARKGREY);
		tft.fillRect(x, bottom, 6, 6, TFT_DARKGREY);
		x += 12;
	}
}

void ui_draw_pin(int count, bool wide, int pin, uint16_t color) {
	int width = (count / 2) * 12 + 6;
	int height = wide ? 80 : 40;
	int left = (tft.width() - width) / 2;
	int top = (tft.height() - height) / 2;
	int bottom = top + height;
	// Pins
	int x = left + 6;
	if (pin < count / 2) {
		x += ((count / 2) - pin % (count / 2) - 1) * 12;
	} else {
		x += (pin - (count / 2)) * 12;
	}
	int y = pin < (count / 2) ? top - 6 : bottom;
	tft.fillRect(x, y, 6, 6, color);
}

////////////////////////
// Integrated circuit //
////////////////////////

void ic_package_create() {
	for (uint8_t index = 0, delta = 0; index < worker.package.count; ++index) {
		if (index == worker.package.count >> 1) {
			delta = ZIF_COUNT - worker.package.count;
		}
		uint8_t port = digitalPinToPort(ZIF[index + delta]);
		worker.package.pins[index].bit = digitalPinToBitMask(ZIF[index + delta]);
		worker.package.pins[index].mode = portModeRegister(port);
		worker.package.pins[index].output = portOutputRegister(port);
		worker.package.pins[index].input = portInputRegister(port);
	}
}

void ic_package_output() {
	for (uint8_t index = 0; index < worker.package.count; ++index) {
		ic_pin_mode(worker.package.pins[index], OUTPUT);
		ic_pin_write(worker.package.pins[index], LOW);
	}
}

void ic_package_idle() {
	for (uint8_t index = 0; index < worker.package.count; ++index) {
		ic_pin_mode(worker.package.pins[index], INPUT);
	}
}

#if TEST
void ic_package_dump() {
	Serial.print("DIP");
	Serial.print(worker.package.width);
	Serial.print(" ");
	for (uint8_t index = 0; index < worker.package.width; ++index) {
		if (index) {
			Serial.print(", ");
		}
		// TODO adapt to Pin
		// Serial.print(worker.package.pins[index]);
	}
	Serial.println();
}

void ic_package_test() {
	worker.package.width = 20;
	ic_package_create();
	// 30, 32, 34, 36, 38, 40, 42, 44, 50, 48, 49, 47, 45, 43, 41, 39, 37, 35, 33, 31
	ic_package_dump();
	worker.package.width = 18;
	ic_package_create();
	// 30, 32, 34, 36, 38, 40, 42, 44, 50, 47, 45, 43, 41, 39, 37, 35, 33, 31
	ic_package_dump();
	worker.package.width = 16;
	ic_package_create();
	// 30, 32, 34, 36, 38, 40, 42, 44, 45, 43, 41, 39, 37, 35, 33, 31
	// 30, 32, 34, 36, 38, 40, 42, 44, 45, 43, 41, 39, 37, 35, 33, 31
	ic_package_dump();
	worker.package.width = 14;
	ic_package_create();
	// 30, 32, 34, 36, 38, 40, 42, 43, 41, 39, 37, 35, 33, 31
	// 30, 32, 34, 36, 38, 40, 42, 43, 41, 39, 37, 35, 33, 31
	ic_package_dump();
}
#endif

bool ic_logic_test(String line) {
	bool result = true;
	bool clock = false;
//	int8_t clock = -1;
	Debugln(line);
	// VCC, GND and input
	for (uint8_t index = 0; index < worker.package.count; ++index) {
		switch (line[index]) {
		case 'V':
			ic_pin_mode(worker.package.pins[index], OUTPUT);
			ic_pin_write(worker.package.pins[index], HIGH);
			break;
		case 'G':
			ic_pin_mode(worker.package.pins[index], OUTPUT);
			ic_pin_write(worker.package.pins[index], LOW);
			break;
		case 'L':
			ic_pin_write(worker.package.pins[index], LOW);
			ic_pin_mode(worker.package.pins[index], INPUT_PULLUP);
			break;
		case 'H':
			ic_pin_write(worker.package.pins[index], LOW);
			ic_pin_mode(worker.package.pins[index], INPUT_PULLUP);
			break;
		}
	}
	delay(5);
	// Write
	for (uint8_t index = 0; index < worker.package.count; ++index) {
		switch (line[index]) {
		case 'X':
		case '0':
			ic_pin_mode(worker.package.pins[index], OUTPUT);
			ic_pin_write(worker.package.pins[index], LOW);
			break;
		case '1':
			ic_pin_mode(worker.package.pins[index], OUTPUT);
			ic_pin_write(worker.package.pins[index], HIGH);
			break;
		case 'C':
//			clock = index;
			clock = true;
			ic_pin_mode(worker.package.pins[index], OUTPUT);
			ic_pin_write(worker.package.pins[index], LOW);
			break;
		}
	}
	// Clock
#if 0
	if (clock != -1) {
		ic_pin_mode(worker.package.pins[clock], INPUT_PULLUP);
		delay(10);
		ic_pin_mode(worker.package.pins[clock], OUTPUT);
		ic_pin_write(worker.package.pins[clock], LOW);
	}
#else
	if (clock) {
		for (uint8_t index = 0; index < worker.package.count; ++index) {
			if (line[index] == 'C') {
				ic_pin_mode(worker.package.pins[index], INPUT_PULLUP);
			}
		}
		delay(10);
		for (uint8_t index = 0; index < worker.package.count; ++index) {
			if (line[index] == 'C') {
				ic_pin_mode(worker.package.pins[index], OUTPUT);
				ic_pin_write(worker.package.pins[index], LOW);
			}
		}
	}
#endif
	delay(5);
	// Read
	for (uint8_t index = 0; index < worker.package.count; ++index) {
		switch (line[index]) {
		case 'H':
			if (!ic_pin_read(worker.package.pins[index])) {
				result = false;
				Debug(F(_LOW));
			} else {
				Debug(F(SPACE));
			}
			break;
		case 'L':
			if (ic_pin_read(worker.package.pins[index])) {
				result = false;
				Debug(F(_HIGH));
			} else {
				Debug(F(SPACE));
			}
			break;
		default:
			Debug(F(SPACE));
		}
	}
	Debugln();
	return result;
}

void ic_logic_identify() {
	String line;
	IC ic;
	File file = fat.open(LOGICFILE);
	if (file) {
		worker.indicator= 0;
		while (file.available()) {
			file.readStringUntil('$');
			if (!file.available()) {
				// Avoid timeout at the end of file
				break;
			}
			ic.code = file.readStringUntil('\n');
			ic.code.trim();
			ic.description = file.readStringUntil('\n');
			ic.count = file.readStringUntil('\n').toInt();
			if (worker.package.count == ic.count) {
				worker.ok = true;
				while (file.peek() != '$') {
					line = file.readStringUntil('\n');
					line.trim();
					if (ic_logic_test(line) == false) {
						worker.ok = false;
						break;
					}
					delay(50);
				}
				if (worker.ok) {
					tft.fillRect(ics.count() * 20 + 2, 202, 16, 16, COLOR_OK);
					ui_draw_indicator(COLOR_OK);
					ics.push(new IC(ic));
					worker.index++;
					ui_draw_content();
				} else {
					ui_draw_indicator(COLOR_KO);
				}

			} else {
				ui_draw_indicator(COLOR_SKIP);
			}
		}
		file.close();
		worker.state = state_identified;
	} else {
		Debug(LOGICFILE);
		Debugln(F(" not found"));
		worker.state = state_media;
	}
	ui_draw_screen();
}

void ic_logic_test() {
	String line;
	IC ic;
	File file = fat.open(LOGICFILE);
	if (file) {
		worker.indicator = 0;
		worker.found = false;
		while (file.available()) {
			file.readStringUntil('$');
			if (!file.available()) {
				// Avoid timeout at the end of file
				break;
			}
			ic.code = file.readStringUntil('\n');
			ic.code.trim();
			ic.description = file.readStringUntil('\n');
			ic.count = file.readStringUntil('\n').toInt();
			if (worker.code.equalsIgnoreCase(ic.code)) {
				worker.found = true;
				worker.index = 0;
				worker.success = 0;
				worker.failure = 0;
				worker.package.count = ic.count;
				ic_package_create();
				ics.push(new IC(ic));
				ui_draw_content();
				while (file.peek() != '$') {
					line = file.readStringUntil('\n');
					line.trim();
					lines.push(new String(line));
				}
				break;
			} else {
				ui_draw_indicator(COLOR_SKIP);
			}
		}
		file.close();
		if (worker.found) {
			bool loop = true;
			while (loop) {
				worker.ok = true;
				for (uint8_t index = 0; index < lines.count(); ++index) {
					if (ic_logic_test(*lines[index]) == false) {
						worker.ok = false;
					}
					delay(50);
				}
				if (worker.ok) {
					worker.success++;
					ui_draw_indicator(COLOR_GOOD);
				} else {
					worker.failure++;
					ui_draw_indicator(COLOR_BAD);
				}
				ui_draw_percentage();
				loop = !ts_touched();
			}
		}
		worker.state = state_tested;
	} else {
		Debug(LOGICFILE);
		Debugln(F(" not found"));
		worker.state = state_media;
	}
	ui_draw_screen();
}

bool ic_parse_bus(String& tag, String& data) {
	const char* LABELS[] = { "A", "R", "C", "D", "Q", "L", "H" };
	const uint8_t BUS[] = { BUS_ADDRESS, BUS_RAS, BUS_CAS, BUS_D, BUS_Q, BUS_LOW, BUS_HIGH };
	for (uint8_t index = 0; index < sizeof(BUS) / sizeof(uint8_t); ++index) {
		if (tag.equals(LABELS[index])) {
			int start = 0, stop = 0;
			uint8_t pin = 0;
			while ((stop = data.indexOf(' ', start)) > -1) {
				pin = data.substring(start, stop++).toInt();
#if 1
				Debug(", ");
				Debug(pin);
#endif
				memory.bus[BUS[index]].pins[memory.bus[BUS[index]].width++] = pin - 1;
				start = stop;
			}
			pin = data.substring(start).toInt();
#if 1
			Debug(", ");
			Debug(pin);
#endif
			memory.bus[BUS[index]].pins[memory.bus[BUS[index]].width++] = pin - 1;
			memory.bus[BUS[index]].high = (uint32_t)1 << memory.bus[BUS[index]].width;
#if 1
			Debug(", width ");
			Debug(memory.bus[BUS[index]].width);
			Debug(", high 0x");
			Debug(memory.bus[BUS[index]].high, HEX);
#endif
			return true;
		}
	}
	return false;
}

bool ic_parse_signal(String& tag, String& data) {
	const char* LABELS[] = { "CS", "RAS", "CAS", "WE", "OE", "GND", "VCC", "VBB", "VDD" };
	const uint8_t SIGNALS[] = { SIGNAL_CS, SIGNAL_RAS, SIGNAL_CAS, SIGNAL_WE, SIGNAL_OE, SIGNAL_GND, SIGNAL_VCC, SIGNAL_VBB, SIGNAL_VDD };
	for (uint8_t index = 0; index < sizeof(SIGNALS) / sizeof(uint8_t); ++index) {
		if (tag.equals(LABELS[index])) {
			uint8_t pin = data.toInt();
#if 1
			Debug(F(" "));
			Debug(pin);
#endif
			memory.signals[SIGNALS[index]] = pin - 1;
			return true;
		}
	}
	return false;
}

void ic_parse_memory(IC& ic) {
	memory.count = ic.count;
	// Reset
	memory.bus[BUS_RAS].width = 0;
	memory.bus[BUS_CAS].width = 0;
	memory.bus[BUS_D].width = 0;
	memory.bus[BUS_Q].width = 0;
	memory.bus[BUS_LOW].width = 0;
	memory.bus[BUS_HIGH].width = 0;
	for (uint8_t index = 0; index < SIGNAL_COUNT; ++index) {
		memory.signals[index] = -1;
	}
	if (lines[0]->equals("DRAM")) {
		ic.type = TYPE_DRAM;
		Debugln("DRAM");
	}
	if (lines[0]->equals("SRAM")) {
		ic.type = TYPE_SRAM;
		Debugln("SRAM");
	}
	if (lines[0]->equals("ROM")) {
		ic.type = TYPE_ROM;
		Debugln("ROM");
	}
	// Parse
	for (uint8_t index = 1; index < lines.count(); ++index) {
		int stop = lines[index]->indexOf(' ');
		String* line = lines[index];
		String tag = line->substring(0, stop);
		String data = line->substring(stop + 1);
		Debug("tag [");
		Debug(tag);
		if (ic_parse_signal(tag, data) || ic_parse_bus(tag, data)) {
			Debugln("]");
		} else {
			Serial.println("] unknown");
		}
	}
	// Decrease data bus
	memory.bus[BUS_D].high -= 1;
	memory.bus[BUS_Q].high -= 1;
//	Debug("signals ");
//	for (uint8_t index = 0; index < SIGNAL_COUNT; ++index) {
//		if (index) {
//			Debug(", ");
//		}
//		Debug(ram.signals[index]);
//	}
//	for (uint8_t index = 0; index < BUS_COUNT; ++index) {
//		Debugln();
//		Debug(RAM_BUS[index]);
//		Debug(" ");
//		Bus bus = ram.bus[index];
//		Debug(bus.width);
//		Debug(" 0b");
//		Debugln(bus.high, BIN);
//		for (uint8_t index = 0; index < bus.width; ++index) {
//			if (index) {
//				Debug(", ");
//			}
//			Debug(bus.pins[index]);
//		}
//		Debugln();
//	}
}

void ic_memory_test() {
	String line;
	IC ic;
	File file = fat.open(MEMORYFILE);
	if (file) {
		worker.indicator = 0;
		worker.found = false;
		while (file.available()) {
			file.readStringUntil('$');
			if (!file.available()) {
				// Avoid timeout at the end of file
				break;
			}
			// Aliases
			bool first = true;
			bool loop = true;
			while (loop) {
				line = file.readStringUntil('\n');
				if (line[0] == '$' || first) {
					// Code and aliases
					if (worker.found == false) {
						String code;
						int start = first ? 0 : 1;
						int stop = 0;
						first = false;
						line.trim();
						while ((stop = line.indexOf(':', start)) > -1) {
							code = line.substring(start, stop++);
							if (worker.code.equalsIgnoreCase(code)) {
								ic.code = code;
								worker.found = true;
							}
							start = stop;
						}
						// Last alias
						code = line.substring(start);
						if (worker.code.equalsIgnoreCase(code)) {
							ic.code = code;
							worker.found = true;
						}
					}
				} else {
					ic.description = line;
					loop = false;
				}
			}
			ic.count = file.readStringUntil('\n').toInt();
			if (worker.found) {
				worker.index = 0;
				worker.success = 0;
				worker.failure = 0;
				worker.package.count = ic.count;
				ic_package_create();
				ic_package_output();
				ics.push(new IC(ic));
				ui_draw_content();
				while (file.peek() != '$') {
					line = file.readStringUntil('\n');
					line.trim();
					lines.push(new String(line));
				}
				break;
			} else {
				ui_draw_indicator(COLOR_SKIP);
			}
		}
		file.close();
		if (worker.found) {
			ic_parse_memory(ic);
			switch (ic.type) {
			case TYPE_DRAM:
				memory.loop = &ic_ram_loop;
				memory.idle = &ic_dram_idle;
				memory.fill = &ic_dram_fill;
				if (memory.bus[BUS_ADDRESS].width > 16) {
					memory.write_address = &ic_bus_write_address_dword;
				} else {
					memory.write_address = &ic_bus_write_address_word;
				}
				break;
			case TYPE_SRAM:
				memory.loop = &ic_ram_loop;
				memory.idle = &ic_sram_idle;
				memory.fill = &ic_sram_fill;
				if ((memory.bus[BUS_RAS].width > 16) || (memory.bus[BUS_CAS].width > 16)) {
					memory.write_address = &ic_bus_write_address_dword;
				} else {
					memory.write_address = &ic_bus_write_address_word;
				}
				break;
			case TYPE_ROM:
				memory.loop = &ic_rom_loop;
				memory.idle = &ic_rom_idle;
				if (memory.bus[BUS_ADDRESS].width > 16) {
					memory.write_address = &ic_bus_write_address_dword;
				} else {
					memory.write_address = &ic_bus_write_address_word;
				}
				break;
			}
			memory.loop();
//			ic_ram();
		}
		worker.state = state_tested;
	} else {
		Debug(MEMORYFILE);
		Debugln(F(" not found"));
		worker.state = state_media;
	}
	ui_draw_screen();
}

#if 0
void ic_test_rom() {
	String line;
	IC ic;
	File file = fat.open(MEMORYFILE);
	if (file) {
		worker.indicator = 0;
		worker.found = false;
		while (file.available()) {
			file.readStringUntil('$');
			if (!file.available()) {
				// Avoid timeout at the end of file
				break;
			}
			// Aliases
			bool first = true;
			bool loop = true;
			while (loop) {
				line = file.readStringUntil('\n');
				if (line[0] == '$' || first) {
					// Code and aliases
					if (worker.found == false) {
						String code;
						int start = first ? 0 : 1;
						int stop = 0;
						first = false;
						line.trim();
						while ((stop = line.indexOf(':', start)) > -1) {
							code = line.substring(start, stop++);
							if (worker.code.equalsIgnoreCase(code)) {
								ic.code = code;
								worker.found = true;
							}
							start = stop;
						}
						// Last alias
						code = line.substring(start);
						if (worker.code.equalsIgnoreCase(code)) {
							ic.code = code;
							worker.found = true;
						}
					}
				} else {
					ic.description = line;
					loop = false;
				}
			}
			ic.count = file.readStringUntil('\n').toInt();
			if (worker.found) {
				worker.index = 0;
				worker.success = 0;
				worker.failure = 0;
				worker.package.count = ic.count;
				ic_package_create();
				ic_package_output();
				ics.push(new IC(ic));
				ui_draw_content();
				while (file.peek() != '$') {
					line = file.readStringUntil('\n');
					line.trim();
					lines.push(new String(line));
				}
				break;
			} else {
				ui_draw_indicator(COLOR_SKIP);
			}
		}
		file.close();
		if (worker.found) {
			ic_parse_memory(ic);
			switch (ic.loop) {
			case TYPE_ROM:
				memory.idle = &ic_rom_idle;
//				ram.fill = &ic_dram_fill;
				if (memory.bus[BUS_ADDRESS].width > 16) {
					memory.write_address = &ic_bus_write_address_dword;
				} else {
					memory.write_address = &ic_bus_write_address_word;
				}
				break;
			}
			ic_rom_loop();
		}
		worker.state = state_tested;
	} else {
		Debug(MEMORYFILE);
		Debugln(F(" not found"));
		worker.state = state_media;
	}
	ui_draw_screen();
}
#endif

///////////////////////////////
// Interrupt Service Routine //
///////////////////////////////

void isr_user_button() {
	worker.interrupted = true;
}

/////////////
// Arduino //
/////////////

void setup() {
	Serial.begin(115200);
#if TEST
	ic_package_test();
	while (true) {};
#endif
	// Power rails default to TTL
	digitalWrite(A7, LOW);
	pinMode(A7, OUTPUT);
	// User button
	pinMode(21, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(21), isr_user_button, LOW);
	// TFT setup
	tft_init();
#if 0
	ui_draw_ic(20, false);
	ui_draw_ic(40, true);
	// 4116
	ui_draw_ic(16, false);
	// VBB -5 V white
	// VSS GND black
	// VCC +5 V red
	// VDD +12 V yellow
	ui_draw_pin(16, false, 0, TFT_WHITE);
	ui_draw_pin(16, false, 7, TFT_YELLOW);
	ui_draw_pin(16, false, 8, TFT_RED);
	ui_draw_pin(16, false, 15, TFT_BLACK);
	while (true) {
		ts_wait();
		Serial.println("12V, 5V");
		digitalWrite(A7, HIGH);
		delay(1000);
		ts_wait();
		Serial.println("TTL");
		digitalWrite(A7, LOW);
		delay(1000);
	};
#endif
	// SD card
	worker.state = sd_begin() ? state_startup : state_media;
#if 0
	uint32_t index = ini_read_key("ROM", "index");
	Debugln(index);
	ini_write_key("ROM", "index", ++index);
	ini_read_key("ROM", "index");
	Debugln(index);
	ini_write_key("ROM", "index", ++index);
	ini_read_key("ROM", "index");
	Debugln(index);
	ini_write_key("ROM", "index", ++index);
	ini_read_key("ROM", "index");
	Debugln(index);
	ini_write_key("ROM", "index", ++index);
	while (true) {};
#endif
	// UI
	ui_draw_screen();
}

void loop() {
	bool touched = ts_touched();
	int state = worker.state;
	if (touched) {
		switch (worker.state) {
		case state_startup:
		case state_identified:
		case state_tested:
		case state_option:
			switch (worker.action) {
				case action_identify_logic:
					if (buttons[button_test].contains(worker.x, worker.y)) {
						Debugln(F("test"));
						worker.code = ics[worker.index]->code;
						worker.action = action_test_logic;
						state =  state_test_logic;
					}
					break;
				case action_test_logic:
				case action_test_ram:
				case action_test_rom:
					if (buttons[button_redo].contains(worker.x, worker.y)) {
						Debugln(F("redo"));
						switch (worker.action) {
						case action_test_logic:
							state = state_test_logic;
							break;
						case action_test_ram:
							state = state_test_ram;
							break;
						case action_test_rom:
							state = state_test_rom;
							break;
						}
#if 0
						state = worker.action == action_test_logic ? state_test_logic : state_test_ram;
#endif
					}
					break;
			}
			if (buttons[button_escape].contains(worker.x, worker.y)) {
				Debugln(F("menu"));
				state = state_menu;
			}
			break;
		case state_media:
			if (buttons[button_escape].contains(worker.x, worker.y)) {
				Debugln(F("media"));
				if (sd_begin()) {
					state = state_menu;
				} else {
					Debugln(F("SD error"));
				}
			}
			break;
		case state_menu:
			if (buttons[button_identify_logic].contains(worker.x, worker.y)) {
				Debugln(F("identify logic"));
				state = state_identify_logic;
			}
			if (buttons[button_test_logic].contains(worker.x, worker.y)) {
				Debugln(F("test logic"));
				ics.clear();
				worker.action = action_test_logic;
				worker.code = "";
				state = state_keyboard;
			}
			if (buttons[button_test_ram].contains(worker.x, worker.y)) {
				Debugln(F("test ram"));
				ics.clear();
				worker.action = action_test_ram;
				worker.code = "";
				state = state_keyboard;
			}
			if (buttons[button_test_rom].contains(worker.x, worker.y)) {
				Debugln(F("test rom"));
				ics.clear();
				worker.action = action_test_rom;
				worker.code = "";
				state = state_keyboard;
			}
			break;
		case state_identify_logic:
			if (buttons[button_dip14].contains(worker.x, worker.y)) {
				Debugln(F("dip 14"));
				state = state_package_dip;
				worker.package.count = 14;
				ic_package_create();
			}
			if (buttons[button_dip16].contains(worker.x, worker.y)) {
				Debugln(F("dip 16"));
				state = state_package_dip;
				worker.package.count = 16;
				ic_package_create();
			}
			if (buttons[button_escape].contains(worker.x, worker.y)) {
				Debugln(F("menu"));
				state = state_menu;
			}
			break;
		}
	}
	if (worker.state == state) {
		switch (worker.state) {
		case state_keyboard:
		if (keyboard.read(worker.x, worker.y, touched)) {
			if (worker.code.length() < INPUT_LENGTH) {
				worker.code += keyboard.key();
			}
			tft.setCursor(INPUT_X, INPUT_Y);
			tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
			tft.setTextSize(INPUT_SIZE);
			tft.print(worker.code);
		}
		for (uint8_t index = button_enter; index < button_escape; ++index) {
			if (touched && buttons[index].contains(worker.x, worker.y)) {
				buttons[index].press(true);
			} else {
				buttons[index].press(false);
			}
			if (buttons[index].justReleased()) {
				buttons[index].drawButton();
			}
			if (buttons[index].justPressed()) {
				buttons[index].drawButton(true);
				// Enter
				if (index == button_enter) {
					if (worker.code.length()) {
						switch (worker.action) {
						case action_test_logic:
							state = state_test_logic;
							break;
						case action_test_ram:
							state = state_test_ram;
							break;
						case action_test_rom:
//							state = state_test_rom;
							state = state_option;
							break;
						}
					} else {
						state = state_menu;
					}
				}
				// Clear
				if (index == button_clear) {
					tft.fillRect(INPUT_X, INPUT_Y, TFT_WIDTH - 2 - INPUT_X, INPUT_SIZE * 8, COLOR_BACKGROUND);
					worker.code = "";
				}
				// Delete
				if (index == button_del) {
					worker.code.remove(worker.code.length() - 1, 1);
					tft.fillRect(INPUT_X + (INPUT_SIZE * 6 * worker.code.length()), INPUT_Y, INPUT_SIZE * 6, INPUT_SIZE * 8, COLOR_BACKGROUND);
				}
				// UI debounce
				delay(100);
			}
		}
			break;
		case state_option:
			for (uint8_t index = 0; index < checkbox_count; ++index) {
				checkboxes[index].Read(worker.x, worker.y, touched);
			}
			break;
		case state_identified:
			if (ics.count() > 1) {
				for (uint8_t index = 0; index < button_escape; ++index) {
					if (touched && buttons[index].contains(worker.x, worker.y)) {
						buttons[index].press(true);
					} else {
						buttons[index].press(false);
					}
					if (buttons[index].justReleased()) {
						buttons[index].drawButton();
					}
					if (buttons[index].justPressed()) {
						buttons[index].drawButton(true);
						// Previous
						if (index == button_previous) {
							if (worker.index == 0) {
								worker.index = ics.count();
							}
							worker.index--;
						}
						// Next
						if (index == button_next) {
							worker.index++;
							if (worker.index == ics.count()) {
								worker.index = 0;
							}
						}
						ui_draw_content();
						// UI debounce
						delay(100);
					}
				}
			}
			break;
		}
	}
	if (worker.state != state) {
		worker.state = state;
		ui_draw_screen();
		Debug(F("State: "));
		Debugln(worker.state);
	}
#if CAPTURE
	if (worker.interrupted) {
		sd_screen_capture();
		worker.interrupted = false;
	}
#endif
}
