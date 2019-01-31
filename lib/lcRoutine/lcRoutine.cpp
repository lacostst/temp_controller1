#include "lcRoutine.h"
#include <OneWire.h>


uint8_t calc_checksum(RxTxMsg *msg) {
	
  uint8_t data[6];
  message_to_array(msg, data);
  uint8_t sum = 0;
  for (uint8_t i = 0; i < msg->size; i++) { // skip last element - crc_sum
        sum += data[i];
  }
  return -sum;
}

void message_to_array(RxTxMsg *msg, uint8_t *buffer) {

	buffer[0] = 6;
	buffer[1] = msg->dst_addr;
	buffer[2] = msg->src_addr;
	buffer[3] = msg->key;
	buffer[4] = msg->value;
	buffer[5] = 0;
	
}

RxTxMsg message_from_array(uint8_t *buffer) {
	RxTxMsg msg;
	
	msg.size = buffer[0];
	msg.dst_addr = buffer[1];
	msg.src_addr = buffer[2];
	msg.key =  buffer[3];
	msg.value = buffer[4];
	msg.checksum = buffer[5];
	
	return msg;
}

byte is_message_incorrect(RxTxMsg *msg, uint8_t this_addr) {
	byte result = 0;
	
	if(msg->dst_addr != this_addr) {
		result = 1;
	}
	if(msg->checksum != calc_checksum(msg)) {
		result = 3;
	}
	
	return result;
}

void message_to_string(char *pref, RxTxMsg *msg, char *str) {
	sprintf(str, "%s message(size(%x), dst_addr(%x), src_addr(%x), key(%x), value(%x), chcksm(%x))\0",
              pref, msg->size, msg->dst_addr, msg->src_addr, msg->key, msg->value, msg->checksum
    );
}

int memoryFree() {
    int freeValue;
    if((int)__brkval == 0)
        freeValue = ((int)&freeValue) - ((int)&__bss_end);
    else
        freeValue = ((int)&freeValue) - ((int)__brkval);
    return freeValue;
}

float getTempFromDS(byte *dsAaddr, OneWire ds) {

	//returns the temperature from one DS18S20 in DEG Celsius
	byte data[12];

	ds.reset();
	ds.select(dsAaddr);
	ds.write(0x44);
	delay(750);
	ds.reset();
	ds.select(dsAaddr);
	ds.write(0xBE);
	data[0] = ds.read();
	data[1] = ds.read();

	byte high_byte = data[1];
	byte low_byte = data[0];
	float tempRead = (high_byte << 8) + low_byte;
	float TemperatureSum = tempRead / 16;

	return TemperatureSum;
}
/*
void getAllDsData(OneWire ds) {

	byte i;
	byte present = 0;
	byte type_s;
	byte data[12];
	byte addr[8];
	float celsius, fahrenheit;
	if ( !ds.search(addr)) {
		Serial.println("No more addresses.");
		Serial.println();
		ds.reset_search();
		delay(250);
		return;
	}

	Serial.print("ROM =");
	for( i = 0; i < 8; i++) {
		Serial.write(' ');
		Serial.print(addr[i], HEX);
	}

	if (OneWire::crc8(addr, 7) != addr[7]) {
		Serial.println("CRC is not valid!");
		return;
	}

	Serial.println();
	// первый байт определяет чип
	switch (addr[0]) {
		case 0x10:
			Serial.println(" Chip = DS18S20"); // или более старый DS1820
			type_s = 1;
			break;

		case 0x28:
			Serial.println(" Chip = DS18B20");
			type_s = 0;
			break;

		case 0x22:
			Serial.println(" Chip = DS1822");
			type_s = 0;
			break;

		default:
			Serial.println("Device is not a DS18x20 family device.");

			return;
	}

	ds.reset();
	ds.select(addr);
	ds.write(0x44);   // начинаем преобразование, используя ds.write(0x44,1) с "паразитным" питанием
	delay(1000);      // 750 может быть достаточно, а может быть и не хватит
	// мы могли бы использовать тут ds.depower(), но reset позаботится об этом
	present = ds.reset();
	ds.select(addr);
	ds.write(0xBE);
	Serial.print(" Data = ");
	Serial.print(present, HEX);
	Serial.print(" ");

	for ( i = 0; i < 9; i++) { // нам необходимо 9 байт
		data[i] = ds.read();
		Serial.print(data[i], HEX);
		Serial.print(" ");
	}

	Serial.print(" CRC=");
	Serial.print(OneWire::crc8(data, 8), HEX);
	Serial.println();

	// конвертируем данные в фактическую температуру
	// так как результат является 16 битным целым, его надо хранить в
	// переменной с типом данных "int16_t", которая всегда равна 16 битам,
	// даже если мы проводим компиляцию на 32-х битном процессоре

	int16_t raw = (data[1] << 8) | data[0];
	if (type_s) {
		raw = raw << 3; // разрешение 9 бит по умолчанию
		if (data[7] == 0x10) {
			raw = (raw & 0xFFF0) + 12 - data[6];
		}
	} else {
		byte cfg = (data[4] & 0x60);
		// при маленьких значениях, малые биты не определены, давайте их обнулим
		if (cfg == 0x00) raw = raw & ~7; // разрешение 9 бит, 93.75 мс
		else if (cfg == 0x20) raw = raw & ~3; // разрешение 10 бит, 187.5 мс
		else if (cfg == 0x40) raw = raw & ~1; // разрешение 11 бит, 375 мс
		//// разрешение по умолчанию равно 12 бит, время преобразования - 750 мс
	}

	celsius = (float)raw / 16.0;
	fahrenheit = celsius * 1.8 + 32.0;
	Serial.print(" Temperature = ");
	Serial.print(celsius);
	Serial.print(" Celsius, ");
	Serial.print(fahrenheit);
	Serial.println(" Fahrenheit");
}

char *toString(uint8_t val) {
	char buffer[7];
	sprintf(buffer, "%d", val);
	//itoa(val, buffer, 10);
	return buffer;
}

char *toString(int8_t val) {
	char buffer[5];
	sprintf(buffer, "%d", val);
	//itoa(val, buffer, 16);
	return buffer;
}

char *toString(struct room_data *room) {

	char format[] =
	"room_id=%d,\n\ropen_pin=%d,\n\rclose_pin=%d,\n\rdirection=%d,\n\rcheck_time=%d,\n\rstop_time=%d,\n\rnext_check=%d,\n\rin_motion=%d,\n\rwas_in_0=%d,\n\rregulator_position=%d";
	char buffer[256];
	sprintf(buffer, format, room->room_id, room->open_pin, room->close_pin, room->direction, room->check_time, room->stop_time, room->next_check, room->in_motion, room->was_in_0, room->regulator_position);
	return buffer;

		struct temp_data boiler_line_t;
		struct temp_data room_t;
		struct temp_data room_line_t;
		struct temp_data req_t;
		int8_t direction; // направление воздействия
		unsigned long check_time; // время последней проверки
		unsigned long stop_time; // время окончания воздействия
		unsigned long next_check; // время следующей проверки
		uint8_t in_motion; // регулятор в движении
		uint8_t was_in_0; // регулятор был в нуле
		int regulator_position; // позиция регулятора


}

*/
