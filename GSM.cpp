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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <math.h>
#define F_CPU     16000000UL     /**< Системная частота, МГц */
#include <util/delay.h>

#include "GSM.h"
#include <string.h>

#define PD7	(7)
#define PD6	(6)
/*
static char RX_LINE1[50];
static char RX_LINE2[50];
*/
#define pause256 (256)
#define pause64 (64)
volatile u8 IncomingCall;
char CallerID[16];

#define RX_BUFFER_SIZE	128
#define RX_BUFFER_MASK	(RX_BUFFER_SIZE - 2)
static unsigned char rx_buffer[RX_BUFFER_SIZE];
//! Индекс поиска
static u8 rx_i;
//! Индекс записи в буфер приёма
static u8 rx_wr_i;
//! Флаги переполнения и подтверждения
volatile u8 rx_overflow, rx_ack;

//! Всевозможные строковые ответы, запросы
const char ATE0[] PROGMEM         = "ATE0\r\n";
const char OK[] PROGMEM			  = "OK\r\n";
const char RING[] PROGMEM         = "RING\r";
const char CLIP[] PROGMEM         = "+CLIP: ";
const char CR_LF[] PROGMEM        = "\r";
const char MORE[] PROGMEM         = ">";
const char CPIN[] PROGMEM         = "+CPIN: ";
const char CGATT[] PROGMEM        = "+CGATT: 1";
const char CMTI[] PROGMEM         = "+CMTI: ";
const char CMGR[] PROGMEM         = "+CMGR: ";
const char * searchFor;
const char * searchStrings[9] PROGMEM         = {OK, RING, CLIP, CR_LF, MORE, CPIN, CGATT, CMTI, CMGR};

static u8 rx_i_CMTI;
volatile u8 NewSMS_index, NewSMS_index_;
static unsigned char searchStr;

#define OK_     0
#define RING_   1
#define CLIP_   2
#define CRLF_   3
#define MORE_   4
#define CPIN_	5
#define CGATT_	6
#define CMTI_	7
#define CMGR_	8

const char * searchCMTI;

char i2a_buf[32];

static u8 putchar(unsigned char send_char)
{
while (!(UCSR0A & 0x20));
UDR0 = (unsigned char) send_char;
_delay_us(8);
return(send_char);
}

void UART0_rx_reset( void )
{

    UCSR0B &= ~(1<<RXCIE0);
    rx_i = rx_wr_i = 0;
    rx_overflow = rx_ack = 0;
    rx_buffer[ rx_wr_i ] = '\0';
}



void UART0_rx_on( void )
{
    UCSR0B |= ( 1 << RXCIE0 );
}


void UART0_rx_off( void )
{

    UCSR0B &= ~( 1 << RXCIE0 );
}


void UART0_setSearchString( unsigned char Response )
{
    UCSR0B &= ~( 1 << RXCIE0 );
	searchFor = (const char*)pgm_read_word(&searchStrings[Response]);
    searchStr = Response;
    rx_i = 0;
}

static u8 getchar( void )
{
	while ( !(UCSR0A & (1<<RXC0)) )
	{
		
	}
	;
return UDR0;
}

/**
	Парсинг выражений в кавычках
	char* Src - указатель на строку источник
	char* Dst - указатель на область памяти куда копируем выражение в кавычках
*/
static void ParseQuotes (char* Src, char* Dst)
{
volatile u8 f = 0;
char c;
	while (c = *Src++)
	{
		if (f == 0)
		{
			if (c == 34) 
			{
				f = 1;
				continue;
			}
			else continue; 						
		}
		else 
		{					
			if (c == 34) 
			{
				break;
			}										
			else						
			{
				*Dst++ = c;
				//putchar(c);
			}
		}					
	}
	*Dst = 0x00;									
}


