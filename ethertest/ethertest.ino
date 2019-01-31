#include <Arduino.h>
# include <OneWire.h>
#include "../../../../../Program Files (x86)/Arduino/hardware/tools/avr/lib/gcc/avr/4.9.2/include/stdint-gcc.h"

#define debug 1

#define RELAY_PIN 2
#define OUTPUT_PIN 3
#define INPUT_PIN 4
#define INTERVAL 5000UL

unsigned long last;
static int8_t cur_state = 1;

void setup() {

    delay(1000);
    Serial.begin(9600);  // Debugging only
    Serial.println("setup");

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(OUTPUT_PIN, OUTPUT);
    pinMode(INPUT_PIN, INPUT);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    digitalWrite(OUTPUT_PIN, HIGH);


    Serial.println("Init OK");
    Serial.println();
}

void loop() {

    if((millis() - last) > INTERVAL) {
        Serial.println(cur_state > 0 ? HIGH : LOW);
        digitalWrite(RELAY_PIN, cur_state > 0 ? HIGH : LOW);
        cur_state = -cur_state;
        last = millis();

        int8_t result = digitalRead(INPUT_PIN);

        Serial.println(result);
    }

    int val = digitalRead(INPUT_PIN);
    digitalWrite(LED_BUILTIN, val);

/*
    if(digitalRead(INPUT_PIN) == HIGH) {
        digitalWrite(LED_BUILTIN, HIGH);
    } else {
        digitalWrite(LED_BUILTIN, LOW);
    }
*/
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