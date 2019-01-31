#include <Arduino.h>
#include <smBut.h>
#include <Manchester.h>
#include <OneWire.h>
#include <LiquidCrystal.h>
#include <stdio.h>
#include <EEPROM.h>
#include <lcRoutine.h>
//#include "../lib/lcRoutine/lcRoutine.h"


smBut btUp(11,1); // button UP
smBut btDown(10,1); // button DOWN

const byte BTN_UP = 1;
const byte BTN_DOWN = 2;

const byte BTN_PRESS = 1;
const byte BTN_LONG_PRESS = 2;
const byte BTN_HOLD = 3;

unsigned long BTN_PRESSED_LAST_TIME;
const unsigned long BTN_PRESSED_DELTA = 500; // 0,5 sec

// PIN
// rx-tx 433MHZ
const int transmit_pin = 12;
const int receive_pin = 2;
uint8_t buffer[BUFFER_SIZE];

// DS18B20
const int DS_PIN = 8;

// MT16-S2
const int RS_PIN = 4;
const int E_PIN = 5;
const int DB4_PIN = 6;
const int DB5_PIN = 7;
const int DB6_PIN = 3;
const int DB7_PIN = 9;

const byte LCD_UNDEF = 0;
const byte LCD_STANDBY = 1;
const byte LCD_TEMP_SET = 2;

byte LCD_MODE = LCD_UNDEF;
byte LCD_NEXT_MODE = LCD_STANDBY;
unsigned long LCD_last_time = 0;
const unsigned long LCD_SET_TIMEOUT = 10000; // 10 sec 


byte lcd_rxtx[] = {0, 0, 0, 0, 0};
byte RXTX_ROOM = 0;
byte RXTX_LINE = 1;
byte RXTX_OUT = 2;
byte RXTX_BOILER = 3;
byte RXTX_REQ_T = 4;

byte RXTX_SEND = 1;
byte RXTX_RECEIVE = 2;

byte LCD_RXTX_STATE = 4;



byte NEW_TEMP = 0;

//int8_t req_temp;

const int TEMP_DELTA = 1;
const int REQ_TEMP_EEPROM_ADDR = 16;

byte KEY_PRESSED = 0;

TempData req_t, out_t, boil_t;

TempData room_t = {0, 0, 0, {0x28, 0xFF, 0xAC, 0x77, 0x6A, 0x14, 0x04, 0x0D}};
TempData line_t = {0, 0, 0, {0x28, 0xFF, 0xE3, 0x5A, 0x65, 0x14, 0x03, 0xBA}};

//28 FF AC 77 6A 14 4 D
//28 FF E3 5A 65 14 3 BA


uint8_t server_addr = 50; // 433mhz-server address
uint8_t this_addr = 1;  // this device address

unsigned long store_period = (long) 15*60*1000;
unsigned long last_store_time = 0;

// Символ стрелка вправо
byte tx_char[8] = {
        0b00000,
        0b00010,
        0b11111,
        0b00010,
        0b00000,
        0b00000,
        0b00000,
        0b00000
};


// Символ стрелка вправо стрелка влево
byte rxtx_char[8] = {
        0b00000,
        0b00010,
        0b11111,
        0b00010,
        0b01000,
        0b11111,
        0b01000,
        0b00000
};

OneWire ds(DS_PIN); // ds-sensors pin

LiquidCrystal lcd(RS_PIN, E_PIN, DB4_PIN, DB5_PIN, DB6_PIN, DB7_PIN);

void getData();
void updateLCD();
void updateStoredData();

void setup() {

    delay(1000);
    Serial.begin(9600);  // Debugging only
    Serial.println("setup");

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    //Initialise LCD
    lcd.begin(16, 2);
    lcd.print("LC-smart home");
    lcd.setCursor(0, 1);
    lcd.print("Init...");
    lcd.createChar(1, tx_char);
    lcd.createChar(2, rxtx_char);

    man.setup(transmit_pin, receive_pin, RXTX_SPEED);

    load_req_temp();

    delay(1000);
    lcd.clear();
}

void loop() {

    int btUpState = btUp.start();
    int btDownState = btDown.start();

    if(btUpState)
        processKey(BTN_UP, btUpState);
    if(btDownState)
        processKey(BTN_DOWN, btDownState);

    getData();
    updateLCD();

    updateStoredData();
}

/*
 * Отрисовываем данные на экране
 */