/************************************************************************/
/* Обработчик прерываний по приему нового символа по UART    
/* Выполняет сравнение с шаблоном из FLASH "на лету"                                                                 */
/************************************************************************/
ISR(USART_RX_vect)
{
    char data;
	static u8 WaitClip;	//флажок - был "RING", ждём "+CLIP:"
	static u8 CMTI_received;
	char * Src;
    data = UDR0;
	if (!data) return;
    rx_buffer[ rx_wr_i++ ] = data; //Положили новый символ в буфер
    if( rx_wr_i > RX_BUFFER_MASK ) //Если буфер переполнен, продолжим сначала
    {
        rx_wr_i = 0;
        rx_overflow = 1;
/*        UCSR0B &= ~( 1 << RXCIE0 );*/
    }
	
    if( pgm_read_byte(&searchFor[rx_i]) == data )
    {
        rx_i++;
        if( !pgm_read_byte(&searchFor[rx_i]) )
        {
            rx_i = 0;
            if( searchStr == RING_ )
            {
				searchFor = (const char*)pgm_read_word(&searchStrings[CLIP_]);                
                searchStr = CLIP_;
				rx_wr_i = 0;
            }
			else if ( searchStr == CLIP_ )
			{
				searchFor = (const char*)pgm_read_word(&searchStrings[CRLF_]);                
                searchStr = CRLF_;
				rx_wr_i = 0;
				WaitClip = 1;
			}				
            else
            {
				rx_buffer[ rx_wr_i] = 0x00;
				if (WaitClip)
				{
					 WaitClip = 0;
					 ParseQuotes((char*)rx_buffer, CallerID);
					 IncomingCall = 1;
				}				
				if (CMTI_received)		
				{
					CMTI_received = 0;
					//Приняли оповещение:"+CMTI: <mem>,<index>" целиком
					Src = (char*)rx_buffer;
					while (*Src++ != ',');
					// Парсим индекс
				    int i, n;
					n = 0;
					while (*Src)
					{
						if (( *Src >= '0' ) && ( *Src <= '9' ) ) 
						{
							n = 10*n + ( *Src - '0' );
						}
						Src++;
					}
					NewSMS_index = n;
				}
                rx_ack = 1;
                UCSR0B &= ~( 1 << RXCIE0 );
            }
        }
    }
	else if ( pgm_read_byte(&searchCMTI[rx_i_CMTI]) == data )
	{
		rx_i_CMTI++; rx_i = 0;
		if( !pgm_read_byte(&searchCMTI[rx_i_CMTI]) )
		{
			//Приняли шаблон: "+CMTI: " целиком
			rx_i_CMTI = 0;
			searchFor = (const char*)pgm_read_word(&searchStrings[CRLF_]);                
            searchStr = CRLF_;
			rx_wr_i = 0;
			CMTI_received = 1;
			// Ждём конца строки
		}
	}
    else
    {
        rx_i = 0; rx_i_CMTI = 0;
    }	
}


static void FlushRX( void )
{
u8 dummy;
while ( UCSR0A & (1<<RXC0) ) dummy = UDR0;
}

void GSMClass::begin()
{
	DDRD |= (1 << 7);		
	UCSR0B = 0x00;
	UCSR0A = 0x00;
	UCSR0C = 0x06;
	UBRR0L = 25; //38400 low speed ( @ 16 MHz)
	UBRR0H = 0x00;
    UCSR0B = ( 1 << TXEN0 )|( 1 << RXEN0 ); 
	sei();	
}

void UART0_init(void)
{
	DDRD |= (1 << 7);
	UCSR0B = 0x00;
	UCSR0A = 0x00;
	UCSR0C = 0x06;
	UBRR0L = 25; //38400 low speed ( @ 16 MHz)
	UBRR0H = 0x00;
    UCSR0B = ( 1 << TXEN0 )|( 1 << RXEN0 ); 
    UART0_rx_reset();
}



s8 UART0_check_acknowledge( u16 pause )
{
    //! Local variables
    static u16 i, ii;

    for( i = 0; ( rx_ack == 0 ) && ( i < 65535 ); i++ ) //Цикл ожидания
    {
        for( ii = 0; ii < pause; ii++ )
        {
            asm("nop"); // Тратим энергию в пустую
        }
    }

    if( rx_ack > 0 ) 
    {
        rx_ack = 0; 
        return 1;
    }

    else
    {
        UART0_rx_off( );
        UART0_rx_reset( );
        return 0;
    }
}

void GSMClass::end()
{
}		


void GSMClass::WriteStr(char* s)
{
	while (*s) putchar(*s++);		
}

void GSMClass::WriteStr_P(const char* s)
{
	while (pgm_read_byte(s))
    putchar(pgm_read_byte(s++));
}

// Ждем ответ и возвращаем указатель на него в RAM
void GetResponse(char * pDst)
{
	u8 c, i, f = 0;
	while ((c = getchar()) && (i < 100))
	{
		if ((c != 0x0A) && (c != 0x0D)) 
		{ // Если не <CR> и не <LF>, то накапливаем в буфер
			*pDst++ = c;
			//putchar(c);
			i++;
			f++;
		}			
		if (/*(c == 0x0A)||*/(c == 0x0D))
		{ //если пришел <CR>			
			if (f > 1) break; // выходим, если принимали хоть один полезный символ
		}			
	}	
	*pDst = 0x00;
}

