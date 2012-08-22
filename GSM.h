/*
Name:		GSM library for Arduino
Version:	1.0
Created:	19.03.2012
Updated:	21.08.2012
Programmer:	Erezeev A.
Production:	JT5.RU
Source:		https://github.com/jt5/GSM

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

class GSMClass{
public:
	static void begin();
	static void end();	
	static void WriteStr(char *String);
	static s8 Init(const char *PIN);	
	static void WriteStr_P(const char *PString);
	static void WaitCall(void);
	static void WaitSMS(void);
	static s8 TerminateCall(void);
	static s8 SendSMS_P (const char* Num, const char *Txt);
	static s8 SendSMS (const char* Num, char *Txt);
	static s8 ReadSMS(u8 index, char* Dst);
	static s8 CheckStatus(void);
	static s8 NewSMSindic(void);
	static s8 DeleteAllSMS(void);
	static s8 CheckSMS(void);
	static s8 SocketTCPWrite(const char* ISP_IP, const char* LOGIN, const char* PWD, const char* SERVER_IP, unsigned int SERVER_PORT, void (*CALLBACK_FUNC)(unsigned char(*SocketWr)(unsigned char)));
	static s8 GetIMEI (char* IMEIbuf);
	static s8 PowerOFF();
};

extern volatile u8 IncomingCall;
extern volatile u8 NewSMS_index;
extern char CallerID[16];
extern GSMClass GSM;