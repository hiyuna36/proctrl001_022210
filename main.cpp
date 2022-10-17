//C
#include <stdio.h>
//pick sdk
#include "pico/stdlib.h"
#include "hardware/uart.h"
//tinyusb
#include "tusb.h"
#include "bsp/board.h"

#define BAUD_RATE 38400

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

class uart {
private:
	static constexpr int TX_PIN = 6;
	static constexpr int RX_PIN = 7;
public:
	void Init();
	void Enable();
	void Disable();
	//送信（同期）
	int Send(const uint8_t data[], const int size);
	//受信（バッファ読み込み）
	int RecvBuf(uint8_t data[], const int size);
	//受信バッファのクリア
	void ClearRecvBuf();
};

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

void init()
{
	//tinyusb
    board_init();
    tusb_init();

	//TODO UART機能のON/OFFが出来るにようにする
	//uart

	// Set up our UART with the required speed.
    uart_init(uart1, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(6, GPIO_FUNC_UART);
    gpio_set_function(7, GPIO_FUNC_UART);

	//試しの送信
	// Send out a string, with CR/LF conversions
    uart_puts(uart1, " Hello, UART!\n");
}

void ctrl_task()
{
	static uint32_t start_ms = 0;
	static bool led_state = false;
	constexpr uint32_t interval_ms = 10;//ms
	static uint32_t counter = 0;

	if (board_millis() - start_ms < interval_ms) return; // not enough time
	start_ms += blink_interval_ms;

	counter++;
	if (counter == 150) {
		JoystickReport.Button.L = 1;
		JoystickReport.Button.R = 1;
	}
	else if (counter >= 200) {
		JoystickReport.Button.L = 0;
		JoystickReport.Button.R = 0;
		counter = 0;
	}

	if ( tud_suspended() ) {
		tud_remote_wakeup();
		return;
	}
	if ( !tud_hid_ready() ) return;
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
//TUD_HID_REPORT_DESC_GENERIC_INOUT(CFG_TUD_HID_EP_BUFSIZE);

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
	// TODO not Implemented
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
	// This example doesn't use multiple report and report ID
	(void) itf;
	(void) report_id;
	(void) report_type;

	// echo back anything we received from host
	//tud_hid_report(0, buffer, bufsize);
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