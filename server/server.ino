#include <Arduino.h>
#include <avr/pgmspace.h>
#include <SPI.h>
#include <Manchester.h>
#include <lcRoutine.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/lcRoutine/lcRoutine.h"
#include "../../../../../Program Files (x86)/Arduino/libraries/Ethernet/src/Ethernet.h"


#define DEBUG 0
#define EMULATE 0
#define ETHERNET 1

// 30 sec
#define HTTP_TIMEOUT 30000UL

//5 sec
#define HTTP_CONNECTION_TIMEOUT 5000UL

// размер 1 строки для обработки ответа сервера
#define HTTP_RESPONSE_MAX_LENGTH 255

// Enter a MAC address for your controller below.
const byte mac[]  = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xAE };
IPAddress server(10,10,0,55); // numeric IP for server (no DNS)

#define SERVER_PORT 80

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 125, 4);
IPAddress dnServer(10,10,0,100);
IPAddress gw(192,168,125,1);
IPAddress subnet(255,255,255,0);

// Initialize the Ethernet client library
EthernetClient client;

const char SERVER_HOST[] PROGMEM = "Host: sn.lan";

const char REQUEST_URL[] PROGMEM = "GET /l/b/index.php?action=%s&room_id=%d&sensor_name=%s&temp=%d HTTP/1.1";
const char ACTION_ROOM_TEMP[] PROGMEM = "room_temp";
const char ACTION_GET_REQ_TEMP[] PROGMEM = "get_room_set";
const char ACTION_SET_REQ_TEMP[] PROGMEM = "set_room_set";

const char SENSOR_ROOM_TEMP[] PROGMEM = "room_temp";
const char SENSOR_LINE_TEMP[] PROGMEM = "room_line_temp";
const char SENSOR_ROOM_REQ_TEMP[] PROGMEM = "room_req_temp";
const char SENSOR_BOILER_TEMP[] PROGMEM = "boiler_temp";
const char SENSOR_OUT_TEMP[] PROGMEM = "out_temp";

const char REG_RULE_URL[] PROGMEM = "GET /l/b/index.php?action=reg_rule&room_id=%d&dir=%d&n=%lu&st=%lu&nck=%lu HTTP/1.1";
const char REG_ACTION_URL[] PROGMEM = "GET /l/b/index.php?action=reg_action&room_id=%d&pin=%d&level=%d&n=%lu HTTP/1.1";

const char CONTENT_LENGTH[] PROGMEM = "Content-Length: ";
const char SR_SEQUENCE[] PROGMEM = "==[[";
const char ER_SEQUENCE[] PROGMEM = "]]==";

const char RECEIVE_STR[] PROGMEM = "Receive :: ";
const char SEND_STR[] PROGMEM = "Send :: ";

// Внимание!!! В этот буфер должна вместиться самая большая строка из флеш-памяьи
//char PROGMEM_buffer[80];
//char PROGMEM_small_buffer[20];



//Отправлять каждые 30 sec
#define SEND_TIME_INTERVAL 30*1000UL

//Забирать каждые 30 sec
#define GET_TIME_INTERVAL 30*1000UL


#define ONEWIRE_DS_PIN 9

OneWire ds(ONEWIRE_DS_PIN); // на пине (нужен резистор 4.7 КОм)

#define TRANSMIT_PIN 3
#define RECEIVE_PIN 2

// все в секундах, NEXTCHECK в минутах
#define REGULATOR_MAX_TIME 120000L
#define DURATION_FOR_1_DEGREE 2L
#define NEXTCHECK_FOR_1_DEGREE 2L
#define DURATION_FOR_2_4_DEGREE 10L
#define NEXTCHECK_FOR_2_4_DEGREE 2L
#define DURATION_FOR_5_DEGREE 20L
#define NEXTCHECK_FOR_5_DEGREE 1L


uint8_t buffer[BUFFER_SIZE];

