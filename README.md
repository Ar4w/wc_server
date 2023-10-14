# wc_server
Water Counter server )

ESP32 собирет показания со счетчиков воды и автоматически отправляет их в МОСОБЛЕИРЦ
Мониторинг и управление процессом осуществляется через телеграм бот.

Функции
- Считает импульсы от приборов учета оснащенных герконом
- Счетчики хранятся во внутреннем Flash (библиотека Preferences)
- Подключается к одной из доступных WiFi сетей (библиотека WiFiMulti)
- Оснащен telegram ботом (библитека FastBot)
- Подключается к сайту mosoblerc.ru и забирает оттуда текущие данные о ваших счетчиках
    - id приборов
    - ранее переданные показания
    - дата плановой поверки
    - даты когда можно подавать показания
- Время синхронизирует по NTP
- В нужный день месяца отправляет показания в мособлеирц
- Не передает показания если значения превысили месячный лимит
- За три месяца до срока поверки начинает слать напоминания
- начальные значения счетчиков выставляются через telegram

# Подготовка
Чтобы настроить программу, нужно отредактировать файл wc_server.ino
Все строки которые там нужно отредактировать помечены фразой //Edit me

```
const char* ssid1              = "ssid1";                        // Edit me
const char* ssid2              = "ssid2";                        // Edit me
const char* password           = "wifi_password";                // Edit me
const char* ntpServer1         = "ntp3.vniiftri.ru";
const char* ntpServer2         = "time.nist.gov";
const long  gmtOffset_sec      = 10800;
const int   daylightOffset_sec = 0;

#define BOT_TOKEN "your:bot_token"                               // Edit me
#define CHAT_ID "yourchatid"                                     // Edit me

FastBot bot(BOT_TOKEN);

#define HOT_PIN 23
#define COL_PIN 18

#define HOT_ID  1111111                                              // Edit me
#define COL_ID  2222222                                              // Edit me

Mosobleirc eirc( "eirc_login", "eirc_password", HOT_ID, COL_ID, bot);  // Edit me
```
Здесь вам нужно настроить ssid своих сетей и пароль от них. У меня на обоих ssid одинаковаый пароль. Если у вас не так, то отредактируйте еще и функции в которой инициализируется wifi

Зарегистрируйте своего telegram бота и узнайте id. Как это делается? в сети масса материалов.

Далее обратите внимание на определение пинов для подключения счетчиков горячей и холодной воды

```
#define HOT_PIN 23
#define COL_PIN 18
```

Теперь нам нужно узнать ID своих счетчиков. Для этого можно использовать маленький python скрит get_data_from_eirc.py который подключится к мособлеирц и в json покажет содержимое вашего профиля.  Нам оттуда нужны только значения id. Вот как они там выглядят

```
        "meter": {
            "id": 222222,
            "type": "ColdWater",
```
Значения для нужных счетчиков нужно записать соотвественно сюда
```
#define HOT_ID  1111111                                              // Edit me
#define COL_ID  2222222                                              // Edit me
```
Ну и конечно отредактировать логин и пароль от профиля mosobleirc
```
Mosobleirc eirc( "eirc_login", "eirc_password", HOT_ID, COL_ID, bot);  // Edit me
```
Теперь можно компилировать и прошивать.

# Отладка
Для отладки в вашем IDE нужно указать необходимый уровень логирования в параметре Core_Debug_Level.  В Arduino IDE это находится в меню Tools

# Установка/корректировка начальных значений счетсиков
Эта программа написана для счетчиков у которых импульсы посылаются на каждые 10 литров воды
![89050143](https://github.com/Ar4w/wc_server/assets/89636312/1aa8a53f-060e-4f4f-a7b2-7e357ae351c8)
То есть мы считаем десятки литров но передавать в мособлерц будем кубометры
Для установки начальных значений счетчиков используется команда бота /set
```
set <hot> <col> [<data>]
```
Здесь hot и col это целые числа в десятках литров.
Например, если прибор сейчас показывает значение 00012,345 (345 красные цифры единиц литров) нам нужно отбросить последнюю красную цифру и запятую. Таким образом нужно передать боту только 1234.  При этом, в мособлеирц бот отправить только количество целых кубометров, т.е. 12


# Команды бота
Если мы все правильно настроили, то при старте мы получим сообщение Started.
Теперь можно воспользоваться командой /help
Если вы подключили бот не к группе а просто указали свой персональный id, тогда слэш перед командами можно не вставлять