void updateLCD() {

    if(LCD_MODE == LCD_UNDEF) {
        // На экране еще ничего не написано или данные уже не нужны.
        // Стираем экран и задаем стандартные значения для следующего режима

        clearLCD();

        switch (LCD_NEXT_MODE) {
            case LCD_STANDBY:
                printStandbyScreen();
                LCD_MODE = LCD_STANDBY;
                break;
            case LCD_TEMP_SET:
                printTempSetScreen();
                LCD_MODE = LCD_TEMP_SET;
                break;
        }
    }

    if(LCD_MODE == LCD_STANDBY && NEW_TEMP) {
        // Выводим на экран текущую температуру
        printTempData();
        NEW_TEMP = 0;
    }

    if(LCD_MODE == LCD_STANDBY && LCD_RXTX_STATE) {
        // Выводим состояние RXTX
        printRxTxState();
        LCD_RXTX_STATE = 0;
    }

    if(LCD_MODE == LCD_TEMP_SET) {
        // Переход в режим задачи температуры
        if(KEY_PRESSED) {
            printTempsetData();
            KEY_PRESSED = 0;
        }
        if(abs(millis() - LCD_last_time) > LCD_SET_TIMEOUT) {
            // таймаут на странице установок
            store_req_temp();
            LCD_MODE = LCD_UNDEF;
            LCD_NEXT_MODE = LCD_STANDBY;
        }
    }
}

/*
 * Периодически проверяем и обновляем данные в EEPROM
 */
void updateStoredData() {
    unsigned long s = millis();
    if( abs(s - last_store_time) > store_period ) {
        last_store_time = s;
        // здесь сохраняем значения. говорят, что если значение не меняется - то перезаписывать не будет.
        store_req_temp();
    }

}

void store_req_temp() {
    EEPROM.put(REQ_TEMP_EEPROM_ADDR, req_t.value);
    processTempData(&req_t, SET_REQ_TEMP, RXTX_REQ_T, 3);
}

void load_req_temp() {
    EEPROM.get(REQ_TEMP_EEPROM_ADDR, req_t.value);

    if(Serial) {
        Serial.print("Loaded REQ_TEMP:");
        Serial.println(req_t.value, 1);
    }
}

void clearLCD() {
    lcd.clear();
}

void printStandbyScreen() {

    lcd.setCursor(0, 0);
    //           K
    lcd.print("\x4b:");

    lcd.setCursor(0, 1);
    //           Б
    lcd.print("\xa0:");

    lcd.setCursor(9, 0);
    //           У
    lcd.print("\xa9:");

    lcd.setCursor(9, 1);
    //           Г
    lcd.print("\xa1:");

}

void printTempData() {
    _printTempData(2,  0, room_t.value);
    _printTempData(2,  1, line_t.value);
    _printTempData(11, 0, out_t.value);
    _printTempData(11, 1, boil_t.value);
}

void _printTempData(byte pos, byte line, float temp) {
    lcd.setCursor(pos, line);
    if(temp >= 0) {
        lcd.print("+");
        pos++;
    }
    lcd.setCursor(pos,line);
    lcd.print(temp, 0);
}

void printTempSetScreen() {
    lcd.setCursor(0, 0);
    //           У   с   т .   к   о   м   н .   т   е   м   п :
    lcd.print("\xa9\x63\xbf. \xba\x6f\xbc\xbd. \xbf\x65\xbc\xbe:");
}

void printTempsetData() {
    _printTempData(6, 1, req_t.value);
    lcd.print("  ");
}

void printRxTxState() {

    lcd.setCursor(7,0);
    lcd.write((uint8_t)lcd_rxtx[RXTX_ROOM]);

    lcd.setCursor(7,1);
    lcd.write((uint8_t)lcd_rxtx[RXTX_LINE]);

    lcd.setCursor(15,0);
    lcd.write((uint8_t)lcd_rxtx[RXTX_OUT]);

    lcd.setCursor(15,1);
    lcd.write((uint8_t)lcd_rxtx[RXTX_BOILER]);
}

void updateLCDRxTx(byte rxtx, byte state) {
    lcd_rxtx[rxtx] = state;
    LCD_RXTX_STATE = 1;
}

void getData() {
    //req_t, out_t, boil_t
    processTempData(&room_t, ROOM_TEMP, RXTX_ROOM, 1);
    processTempData(&line_t, LINE_TEMP, RXTX_LINE, 1);
    processTempData(&req_t, GET_REQ_TEMP, RXTX_REQ_T, 0);
    processTempData(&out_t, GET_OUT_TEMP, RXTX_OUT, 0);
    processTempData(&boil_t, GET_BOILER_TEMP, RXTX_BOILER, 0);
}