// 433mhz-server address
#define THIS_ADDR 50
#define BOILER_ROOM_ID 50

RoomData room1 = {1, 7, 8,  // room_id = 1, open_pin 7, close_pin 8
        {0, 0, 0, { 0x28, 0x79, 0xFC, 0x26, 0x00, 0x00, 0x80, 0x62 }}, // boiler_line_t
        {0, 0, 0, {}}, //room_temp
        {0, 0, 0, {}}, //room_line_temp
        {0, 0, 0, {}}, //room_req_temp
        0, 0, 0, 0, 0, 0, 0, 0,  REGULATOR_MAX_TIME};

RoomData room2 = {2, 5, 6, // room_id = 2, open_pin 5, close_pin 6
        {0, 0, 0, { 0x28, 0xB3, 0xC4, 0x26, 0x00, 0x00, 0x80, 0x88 } }, // boiler_line_t
        {0, 0, 0, {}}, //room_temp
        {0, 0, 0, {}}, //room_line_temp
        {0, 0, 0, {}}, //room_req_temp
        0, 0, 0, 0, 0, 0, 0, 0,  REGULATOR_MAX_TIME};

//room1.regulator_position = REGULATOR_MAX_TIME;
//room2.regulator_position = REGULATOR_MAX_TIME;

RoomData * rooms[] = { &room1, &room2 }; //, &room2  only 1 room now

// Датчик уличной температуры
TempData out_t = {0, 0, 0, { 0x28, 0x07, 0xE0, 0x26, 0x00, 0x00, 0x80, 0xB4 }};

// Датчик температуры основного контура -- адрес пока тот же что на улицу
TempData boiler_t = {0, 0, 0, { 0x28, 0xB3, 0xC4, 0x26, 0x00, 0x00, 0x80, 0x88 }};

RoomData * get_room_by_id(uint8_t room_id);
void processRequest();
void getData();
void storeData();
void getSettings();
void getAction();
void makeAction();
void prepare_temp_http_request(char *action, uint8_t room_id, char *sensor_name, int8_t temp, uint8_t *result);
void prepare_action_rule_http_request(RoomData * room, unsigned long now);
void prepare_action_reg_http_request(RoomData * room, uint8_t pin, uint8_t level, unsigned long now);
void pre_make_http_request(char * request, uint8_t *result);
void getStringFromFlash(const char * src, char * dst);


void setup() {

    delay(1000);
    Serial.begin(9600);
    if(DEBUG) {
        Serial.println(F("setup"));
    }


    // start the Ethernet connection:
    if(ETHERNET) {
        if (Ethernet.begin(mac) == 0) {
            Serial.println(F("Failed to configure Ethernet using DHCP"));
            // try to congifure using IP address instead of DHCP:
            Ethernet.begin(mac, ip, dnServer, gw, subnet);
        }
        // give the Ethernet shield a second to initialize:
        delay(1000);
        client.setConnectionTimeout(HTTP_CONNECTION_TIMEOUT);
    }

    if(DEBUG && ETHERNET) {
        Serial.print(F("IP address: "));
        Serial.println(Ethernet.localIP());
        Serial.println(Ethernet.subnetMask());
        Serial.println(Ethernet.gatewayIP());
        Serial.println(Ethernet.dnsServerIP());
    }

    for(uint8_t i = 0; i< sizeof(rooms)/sizeof(RoomData*); i++) {
        RoomData *room = rooms[i];
        pinMode(room->open_pin, OUTPUT);
        pinMode(room->close_pin, OUTPUT);
    }

    man.setup(TRANSMIT_PIN, RECEIVE_PIN, RXTX_SPEED);
    man.beginReceiveArray(BUFFER_SIZE, buffer);

    Serial.println(F("Init OK"));
    Serial.println();
}

