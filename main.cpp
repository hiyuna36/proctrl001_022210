//C
#include <stdio.h>
//pick sdk
#include "pico/stdlib.h"
#include "hardware/uart.h"
//tinyusb
#include "tusb.h"
#include "bsp/board.h"

#define BAUD_RATE 115200

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
	BLINK_NOT_MOUNTED = 250,
	BLINK_MOUNTED = 1000,
	BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);

union USB_JoystickReport_Input_t
{
	uint8_t B[8];
	struct {
		union {
			uint16_t hw;
			struct {
				uint16_t Y:1;
				uint16_t B:1;
				uint16_t A:1;
				uint16_t X:1;
				uint16_t L:1;
				uint16_t R:1;
				uint16_t ZL:1;
				uint16_t ZR:1;
				uint16_t Minus:1;
				uint16_t Plus:1;
				uint16_t LClick:1;
				uint16_t RClick:1;
				uint16_t Home:1;
				uint16_t Capture:1;
			};
		} Button;
		uint8_t Hat;//0-7: 0:u 2:r 4:d 6:l 8 Neutral
		uint8_t LX;
		uint8_t LY;
		uint8_t RX;
		uint8_t RY;
		uint8_t VendorSpec;
	};
	USB_JoystickReport_Input_t() {
		Button.hw = 0;
		Hat = 8;
		LX = 128;
		LY = 128;
		RX = 128;
		RY = 128;
		VendorSpec = 0;
	}
} JoystickReport;

//Gamepad button mapping
// this XInput Switch PS3    DInput
// B1   A      B      ×     2
// B2   B      A      ○     3
// B3   X      Y      □     1
// B4   Y      X      △     4
// L1   LB     L      L1     5
// R1   RB     R      R1     6
// L2   LT     ZL     L2     7
// R2   RT     ZR     R2     8
// S1   Back   -      Select 9
// S2   Start  +      Start  10
// L3   LS     LS     L3     11
// R3   RS     Rs     R3     12
// A1   Guide  Home          13
// A2          Capture       14
union GamepadState {
	uint8_t B[11];
	struct {
		union {
			uint16_t B;
			struct {
				uint16_t B1 : 1;
				uint16_t B2 : 1;
				uint16_t B3 : 1;
				uint16_t B4 : 1;
				uint16_t L1 : 1;
				uint16_t R1 : 1;
				uint16_t L2 : 1;
				uint16_t R2 : 1;
				uint16_t S1 : 1;
				uint16_t S2 : 1;
				uint16_t L3 : 1;
				uint16_t R3 : 1;
				uint16_t A1 : 1;
				uint16_t A2 : 1;
				uint16_t : 2;
			};
		} Button;
		uint16_t Aux;
		union {
			uint8_t B;
			struct {
				uint8_t Up : 1;
				uint8_t Down : 1;
				uint8_t Left : 1;
				uint8_t Right : 1;
				uint8_t : 4;
			};
		} DPad;
		uint8_t LX;
		uint8_t LY;
		uint8_t RX;
		uint8_t RY;
		uint8_t LT;
		uint8_t RT;
	};
	void Clear() {
		DPad.B = 0;
		Button.B = 0;
		Aux = 0;
		LX = 128;
		LY = 128;
		RX = 128;
		RY = 128;
		RT = 0;
		RT = 0;
	}
	GamepadState() {
		Clear();
	}
};