/**************************************************************************
* Инициализация модема 
* ВХОД: char* PIN  - Ссылка на строку с PIN-кодом в качестве параметра
* ВОЗВРАТ:	1  если инициализация прошла нормально,
*			отрицательное - если произошла ошибка
***************************************************************************/		

s8 GSMClass::Init(const char* PIN)		
{	
	DDRD |= (1<<PD7)|(1<<PD6);
	PORTD &= ~(1<<PD6);
	PORTD |= (1<<PD7);
	_delay_ms(500);
	PORTD &= ~(1<<PD7);
	_delay_ms(500);
	PORTD |= (1<<PD6);		
	_delay_ms(500);
	PORTD &= ~(1<<PD6);	
	_delay_ms(500);	
	
	UART0_init();
	GSM.WriteStr_P(PSTR("AT\r\n"));
//	_delay_ms(100);
	GSM.WriteStr_P(PSTR("ATE0\r\n"));
	_delay_ms(500);
	GSM.WriteStr_P(PSTR("AT+IFC= 0,0\r\n"));
//	_delay_ms(500);
	
//	searchFor = (const char*)pgm_read_word(&searchStrings[1]);
//	 GSM.WriteStr_P(searchFor);
	sei();
	do 
	{
		UART0_rx_reset( );
		UART0_setSearchString( OK_ );
//		_delay_ms(500);
		GSM.WriteStr_P(PSTR("AT\r\n"));
		UART0_rx_on( );
	}	while( UART0_check_acknowledge( pause64 ) <= 0 );
	
	
    UART0_rx_reset( );
    UART0_setSearchString( OK_ );
//	_delay_ms(500);
    GSM.WriteStr_P(PSTR("ATE0\r\n"));
	sei();
    UART0_rx_on( );
	if( UART0_check_acknowledge( pause64) > 0 )
	{		
		UART0_rx_reset( );
//		_delay_ms(500);
		UART0_setSearchString( CPIN_ );
		GSM.WriteStr_P(PSTR("AT+CPIN?\r\n"));
		UART0_rx_on( );
		if( UART0_check_acknowledge( pause256 ) > 0 )
		{
			//Пришел ответ "+CPIN:"
			UART0_setSearchString( OK_ );
			UART0_rx_on( );
			if( UART0_check_acknowledge( pause64 ) > 0 )
			{// Пришел ответ OK				
				if (strstr_P((char*)rx_buffer, PSTR("SIM PIN")))
				{
					// Необходимо ввести PIN CODE
					UART0_rx_reset( );
					//_delay_ms(500);
					UART0_setSearchString( OK_ );
					GSM.WriteStr_P(PSTR("AT+CPIN=\""));
					GSM.WriteStr_P(PIN);
					GSM.WriteStr_P(PSTR("\"\r\n"));				
					UART0_rx_on( );
					if( UART0_check_acknowledge(pause256 ) > 0) 
					{
						// Пин-код верный					
						goto L_INIT;
					}
					else return -3;
				}
				else if (strstr_P((char*)rx_buffer, PSTR("READY")))				
				{
					// Pin вводить не надо
					goto L_INIT;
				}
				else return -2;												
			}				
			else return -2;
		}
		else return -2;
	}
	else return -1;
L_INIT:
/*
	Патч: проверим, надо ли регистрироваться в сети?
*/
		if (CheckStatus() == 1) goto			L_INIT_01;
			UART0_setSearchString( OK_ );
			UART0_rx_reset( );
			//_delay_ms(500);		
			GSM.WriteStr_P(PSTR("AT+CGATT=1\r\n"));
			UART0_rx_on( );
			if( UART0_check_acknowledge( 3840 ) > 0 )
			{
				// Запрос СМС центра
	//			GSM.WriteStr_P(PSTR("AT+CSCA?\r\n"));
	//			_delay_ms(1000);				
				//Инициализируем текстовый режим СМС
L_INIT_01:			
					UART0_setSearchString( OK_ );
					UART0_rx_reset( );
					//_delay_ms(500);
				GSM.WriteStr_P(PSTR("AT+CMGF=1\r\n"));
				UART0_rx_on( );
				if( UART0_check_acknowledge( pause64 ) > 0 )
			{
				UART0_rx_reset( );
				//_delay_ms(500);
				GSM.WriteStr_P(PSTR("AT+CLIP=1\r\n")); 
				UART0_rx_on( );
				if( UART0_check_acknowledge( pause64 ) > 0 )										
				{
					return 1;
				}
			} else return -4;						
		}else return -3;		
}