void loop() {
    //Serial.println(memoryFree());
    if(man.receiveComplete()) {
        processRequest();
        man.beginReceiveArray(BUFFER_SIZE, buffer);
    }

    getData();        // получить данные с термодатчиков
    getAction();      // получить значения управляющих воздействия
    makeAction();     // сделать управляющие воздействия
    storeData();      // сохранить настройки
    getSettings();      // взять настройки для комнат
}
void getAction(){
    // Перебор контуров и определение действия
    for(uint8_t i = 0; i< sizeof(rooms)/sizeof(RoomData*); i++) {
        RoomData *room = rooms[i];
        getActionForRoom(room);
    }
}



void makeAction() {
    for(uint8_t i = 0; i< sizeof(rooms)/sizeof(RoomData*); i++) {
        RoomData *room = rooms[i];
        makeActionForRoom(room);
    }
}

void makeActionForRoom(RoomData *room){
    unsigned long s = millis();

    // запустить процесс поворота
    if(room->in_motion == 0 && room->stop_time > s ) {
        uint8_t highPin, lowPin;
        if(room->direction > 0) {
            highPin = room->open_pin;
            lowPin  = room->close_pin;
        } else {
            highPin = room->close_pin;
            lowPin  = room->open_pin;
        }
        digitalWrite(lowPin, LOW);
        digitalWrite(highPin, HIGH);

        room->in_motion = 1;

        if(ETHERNET) {
            prepare_action_reg_http_request(room, lowPin, LOW, s);
            prepare_action_reg_http_request(room, highPin, HIGH, s);
        }
    }

    if(room->in_motion && (room->stop_time < s)) {
        digitalWrite(room->open_pin, LOW);
        digitalWrite(room->close_pin, LOW);
        room->in_motion = 0;

        if(ETHERNET) {
            prepare_action_reg_http_request(room, room->open_pin, LOW, s);
            prepare_action_reg_http_request(room, room->close_pin, LOW, s);
        }

    }
}


void storeData() {
    if(!ETHERNET) {
        return;
    }
    static unsigned long lastSendTime;
    unsigned long s = millis();
    if(abs(s - lastSendTime) > SEND_TIME_INTERVAL ) {
        int8_t result;
        for(uint8_t i = 0; i< sizeof(rooms)/sizeof(RoomData*); i++) {
            RoomData *room = rooms[i];

            char action_str[20];
            char sensor_str[20];

            getStringFromFlash(ACTION_ROOM_TEMP, action_str);
            getStringFromFlash(SENSOR_ROOM_TEMP, sensor_str);
            prepare_temp_http_request(action_str, room->room_id, sensor_str, room->room_t.value, &result);

            getStringFromFlash(ACTION_ROOM_TEMP, action_str);
            getStringFromFlash(SENSOR_LINE_TEMP, sensor_str);
            prepare_temp_http_request(action_str, room->room_id, sensor_str, room->room_line_t.value, &result);

            getStringFromFlash(ACTION_ROOM_TEMP, action_str);
            getStringFromFlash(SENSOR_ROOM_REQ_TEMP, sensor_str);
            prepare_temp_http_request(action_str, room->room_id, sensor_str, room->req_t.value, &result);

            getStringFromFlash(ACTION_ROOM_TEMP, action_str);
            getStringFromFlash(SENSOR_BOILER_TEMP, sensor_str);
            prepare_temp_http_request(action_str, room->room_id, sensor_str, room->boiler_line_t.value, &result);

        }

        char action_str[20];
        char sensor_str[20];

        getStringFromFlash(ACTION_ROOM_TEMP, action_str);
        getStringFromFlash(SENSOR_BOILER_TEMP, sensor_str);
        prepare_temp_http_request(action_str, BOILER_ROOM_ID, sensor_str, boiler_t.value, &result);


        getStringFromFlash(ACTION_ROOM_TEMP, action_str);
        getStringFromFlash(SENSOR_OUT_TEMP, sensor_str);
        prepare_temp_http_request(action_str, BOILER_ROOM_ID, sensor_str, out_t.value, &result);

        lastSendTime = s;
    }
}