struct UartBuffer {
	uint8_t buf[32];
	uint8_t buf_write_ptr;
	uint8_t buf_read_ptr;
	bool sync;
	uint8_t last_syncb;
	UartBuffer() {
		buf_read_ptr = 0;
		buf_write_ptr = 0;
		sync = false;
		last_syncb = 0x00;
	}
	uint datain(const uint8_t _c) {
		buf[buf_write_ptr] = _c;
		buf_write_ptr = (buf_write_ptr + 1) & 0x1F;
		if (buf_write_ptr == buf_read_ptr) {
			buf_read_ptr = (buf_write_ptr + 1) & 0x1F;
		}
		uint8_t bufdatasize = (buf_write_ptr - buf_read_ptr) & 0x1F;
		if (sync == false) {
			if (bufdatasize >= 1) {
				uint8_t syncb = buf[(buf_read_ptr + 0) & 0x1F];
				if ((syncb & 0xF0) == 0b1010'0000) {
					if (bufdatasize >= 2) {
						uint8_t datasize = buf[(buf_read_ptr + 1) & 0x1F];
						if (12 == datasize) {
							if (bufdatasize == 14) {
								sync = true;
								last_syncb = buf[(buf_read_ptr + 0) & 0x1F];
								return 14 - 2;//1 pacet 受信
							}
						}
						else {
							buf_read_ptr = (buf_read_ptr + 2) & 0x1F;
						}
					}
				}
				else {
					buf_read_ptr = (buf_read_ptr + 1) & 0x1F;
				}
			}
		}
		else {
			if (bufdatasize >= 2) {
				uint8_t sync = buf[(buf_read_ptr + 0) & 0x1F];
				uint8_t datasize = buf[(buf_read_ptr + 1) & 0x1F];
				if (sync == ((0b1010'0000 & 0xF0) + ((last_syncb + 1) & 0x0F))) {
					if (12 == datasize) {
						if (bufdatasize == 14) {
							sync = true;
							last_syncb = buf[(buf_read_ptr + 0) & 0x1F];
							return 14 - 2;//1 pacet 受信
						}
					}
					else {
						sync = false;
						buf_read_ptr = (buf_read_ptr + 2) & 0x1F;
					}
				}
				else {
					sync = false;
					buf_read_ptr = (buf_read_ptr + 1) & 0x1F;
				}
			}
		}
		return 0;
	}
	int getdata(uint8_t *data, uint8_t &len) {
		uint8_t datasize = buf[(buf_read_ptr + 1) & 0x1F];
		len = datasize;
		for (int i = 0; i < datasize; i++) {
			data[i] = buf[(buf_read_ptr + 2 + i) & 0x1F];
		}
		buf_read_ptr = buf_write_ptr;
		return 0;
	}
} uartbuf;

void init()
{
	//tinyusb
    board_init();
    tusb_init();

	//uart

	// Set up our UART with the required speed.
    uart_init(uart0, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
}

void ctrl_task()
{
	static uint32_t start_ms = 0;
	static bool led_state = false;
	constexpr uint32_t interval_ms = 10;//ms
	static uint32_t counter = 0;

	//static uint8_t rxbuf[8];
	//static uint8_t rxbufptr = 0;

	//10ms間隔にする
	if (board_millis() - start_ms < interval_ms) return; // not enough time
	start_ms += blink_interval_ms;

	//uartの受信バッファを読む～
	while (true) {
		if (uart_is_readable(uart0) == false) break;
		uint8_t c;
		uart_read_blocking(uart0, &c, 1);
		if (uartbuf.datain(c) == 12) {
			uint8_t len;
			GamepadState state;
			uint8_t buf[12];
			uartbuf.getdata(buf, len);
			::memcpy(state.B, &buf[1], 11);
			JoystickReport.Button.Y = state.Button.B3;
			JoystickReport.Button.B = state.Button.B1;
			JoystickReport.Button.A = state.Button.B2;
			JoystickReport.Button.X = state.Button.B4;
			JoystickReport.Button.L = state.Button.L1;
			JoystickReport.Button.R = state.Button.R1;
			JoystickReport.Button.ZL = ((state.LT > 128) ? 1 : 0);
			JoystickReport.Button.ZR = ((state.RT > 128) ? 1 : 0);
			JoystickReport.Button.Minus = state.Button.S1;
			JoystickReport.Button.Plus = state.Button.S2;
			JoystickReport.Button.LClick = state.Button.L3;
			JoystickReport.Button.RClick = state.Button.R3;
			JoystickReport.Button.Home = state.Button.A1;
			JoystickReport.Button.Capture = state.Button.A2;
			JoystickReport.LX = state.LX;
			JoystickReport.LY = state.LY;
			JoystickReport.RX = state.RX;
			JoystickReport.RY = state.RY;
			if (state.DPad.Up == 1 && state.DPad.Right == 0 && state.DPad.Down == 0 && state.DPad.Left == 0) JoystickReport.Hat = 0;
			else if (state.DPad.Up == 1 && state.DPad.Right == 1 && state.DPad.Down == 0 && state.DPad.Left == 0) JoystickReport.Hat = 1;
			else if (state.DPad.Up == 0 && state.DPad.Right == 1 && state.DPad.Down == 0 && state.DPad.Left == 0) JoystickReport.Hat = 2;
			else if (state.DPad.Up == 0 && state.DPad.Right == 1 && state.DPad.Down == 1 && state.DPad.Left == 0) JoystickReport.Hat = 3;
			else if (state.DPad.Up == 0 && state.DPad.Right == 0 && state.DPad.Down == 1 && state.DPad.Left == 0) JoystickReport.Hat = 4;
			else if (state.DPad.Up == 0 && state.DPad.Right == 0 && state.DPad.Down == 1 && state.DPad.Left == 1) JoystickReport.Hat = 5;
			else if (state.DPad.Up == 0 && state.DPad.Right == 0 && state.DPad.Down == 0 && state.DPad.Left == 1) JoystickReport.Hat = 6;
			else if (state.DPad.Up == 1 && state.DPad.Right == 0 && state.DPad.Down == 0 && state.DPad.Left == 1) JoystickReport.Hat = 7;
			else JoystickReport.Hat = 8;
		}
	}

	if (tud_suspended()) {
		tud_remote_wakeup();
		return;
	}
	if (!tud_hid_ready()) return;
	tud_hid_report(0, &JoystickReport, sizeof(JoystickReport));
}

int main(){
	init();
    while(1) {
		ctrl_task();
        tud_task(); // device task
		led_blinking_task();
    }
    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
extern "C" void tud_mount_cb(void)
{
	blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
extern "C" void tud_umount_cb(void)
{	
	blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
extern "C" void tud_suspend_cb(bool remote_wakeup_en)
{
	(void) remote_wakeup_en;
	blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
extern "C" void tud_resume_cb(void)
{
	blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
extern "C" uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
	(void) itf;
	(void) report_id;
	(void) report_type;
	(void) buffer;
	(void) reqlen;

	return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
extern "C" void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
	(void) itf;
	(void) report_id;
	(void) report_type;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
	static uint32_t start_ms = 0;
	static bool led_state = false;

	// Blink every interval ms
	if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
	start_ms += blink_interval_ms;

	board_led_write(led_state);
	led_state = 1 - led_state; // toggle
}