void GSMClass::WaitCall(void)
{
	UART0_rx_reset( );
	UART0_setSearchString( RING_ );
	UART0_rx_on( );           	        	
}

s8 GSMClass::TerminateCall(void)
{
	UART0_rx_reset( );
    UART0_setSearchString( OK_ );
//	_delay_ms(1000);
    GSM.WriteStr_P(PSTR("ATH0\r\n"));
    UART0_rx_on( );
	if( UART0_check_acknowledge( pause256 ) > 0 )
	{
		return 1;
	}
	else return -1;	
}


s8 GSMClass::CheckStatus(void)
{
	GSM.WriteStr_P(PSTR("AT\r\n"));
	_delay_ms(500);
	UART0_rx_reset( );		
	UART0_setSearchString( CGATT_ );
	GSM.WriteStr_P(PSTR("\r\nAT+CGATT?\r\n"));	
	UART0_rx_on( );
	if( UART0_check_acknowledge( 63 ) > 0 )
		{	//Пришел ответ "+CGATT: 1", ждем теперь "OK"	
			UART0_rx_reset( );
			UART0_setSearchString( OK_ );
			UART0_rx_on( );		
			if( UART0_check_acknowledge( pause256 ) > 0 )
			{
				//Пришел OK
			return 1;
			}
			else
			{				
				// нет регистрации
				return 0;	
			}	
		}
		else 
		{
			//_delay_ms(500); // время на OK от предыдущего запроса
			return -1;	// ошибка связи	
		}
}	

const char CTRLZ[2] PROGMEM =  {26, 00};

/****************************************************
Отправка СМС с текстом во FLASH
*****************************************************/
s8 GSMClass::SendSMS_P (const char* Num, const char *Txt)
{
	GSM.WriteStr_P(PSTR("AT\r\n"));
	_delay_ms(500);
	UART0_rx_reset( );
	UART0_setSearchString( MORE_ );
	_delay_ms(1000);
	UART0_rx_on( );
	GSM.WriteStr_P(PSTR("AT+CMGS=\""));
	GSM.WriteStr_P(Num);
	GSM.WriteStr_P(PSTR("\"\r\n"));	
	if( UART0_check_acknowledge( pause256 ) > 0 )
	{
		UART0_rx_reset( );
		UART0_setSearchString( OK_ );
		_delay_ms(1000);		
		UART0_rx_on( );
		GSM.WriteStr_P(Txt);
		GSM.WriteStr_P(CTRLZ);
		GSM.WriteStr_P(PSTR("\"\r\n"));			
		if( UART0_check_acknowledge( pause256) > 0 )
		{
			return 1;
		}
		else return -3;						
	}
	else return -2;
}	

/****************************************************
Отправка СМС с текстом из SRAM
*****************************************************/
s8 GSMClass::SendSMS (const char* Num, char *Txt)
{
	GSM.WriteStr_P(PSTR("AT\r\n"));
	_delay_ms(500);
	UART0_rx_reset( );
	UART0_setSearchString( MORE_ );
	_delay_ms(1000);
	UART0_rx_on( );
	GSM.WriteStr_P(PSTR("AT+CMGS=\""));
	GSM.WriteStr_P(Num);
	GSM.WriteStr_P(PSTR("\"\r\n"));	
	if( UART0_check_acknowledge( pause256 ) > 0 )
	{
		UART0_rx_reset( );
		UART0_setSearchString( OK_ );
		_delay_ms(1000);		
		UART0_rx_on( );
		GSM.WriteStr(Txt);
		GSM.WriteStr_P(CTRLZ);
		GSM.WriteStr_P(PSTR("\"\r\n"));			
		if( UART0_check_acknowledge( pause256 ) > 0 )
		{
			return 1;
		}
		else return -3;						
	}
	else return -2;
}	

void GSMClass::WaitSMS(void)
{
	UART0_rx_reset( );
	searchCMTI = (const char*)pgm_read_word(&searchStrings[CMTI_]);
//	UART0_setSearchString( RING_ );
	UART0_rx_on( );           	        	
}