void getSettings() {
    if(!ETHERNET) {
        return;
    }
    static unsigned long lastSendTime;
    unsigned long s = millis();
    if(abs(s - lastSendTime) > GET_TIME_INTERVAL ) {
        for(uint8_t i = 0; i< sizeof(rooms)/sizeof(RoomData*); i++) {
            RoomData *room = rooms[i];

            char action_str[20];

            getStringFromFlash(ACTION_GET_REQ_TEMP, action_str);
            prepare_temp_http_request(action_str, room->room_id, "", 0, &room->req_t.value);
        }
        lastSendTime = s;
    }
}

void prepare_temp_http_request(char *action, uint8_t room_id, char *sensor_name, int8_t temp, uint8_t *result) {
    char request[128];
    char url_str[100];
    //GET /l/b/index.php?action=room_temp&room_id=1&sensor_name=room_temp&temp=14 HTTP/1.1
    getStringFromFlash(REQUEST_URL, url_str);
    sprintf(request, url_str, action, room_id, sensor_name, temp);
    pre_make_http_request(request, result);
}

void prepare_action_rule_http_request(RoomData * room, unsigned long now) {
    char request[128];
    uint8_t result;
    char url_str[85];

    //const char REG_RULE_URL[] PROGMEM = "GET /l/b/index.php?action=reg_rule&room_id=%d&dir=%d&n=%u&st=%u&nck=%u HTTP/1.1";
    getStringFromFlash(REG_RULE_URL, url_str);
    sprintf(request, url_str, room->room_id, room->direction, now, room->stop_time, room->next_check);
    pre_make_http_request(request, &result);
}

void prepare_action_reg_http_request(RoomData * room, uint8_t pin, uint8_t level, unsigned long now) {
    char request[128];
    uint8_t result;
    char url_str[82];
    getStringFromFlash(REG_ACTION_URL, url_str);
    sprintf(request, url_str, room->room_id, pin, level, now);
    pre_make_http_request(request, &result);
}

void pre_make_http_request(char * request, uint8_t *result) {
    char response[HTTP_RESPONSE_MAX_LENGTH]; // должен вместить response
    char body[32]; // тело ответа.

    make_http_request(request, response);
    if(strlen(response)) {
        if(parse_http_response(response, body)){
            * result = atoi(body);
            if(DEBUG) {
                Serial.print(F("Body = "));Serial.println(body);
            }
        } else {
            if(DEBUG) {
                Serial.print(F("NO result. ")); Serial.println(body);
            }
        }
    }
}


