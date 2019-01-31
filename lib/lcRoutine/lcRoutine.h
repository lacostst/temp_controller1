#ifndef LC_RTN_H
#define LC_RTN_H

#if ARDUINO >= 100
 #include <Arduino.h>
#else
 #include <WProgram.h>
#endif

#include <OneWire.h>

#define BUFFER_SIZE 32

#define RXTX_SPEED MAN_1200
#define RXTX_RECEIVE_TIMEOUT 1000

// KEYS
#define ROOM_TEMP 1
#define LINE_TEMP 2

#define SERVER_REPLY 40

#define GET_REQ_TEMP 51
#define GET_OUT_TEMP 52
#define GET_BOILER_TEMP 53

#define SET_REQ_TEMP 70

// VALUES
#define VALUE_ACK 6


// crontab. make this every mills
const unsigned long GET_DATA = 60UL*1000UL; // Sync every 60 sec
const unsigned long RETRY_DATA = 5*1000; // Retry of fail every 5 sec




// structure to send/receive data
typedef struct message {
	uint8_t size; // size of received message
	uint8_t dst_addr;		// dst address
	uint8_t src_addr;		// src address
	uint8_t key;		// parameter name
	int8_t value;	// parameter value
	uint8_t checksum;	// checksum
} RxTxMsg;

typedef struct temp_data {
	unsigned long timestamp;
	unsigned long last_ok;
	int8_t value;
	byte ds_addr[8];
} TempData;

typedef struct room_data {
	uint8_t room_id;
	uint8_t open_pin;
	uint8_t close_pin;
	TempData boiler_line_t;
	TempData room_t;
	TempData room_line_t;
	TempData req_t;
	int8_t last_req_t; // это и следующее: последняя температура из расчета на которую было управление
	int8_t last_boiler_line_t; // после
	int8_t direction; // направление воздействия
	unsigned long check_time; // время последней проверки
	unsigned long stop_time; // время окончания воздействия
	unsigned long next_check; // время следующей проверки
	uint8_t in_motion; // регулятор в движении
	uint8_t was_in_0; // регулятор был в нуле
	unsigned long regulator_position; // позиция регулятора
} RoomData;

// calculate checksum
uint8_t calc_checksum(RxTxMsg *msg);

void message_to_array(RxTxMsg *msg, uint8_t *buffer);

struct message message_from_array(uint8_t *buffer);

byte is_message_incorrect(RxTxMsg *msg, uint8_t this_addr);

void message_to_string(char *pref, RxTxMsg *msg, char *str);

float getTempFromDS(byte *dsAddr, OneWire ds);

/*

void getAllDsData(OneWire ds);

char *toString(uint8_t val);
char *toString(int8_t val);

char *toString(struct room_data *room);
*/

// Переменные, создаваемые процессом сборки,
// когда компилируется скетч
extern int __bss_end;
extern void *__brkval;

int memoryFree();


#endif