void put_integer( s16 i )
{

    //! Local variables
    int ii;
    unsigned char int_buf[5];

    if (i < 0)                                              //Integer is negative
    {
        i = -i;                                             //Convert to positive Integer
        putchar('-');                                   //Print - sign
    }

    for (ii=0; ii < 5; )                                    //Convert Integer to char array
    {
        int_buf[ii++] = '0'+ i % 10;                        //Find carry using modulo operation
        i = i / 10;                                         //Move towards MSB
    }
    do{ ii--; }while( (int_buf[ii] == '0') && (ii != 0) );  //Remove leading 0's
    do{ putchar( int_buf[ii--] ); }while (ii >= 0);     //Print int->char array convertion
}


static void i2a( u16 i, char* pOut_buf )
{

    //! Local variables
    int ii;
    char int_buf[5];
    
    for (ii=0; ii < 5; )                                    //Convert Integer to char array
    {
        int_buf[ii++] = '0'+ i % 10;                        //Find carry using modulo operation
        i = i / 10;                                         //Move towards MSB
    }
    do{ ii--; }while( (int_buf[ii] == '0') && (ii != 0) );  //Remove leading 0's
    do
	{ 
		*pOut_buf++= int_buf[ii--];
	} while (ii >= 0);
	*pOut_buf = 0x00;
}



/*
Чтение СМС и копирование текста
*/
s8 GSMClass::ReadSMS(u8 index, char* Dst)
{
	char* Src;
    UART0_rx_reset( );
    UART0_setSearchString( CMGR_ );
    GSM.WriteStr_P(PSTR("AT+CMGR="));
    put_integer( (s16) index );
    GSM.WriteStr_P(PSTR("\r\n"));		
    UART0_rx_on( );
	if( UART0_check_acknowledge(pause256 ) > 0 )
	{
		// Пришел ответ "+CMGR: "
		UART0_rx_reset( );
		UART0_setSearchString( CRLF_ ); // Ждём перевод строки	
		UART0_rx_on( );
		if( UART0_check_acknowledge(pause256 ) > 0 )
		{
			Src = (char*)rx_buffer;
			while (*Src++ != ',');
			//Пришла первая порция ответа - из неё нужно вытащить номер отправителя СМС			
			ParseQuotes(Src, CallerID);
			UART0_rx_reset( );
			UART0_setSearchString( CRLF_ ); // Ждём конца СМС
			UART0_rx_on( );		
			if( UART0_check_acknowledge(pause256 ) > 0 )
			{
				//Теперь, копируем текст СМС в буфер
				Src = (char*)rx_buffer;
				while (*Src)
				{
					if ((*Src != '\r') && (*Src != '\n'))
					{
						*Dst++ = *Src;
					}
					Src++;
				}
				*Dst = 0x00; //Конец строки
				return 1;
			}else return -3;		
		}else return -2;
	} else return -1;			
}

s8 GSMClass::NewSMSindic(void)
{
	GSM.WriteStr_P(PSTR("AT+CNMI=1,1,,,1\r\n"));
	UART0_rx_reset( );
	UART0_setSearchString( OK_ ); 
	UART0_rx_on( );		
	if( UART0_check_acknowledge(pause256 ) > 0 )
	{
		return 1;
	}
	else return -1;			
}	

s8 GSMClass::DeleteAllSMS(void)
{
	NewSMS_index_ = NewSMS_index = 0;
	GSM.WriteStr_P(PSTR("AT+CMGD=0,5\r\n"));	
	UART0_rx_reset( );
	UART0_setSearchString( OK_ ); 
	UART0_rx_on( );		
	if( UART0_check_acknowledge( pause256) > 0 )
	{
		return 1;
	}
	else return -1;			
}

s8 GSMClass::CheckSMS(void)
{
  if (NewSMS_index_ != NewSMS_index)
  {
	  NewSMS_index_ = NewSMS_index;
	  return 1;
  }
  else return 0;
}