void make_http_request(char *request, char *response) {

    // if you get a connection, report back via serial:

    if (client.connect(server, SERVER_PORT)) {
        if(DEBUG) { Serial.println(F("Conn")); }

        client.println(request);
        if(DEBUG) { Serial.println(request); }

        char server_host_str[20];
        getStringFromFlash(SERVER_HOST, server_host_str);
        client.println(server_host_str);

        client.println(F("Connection: close"));
        client.println();
    } else {
        // if you didn't get a connection to the server:
        if(DEBUG) {
            Serial.println(F("Connection: fail"));
        }
        return;
    }

    unsigned long time = millis();
    int i = 0;
    uint8_t isBody = 0;
    uint8_t cLength = 0;
    while(client) {
        if (client.available()) {
            char c = client.read();
            //if(DEBUG) Serial.print(c);
            // Проверим заголовки
            if(!isBody && c == 0x0D) { // \r
                if(i == 0) { // если первый символ /r то это разделитель между Header и Body
                    c = client.read(); // читаем /n
                    //if(DEBUG) Serial.print(c);
                    i = 0; // обнуляем позицию.
                    isBody = 1;
                    response[i] = '\0'; // обнуляем строку
                    continue;
                }

                // ищем заголовок Content-Length
                char ct_str[20];
                getStringFromFlash(CONTENT_LENGTH, ct_str);
                if(strstr(response, ct_str)){
                    // 0 - идентичны
                    char buf[4]; // ждем только 3 символа
                    int k = 0;
                    buf[k] = '\0';
                    for(k = 0; k < i - strlen(ct_str); k++) {
                        buf[k] = response[k + strlen(ct_str)];
                    }
                    buf[k++] = '\0';
                    cLength = atoi(buf);
                    //if(DEBUG) {
                    //    Serial.println();
                    //    Serial.print(F("ContentLength = "));Serial.println(cLength);
                    //}
                }
                if(!isBody) {
                    c = client.read(); // читаем /n
                    //if(DEBUG) Serial.print(c);
                    i = 0;
                    response[i] = '\0'; // обнуляем строку
                    continue;
                }
            }
            if( i >= HTTP_RESPONSE_MAX_LENGTH - 2 ) { // magic nubmer 253 (255 max size of response)
                if(DEBUG) {
                    Serial.println(F("This line of Response is more then 253 chars. Skiping..."));
                    continue;
                }
            }
            response[i++] = c;
            response[i+1] = '\0';
        }

        if(isBody && cLength > 0 && i >= cLength) {
            client.stop();
            Serial.println(F("Stop by length"));
        }
        if (!client.connected()) {
            if(DEBUG) {
                Serial.println();
                Serial.println(F("disconn"));
            }
            client.stop();
        }
        if(millis() - time > HTTP_TIMEOUT) {
            if(DEBUG) Serial.println(F("Conn time out"));
            client.stop();
        }
    }
    response[i] = '\0';
}


int parse_http_response(char *response, char *data) {
    int size = 0;

    char sr_str[20];
    char er_str[20];

    getStringFromFlash(SR_SEQUENCE, sr_str);
    getStringFromFlash(ER_SEQUENCE, er_str);

    int pos_s = strstr(response, sr_str);
    int pos_e = strstr(response, er_str);
    if(pos_s > 0 && pos_e > 0) {
        size = pos_e - (pos_s + strlen(sr_str));
        memcpy(data, pos_s+strlen(er_str), size);
        data[size] = '\0';
    }
    return size;
}

