/*
Name:		Narodmon library
Version:	0.1
Created:	17.08.2012
Programmer:	Erezeev A.
Production:	JT5.RU

This library is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

typedef unsigned char u8;
typedef signed int s16;
typedef signed char s8;
typedef unsigned int u16;

#define MAX_NUM_SENSORS (4)

typedef struct
{
		u8 MAC_ID[16]; // MAC устройства
		u8 MAC_SENSORS	[MAX_NUM_SENSORS][8]; // MAC устройства
		s16 DATA_SENSORS	[MAX_NUM_SENSORS]; // MAC датчиков
		u8 NUM_SENSORS; //Число датчиков для отправки	
} tNarodmonData;
	
class NarodmonClass{
public:	
	static void begin();
	static void end();	
	static void SetDeviceMACbyIMEI (char* pIMEI); // Задать MAC устройства
	static void SetNumSensors (u8 Num);	// Задать кол-во датчиков для отправки
	static void SetMACnByIndex ( u8 Index, u8* pMACn);
	static void SetDATAByIndex ( u8 Index, s16 Data);
	static void TelnetSend ( unsigned char (*PutSocket) (unsigned char));
};

extern NarodmonClass Narodmon;
