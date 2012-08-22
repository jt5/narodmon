/*
Скетч для отправки данных температуры на сервис http://narodmon.ru/
Используется плата Ардуино, шилд «Cosmo GSM Connect» и температурный датчик - DS18B20 или DS18S20
К устройству можно подключить до 4-х температурных датчиков.
Для подключения большего кол-ва датчиков необходимо внести изменения в данный скетч.
*/

#include <Narodmon.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <GSM.h>
#include <avr/pgmspace.h>

#define ONE_WIRE_BUS 8 // Номер линии, к которой подключены датчики температуры
#define TEMPERATURE_PRECISION 9

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor_1, sensor_2, sensor_3, sensor_4; // Используем 4 температурных датчика

//ПИН-код! Поменяйте на свой!!! Иначе СИМ карта заблокируется!
const char PIN[] PROGMEM =  "0000";
//Настройки для доступа к GPRS
const char ISP_IP[] PROGMEM =  "home.beeline.ru";
const char LOGIN[] PROGMEM =  "beeline";
//Буфер для считывания IMEI
char IMEI[16];

unsigned int minutes = 0;
unsigned char SendIntervalInMinutes = 5; //Интервал отправки данных в минутах
unsigned char GSM_TASK_flag; //Флаг для активации отправки

volatile unsigned char WDT_wake;
volatile unsigned char SleepTimeInSec = 60;

/*
Обработчик прерываний по срабатыванию охранного таймера
*/
ISR(WDT_vect) {  
	static unsigned char WDT_counter;  // увеличим счетчик прерываний
	if (WDT_counter++ == SleepTimeInSec) { //отсчет минут
		WDT_counter = 0;
		if (minutes++ == SendIntervalInMinutes) {
			minutes = 0;
			GSM_TASK_flag = 1; // Надо выполнить задачу по отправке данных на сервер
		}
	}  
	asm ("WDR");
}

void setup_WDT(void) {
	asm ("CLI");
	asm ("WDR");
	// WDT_counter = 0; 
	MCUSR &= ~(1<<WDRF); //Сброс флага срабатывания
	WDTCSR = (1 << WDCE)|(1<<WDE);
	WDTCSR = (1 << WDP2)|(1 << WDP1)|(1 << WDIE)|(1 << WDIF);  // Настройка сторожевого таймера на период 1 сек. Перевод в режим работы генерирования прерываний
	asm ("SEI"); 
}

void off_WDT(void) {
	asm ("CLI");
	asm ("WDR");
	// WDT_counter = 0;
	MCUSR &= ~(1<<WDRF); //Сброс флага срабатывания
	WDTCSR = (1 << WDCE)|(1<<WDE)|(1 << WDIF); 
	/* Turn off WDT */
	WDTCSR = 0x00;
	asm ("SEI"); 
}

void sys_sleep(void) {
	asm ("CLI");    
	// asm ("WDR");      
	ADCSRA &= ~(1 << ADEN);  // Выключим АЦП  
	SMCR = (1<<SE)|(1<<SM1); // Конфигурируем режим power-down
	//MCUCR |= (1<<BODSE) | (1<<BODS); // Отключаем BOD на время сна
	asm ("SEI");    
	asm ("SLEEP");
}

void sys_wake(void) {
	SMCR = 0x00; // Конфигурируем режим power-down
}

void setup() {
    delay(1000);    
    sensors.begin();
    // Поиск 1-wire датчиков
    sensors.getAddress(sensor_1, 0);
    sensors.getAddress(sensor_2, 1);
    sensors.getAddress(sensor_3, 2); 
    sensors.getAddress(sensor_4, 3);    
    // установка разрядности датчиков
	sensors.setResolution(sensor_1, TEMPERATURE_PRECISION);
	sensors.setResolution(sensor_2, TEMPERATURE_PRECISION);
	sensors.setResolution(sensor_3, TEMPERATURE_PRECISION);
	sensors.setResolution(sensor_4, TEMPERATURE_PRECISION);    
	// опрос всех датчиков на шине
	sensors.requestTemperatures(); 
	GSM_TASK_flag = 1;           
}


void loop() {        
	if (GSM_TASK_flag) { // В обработчике Watchdog таймер установился флаг GSM-задания
		GSM_TASK_flag = 0;
		// Отключаем сторожевой таймер
		off_WDT();   
		// инициализация GSM модема 
		while (GSM.Init(PIN)<0);     
		// Пoлучаем IMEI    
		while (GSM.GetIMEI (IMEI) < 0);
		// Конверитруем в MAC    
		Narodmon.SetDeviceMACbyIMEI(IMEI);
		// считываем температуры, заносим в массив          
		unsigned char SensorsNum = 0; // Для подсчета работающих датчиков
		signed int SensorData;
		if (ReadSensor (sensor_1, &SensorData)) {  
			Narodmon.SetMACnByIndex(SensorsNum, sensor_1);
			Narodmon.SetDATAByIndex(SensorsNum, SensorData);
			SensorsNum++;
		}
		if (ReadSensor (sensor_2, &SensorData)) {  
			Narodmon.SetMACnByIndex(SensorsNum, sensor_2);
			Narodmon.SetDATAByIndex(SensorsNum, SensorData);
			SensorsNum++;
		}
		if (ReadSensor (sensor_3, &SensorData)) {  
			Narodmon.SetMACnByIndex(SensorsNum, sensor_3);
			Narodmon.SetDATAByIndex(SensorsNum, SensorData);
			SensorsNum++;
		}
		if (ReadSensor (sensor_4, &SensorData)) {  
			Narodmon.SetMACnByIndex(SensorsNum, sensor_4);
			Narodmon.SetDATAByIndex(SensorsNum, SensorData);
			SensorsNum++;
		}
		u8 TxCounter = 0;
		
		// попытка отправить данные 
		if (SensorsNum) {
			Narodmon.SetNumSensors(SensorsNum);
			while (GSM.SocketTCPWrite(ISP_IP, LOGIN, LOGIN, PSTR("map.net13.info"), 8283, &Narodmon.TelnetSend) < 0) {
				delay (5000);
				// Если 4 раза неуспешно
				if (TxCounter++ == 4) {
					TxCounter = 0;
					GSM_TASK_flag = 1;
					break; // Выход из цикла отправки
				}
			}
		}
		
		//Усыпляем GSM-модуль
		while (GSM.PowerOFF() < 0);
		// Включаем сторожевой таймер
		setup_WDT();
	}
	// Усыпляем Атмегу
	sys_sleep();
}


signed char ReadSensor (DeviceAddress sensor_addr, signed int* pData) {
	// Считываем с датчика температуры
	sensors.requestTemperatures();
	float tempS = sensors.getTempC(sensor_addr);
	if (tempS != DEVICE_DISCONNECTED) {
		signed int tempS10 = (signed int) (tempS * 10.0);
		*pData = tempS10;
		return 1;
	}
	return 0;
}