void getActionForRoom(RoomData *room) {

    unsigned long s = millis();

    // Для формирования правила регилировки надо знать температуру в доме и заданную температуру.
    // Возможно потом добавить и температуру подающего контура
    if(room->room_t.value != 0 && room->req_t.value != 0) { //  && room->boiler_line_t.value != 0 && room->room_line_t.value != 0

        if( room->req_t.value != room->last_req_t || // если температуру изменили прям совсем недавно, но после управляющего воздействия
            room->next_check == 0 ||        // для данной комнаты еще не проверяли
            room->next_check < s) { // либо время после проверки истекло

            // определяем разницу между текущей и нужной температурой
            int8_t temp_dif = room->room_t.value - room->req_t.value;
            room->last_req_t = room->req_t.value;

            if(DEBUG) {
                Serial.print(F("room->room_t.value - room->req_t.value="));Serial.println(temp_dif);
            }

            room->check_time = s;

            if(abs(temp_dif) == 1) {
                room->stop_time = (DURATION_FOR_1_DEGREE * 1000UL);
                room->next_check =  s + (NEXTCHECK_FOR_1_DEGREE*60UL*1000UL); // проверить изменение температуры через 15 минут
            } else if(abs(temp_dif) >= 2 && abs(temp_dif) <= 4 ) {
                room->stop_time = (DURATION_FOR_2_4_DEGREE * 1000UL);
                room->next_check = s + (NEXTCHECK_FOR_2_4_DEGREE*60UL*1000UL); // проверить изменение температуры через 7 минут
            } else if(abs(temp_dif) > 4) {
                room->stop_time = (DURATION_FOR_5_DEGREE * 1000UL);
                room->next_check = s + (NEXTCHECK_FOR_5_DEGREE*60UL*1000UL); // проверить изменение температуры через 3 минут
            } else { // в случае если температура равна заданной
                room->stop_time = 0;
                room->next_check = s + (NEXTCHECK_FOR_1_DEGREE*60UL*1000UL); // проверить изменение температуры через 2 минут
            }
            room->direction = (temp_dif > 0) ? -1 : 1;


            if(DEBUG) {
                Serial.print(F("s="));Serial.println(s);
                Serial.print(F("room->stop_time="));Serial.println(room->stop_time);
                Serial.print(F("room->next_check="));Serial.println(room->next_check);
                Serial.print(F("room->direction="));Serial.println(room->direction);
                Serial.print(F("room->regulator_position="));Serial.println(room->regulator_position);
            }

            // если надо повернуть на большее чем возможно время, поворачиваем на
            // оставшеееся возможное время либо не поворачиваем вовсе


            if(room->direction > 0) { // открываем
                if(DEBUG) {
                    Serial.println(F("In 1 Direction"));
                }
                unsigned long regMaxTime = REGULATOR_MAX_TIME;
                if(!room->was_in_0) {
                    regMaxTime = 2*REGULATOR_MAX_TIME;
                }
                if(room->regulator_position + room->stop_time >= regMaxTime) { // открывание дальше ляжет на ограничитель
                    room->stop_time = regMaxTime - room->regulator_position;
                    room->regulator_position = REGULATOR_MAX_TIME;
                    room->was_in_0 = 1;
                } else {
                    room->regulator_position += room->stop_time;
                }
            } else if(room->direction < 0) { // закрывавем
                if(DEBUG) {
                    Serial.println(F("In -1 Direction"));
                }
                if(room->regulator_position <= room->stop_time) { // если закрытие приведет к косанию ограничителя
                    room->stop_time = room->regulator_position;
                    room->regulator_position = 0;
                    room->was_in_0 = 1;
                } else {
                    room->regulator_position -= room->stop_time;
                }

            }

            if(DEBUG) {
                Serial.print(F("room->stop_time="));Serial.println(room->stop_time);
                Serial.print(F("room->was_in_0="));Serial.println(room->was_in_0);
            }


            room->stop_time += s;

            if(DEBUG) {
                Serial.print(F("room->stop_time="));Serial.println(room->stop_time);
                Serial.print(F("room->next_check="));Serial.println(room->next_check);
                Serial.print(F("room->direction="));Serial.println(room->direction);
                Serial.print(F("room->regulator_position="));Serial.println(room->regulator_position);
            }


            // Записать правило для управления
            if(ETHERNET) {
                prepare_action_rule_http_request(room, s);
            }
        }
    }

}

void getData() {

    // Получаем температуру линии для каждого контура
    for(uint8_t i = 0; i< sizeof(rooms)/sizeof(RoomData*); i++) {
        RoomData *room = rooms[i];
        processTempData(&room->boiler_line_t);
    }

    // так же для основного контура и температуру на улице
    processTempData(&boiler_t);
    processTempData(&out_t);
}

void processTempData(TempData *t_data) {
    // Исключительно для того, чтобы разнести обращения к датчикам по времени
    if (random(0, 1000) > 600) {
        return;
    }

    unsigned long s = millis();
    if((t_data->last_ok != 0 && (abs(s - t_data->last_ok) < GET_DATA))) {
        // Если последнее успешное чтение не такое старое
        return;
    }
    if(t_data->timestamp == 0 || (abs(s - t_data->timestamp) > RETRY_DATA)) {
        t_data->timestamp = s;
        uint8_t error_code = 0;
        if(EMULATE != 1) {
            t_data->value = round(getTempFromDS(t_data->ds_addr, ds));
            if(DEBUG) {
                Serial.print(F("getDataFromDS = ")); Serial.println(t_data->value);
            }
        } else {
            t_data->value = random(4,70);
        }
        t_data->last_ok = s;
    }
}