void processTempData(TempData *t_data, uint8_t key, byte rxtx_value, uint8_t flag) {
    // flag 1 - from DS, 3 - no random, immediately
    //

    // Исключительно для того, чтобы разнести обращение к серверу по времени
    if (flag != 3 && random(0, 1000) > 600) {
        return;
    }

    unsigned long s = millis();
    //Serial.print("First If "); Serial.print(s - t_data->last_ok);Serial.print(" GET_DATA = ");Serial.println(GET_DATA);
    if(flag !=3 && (t_data->last_ok != 0 && (abs(s - t_data->last_ok) < GET_DATA))) {
        // Если последнее успешное чтение не такое старое
        return;
    }
    //Serial.print("Second If "); Serial.print(s - t_data->timestamp);Serial.print(" RETRY_DATA = ");Serial.println(RETRY_DATA);
    if(flag == 3 || (t_data->timestamp == 0 || (abs(s - t_data->timestamp) > RETRY_DATA))) {
        t_data->timestamp = s;
        uint8_t error_code = 0;
        if(flag == 1) {
            t_data->value = round(getTempFromDS(t_data->ds_addr, ds));
        }
        int8_t result = only_send_data(key, t_data->value, &error_code);
        updateLCDRxTx(rxtx_value, RXTX_SEND);
        if(!error_code) {
            t_data->last_ok = s;
            if(flag != 1) {
                t_data->value = result;
            }
            NEW_TEMP = 1;
            updateLCDRxTx(rxtx_value, RXTX_RECEIVE);
        }
    }
}

/*
 * Обработка нажатия кнопки.
 */
void processKey(byte button, byte btnState) {

    LCD_last_time = millis();

    if(Serial) {
        char msg[32];
        sprintf(msg, "ProcessKey(%d, %d)", button, btnState);
        Serial.println(msg);
    }

    switch (btnState) {
        case BTN_PRESS:
            btnShortPress(button);
            break;
        case BTN_LONG_PRESS:
            btnLongPress(button);
            break;
        case BTN_HOLD:
            btnHold(button);
            break;
        default:

            break;
    }
}

void btnShortPress(byte btn) {
    //btnPress(btn);
}

void btnLongPress(byte btn) {
    btnPress(btn);
}

void btnHold(byte btn) {
    if(Serial) {
        char msg[32];
        sprintf(msg, "btnHold(%d)", btn);
        Serial.println(msg);
    }
    if(btn == BTN_UP || btn == BTN_DOWN) {
        if(LCD_MODE == LCD_STANDBY) {
            LCD_MODE = LCD_UNDEF;
            LCD_NEXT_MODE = LCD_TEMP_SET;
        }
    }
}

void btnPress(byte btn) {
    KEY_PRESSED = 1;
    if(LCD_MODE == LCD_TEMP_SET) {
        if(abs(millis() - BTN_PRESSED_LAST_TIME) > BTN_PRESSED_DELTA) {
            if(btn == BTN_UP) {
                req_t.value+=TEMP_DELTA;
            } else if (btn == BTN_DOWN) {
                req_t.value-=TEMP_DELTA;
            }
            BTN_PRESSED_LAST_TIME = millis();
        }
    }
}


int8_t only_send_data(uint8_t key, int8_t value, uint8_t *error_code) {
    int8_t result;
    uint8_t error = 1;
    byte receive = 0;

    RxTxMsg msg = {6, server_addr, this_addr, key, value, 0};
    msg.checksum = calc_checksum(&msg);

    if(Serial) {
        char str[256];
        message_to_string("Send ::", &msg, str);
        Serial.println(str);
    }

    man.transmitArray(sizeof(msg), (byte *) &msg);
    man.beginReceiveArray(BUFFER_SIZE, buffer);

    unsigned long s = millis();
    while(!receive && abs(millis() - s) < RXTX_RECEIVE_TIMEOUT) {
        if(man.receiveComplete()) {
            RxTxMsg *r_msg;
            r_msg = (RxTxMsg *) buffer;
            if(Serial) {
                char str[256];
                message_to_string("Receive :: ", r_msg, str);
                Serial.println(str);
            }
            if(!is_message_incorrect(r_msg, this_addr)) {
                if(r_msg->key == msg.key) {
                    result = r_msg->value;
                    receive = 1;
                    error = 0;
                } else {
                    if(Serial) {
                        char str[128];
                        sprintf(str, "Error. Message r_msg->key != msg.key (%d), (%d)", r_msg->key, msg.key);
                        Serial.println(str);
                    }
                }
            } else {
                error = 2;
                if(Serial) {
                    Serial.println("Message incorrect");
                }
            }
        }
    }
    *error_code = error;
    return result;
}