s8 GSMClass::SocketTCPWrite(const char* ISP_IP, const char* LOGIN, const char* PWD, const char* SERVER_IP, unsigned int SERVER_PORT, void (*CALLBACK_FUNC)(unsigned char(*SocketWr)(unsigned char)))
{
	GSM.WriteStr_P(PSTR("AT\r\n"));
	_delay_ms(500);
	UART0_rx_reset( );
	UART0_setSearchString( OK_ );
	GSM.WriteStr_P(PSTR("\r\nAT$NOSLEEP=1\r\n"));
	UART0_rx_on( );	
	if( UART0_check_acknowledge(63) <= 0 )
	{
		return -5;
	}
	UART0_rx_reset( );
	UART0_setSearchString( OK_ );
	GSM.WriteStr_P(PSTR("\r\nAT+CGDCONT=1,\"IP\",\""));
	GSM.WriteStr_P(ISP_IP);
	GSM.WriteStr_P(PSTR("\"\r\n"));
	UART0_rx_on( );	
	if( UART0_check_acknowledge(2560) > 0 )
	{
		UART0_rx_reset( );
//		_delay_ms(500);
		UART0_setSearchString( OK_ );
		GSM.WriteStr_P(PSTR("\r\nAT%CGPCO=1,\"PAP,"));
		GSM.WriteStr_P(LOGIN);
		putchar(',');
		GSM.WriteStr_P(PWD);
		GSM.WriteStr_P(PSTR("\",1\r\n"));
		UART0_rx_on( );
		if( UART0_check_acknowledge( pause256) > 0 )
		{
			UART0_rx_reset( );
			//_delay_ms(500);
			UART0_setSearchString( OK_ );
			GSM.WriteStr_P(PSTR("\r\nAT$DESTINFO=\""));
			GSM.WriteStr_P(SERVER_IP);			
			GSM.WriteStr_P(PSTR("\",1,"));
			i2a(SERVER_PORT, i2a_buf);
			char* pi2a = i2a_buf;
			while (*pi2a) putchar(*pi2a++);
			GSM.WriteStr_P(PSTR(",1\r\n"));
			UART0_rx_on( );
			if( UART0_check_acknowledge( pause64) > 0 )
			{
				UART0_rx_reset( );
				//_delay_ms(500);
				UART0_setSearchString( OK_ );
				GSM.WriteStr_P(PSTR("\r\nAT$TIMEOUT=1000\r\n"));
				UART0_rx_on( );
				if( UART0_check_acknowledge(63) <= 0 ) return -7;												
				UART0_rx_reset( );
				//_delay_ms(500);
				UART0_setSearchString( OK_ );
				GSM.WriteStr_P(PSTR("\r\nATD*97#\r\n"));
				UART0_rx_on( );
				if( UART0_check_acknowledge(2560) > 0 )
				{
					// Вот тут и записываем в сокет данные
					(CALLBACK_FUNC)(&putchar);
//					while (*DATA) putchar(*DATA++);
					_delay_ms(1000);
					GSM.WriteStr_P(PSTR("+++"));//Разрываем соединение
					_delay_ms(1000);																				
					GSM.WriteStr_P(PSTR("ATH0")); 
					_delay_ms(2000);
				} 
				else
				{
					_delay_ms(1000);
					GSM.WriteStr_P(PSTR("+++"));//Разрываем соединение
					_delay_ms(1000);																				
					GSM.WriteStr_P(PSTR("ATH0")); 
					_delay_ms(2000);
					return -4;
				}					
			}else return -3;			
		} else return -2;		
	} else return -1;	
	return 1;
}

s8 GSMClass::GetIMEI (char* IMEIbuf)
{
	GSM.WriteStr_P(PSTR("AT\r\n"));
	_delay_ms(500);
	UART0_rx_reset( );		
	UART0_setSearchString( OK_ );
	GSM.WriteStr_P(PSTR("\r\nAT+GSN\r\n"));	
	UART0_rx_on( );
	if( UART0_check_acknowledge( 63 ) > 0 )
	{
		char* pSrc = (char*)rx_buffer;
		u8 n = 0; u8 k = 0;
			while ((n < 15) && (k < 64))
			{							
				if ((*pSrc >= '0') && (*pSrc <= '9')) {*IMEIbuf++ = *pSrc; n++;}
				else if (*pSrc == 'O') break;
				*pSrc++; k++;
			}
			if (n == 15) {*pSrc =0x00; return 1;}
			else return -2;		
	}
	else return -1;
}

s8 GSMClass::PowerOFF()
{
	GSM.WriteStr_P(PSTR("AT\r\n"));
	_delay_ms(500);
	UART0_rx_reset( );		
	UART0_setSearchString( OK_ );
	GSM.WriteStr_P(PSTR("\r\nAT$POWEROFF\r\n"));
	UART0_rx_on( );
	if( UART0_check_acknowledge( 256 ) > 0 )
	{
		return 1;
	}		
	else return -1;
}