void processRequest() {

    RxTxMsg *r_msg = (RxTxMsg *) buffer;
    char prefix[20];

    if(Serial) {
      char str[256];
      getStringFromFlash(RECEIVE_STR, prefix);
      message_to_string(prefix, r_msg, str);
      if(DEBUG) Serial.println(str);
    }

    byte message_incorrect = is_message_incorrect(r_msg, THIS_ADDR);
    if(!message_incorrect) {
      int8_t result;
      byte error = process_msg(r_msg, &result);
      if(!error) {
        // reply if everything OK
        RxTxMsg msg = {6, r_msg->src_addr, THIS_ADDR, r_msg->key, result, 0};
        msg.checksum = calc_checksum(&msg);
        delay(500);
        man.transmitArray(sizeof(msg), (char *) &msg);
        if(Serial) {
          char str[256];
          getStringFromFlash(SEND_STR, prefix);
          message_to_string(prefix, &msg, str);
          if(DEBUG) Serial.println(str);
        }
      } else {
          if(DEBUG) {
              Serial.print(F("Msg pr err. code "));
              Serial.println(error, HEX);
          }
      }
    } else {
        if(DEBUG) {
            Serial.print(F("Msg INCORRECT :: "));
            Serial.println(message_incorrect, HEX);
        }
    }
}

byte process_msg(RxTxMsg *msg, int8_t *result) {

    byte error = 0;

    RoomData *room = get_room_by_id(msg->src_addr);

    if(room->room_id != msg->src_addr) {
        error = 2;
        return error;
    }

    switch (msg->key) {
        case ROOM_TEMP:
            *result = update_room_temp(msg, room);
            break;
        case LINE_TEMP:
            *result = update_line_temp(msg, room);
            break;
        case GET_REQ_TEMP:
            *result = get_req_temp(msg, room);
            break;
        case GET_OUT_TEMP:
            *result = get_out_temp(msg, room);
            break;
        case GET_BOILER_TEMP:
            *result = get_boiler_temp(msg, room);
            break;
        case SET_REQ_TEMP:
            *result = set_req_temp(msg, room);
            break;
        default:
            error = 1;
            break;
    }
    return error;
}

byte update_room_temp(RxTxMsg *msg, RoomData *room) {
    Serial.println(F("update_room_temp"));
    room->room_t.value = msg->value;
    return VALUE_ACK;
}

byte update_line_temp(RxTxMsg *msg, RoomData *room) {
    Serial.println(F("update_line_temp"));
    room->room_line_t.value = msg->value;
    return VALUE_ACK;
}

byte get_req_temp(RxTxMsg *msg, RoomData *room) {
 
  return room->req_t.value;
}

byte get_out_temp(RxTxMsg *msg, RoomData *room) {
  return out_t.value;
}

byte get_boiler_temp(RxTxMsg *msg, RoomData *room) {
  return boiler_t.value;
}

byte set_req_temp(RxTxMsg *msg, RoomData *room) {

    Serial.println(F("set_req_temp"));
    room->req_t.value = msg->value;
    if(ETHERNET) {
        char action[10];
        getStringFromFlash(ACTION_SET_REQ_TEMP, action);
        prepare_temp_http_request(action, room->room_id, "", room->req_t.value, 0);
    }
    return VALUE_ACK;
}

RoomData * get_room_by_id(uint8_t room_id) {
  for(int i = 0; i< sizeof(rooms)/sizeof(RoomData*); i++) {
    RoomData *room = rooms[i];
    if(room->room_id == room_id) {
      return room;
    }
  }
}

void getStringFromFlash(const char * src, char * dst) {
    int k = 0;
    for (k = 0; k < strlen_P(src); k++) {
        dst[k] = pgm_read_byte_near(src + k);
        dst[k+1] = '\0';
    }

}

