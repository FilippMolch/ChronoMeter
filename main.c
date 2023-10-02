#define F_CPU 16000000UL

#include <stdio.h>
#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <util/delay.h>

#include <lib/ssd1306.h>

#define PRECISION 0.00000000000001
#define MAX_NUMBER_STRING_SIZE 32

#define TIMER_START TCCR0B |= (1 << CS02); \
				    _micros = 0;
#define TIMER_STOP TCCR0B ^= (1 << CS02); \
				   _micros = 0;

static void USART_write_byte(uint8_t data, FILE *stream);
static FILE std_o = FDEV_SETUP_STREAM(USART_write_byte, NULL, _FDEV_SETUP_WRITE);

void USART_init(){
	
	//115k BOD
	UBRR0 = 8;
	
	//ENABLE RX TX
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}

static void USART_write_byte(uint8_t data, FILE *stream){
	
	if (data == '\n'){
		data = 0x0A;
		while (!(UCSR0A & (1<<UDRE0)));
		UDR0 = data;
		
		data = 0x0D;
		while (!(UCSR0A & (1<<UDRE0)));
		UDR0 = data;
	}
	
	//Waiting for work bus
	while (!(UCSR0A & (1<<UDRE0)));
	
	UDR0 = data;
	
}

char * dtoa(char *s, double n) {
	// handle special cases
	if (isnan(n)) {
		strcpy(s, "nan");
		} else if (isinf(n)) {
		strcpy(s, "inf");
		} else if (n == 0.0) {
		strcpy(s, "0");
		} else {
		int digit, m, m1;
		char *c = s;
		int neg = (n < 0);
		if (neg)
		n = -n;
		// calculate magnitude
		m = log10(n);
		int useExp = (m >= 14 || (neg && m >= 9) || m <= -9);
		if (neg)
		*(c++) = '-';
		// set up for scientific notation
		if (useExp) {
			if (m < 0)
			m -= 1.0;
			n = n / pow(10.0, m);
			m1 = m;
			m = 0;
		}
		if (m < 1.0) {
			m = 0;
		}
		// convert the number
		while (n > PRECISION || m >= 0) {
			double weight = pow(10.0, m);
			if (weight > 0 && !isinf(weight)) {
				digit = floor(n / weight);
				n -= (digit * weight);
				*(c++) = '0' + digit;
			}
			if (m == 0 && n > 0)
			*(c++) = '.';
			m--;
		}
		if (useExp) {
			// convert the exponent
			int i, j;
			*(c++) = 'e';
			if (m1 > 0) {
				*(c++) = '+';
				} else {
				*(c++) = '-';
				m1 = -m1;
			}
			m = 0;
			while (m1 > 0) {
				*(c++) = '0' + m1 % 10;
				m1 /= 10;
				m++;
			}
			c -= m;
			for (i = 0, j = m-1; i<j; i++, j--) {
				// swap without temporary
				c[i] ^= c[j];
				c[j] ^= c[i];
				c[i] ^= c[j];
			}
			c += m;
		}
		*(c) = '\0';
	}
	return s;
}

void timer_init(){
	TCCR0A |= (1 << WGM01);
	OCR0A = 1;
	TIMSK0 |= (1 << OCIE0A);
}

void _setup(){
	
	//pull ups for i2c\interrupt
	DDRC = 0;
	PORTC |= (1 << PORTC4) | (1 << PORTC5);
	
	DDRD = 0;
	PORTD |= (1 << PORTD2);
	
	USART_init();
	stdout = &std_o;

	SSD1306_Init(SSD1306_ADDR);
	SSD1306_ClearScreen();
	SSD1306_Resize(1);
	
	//set external interrupt (change)
	cli();
	
	EICRA |= (1 << ISC00);
	EIMSK |= (1 << INT0);
	
	sei();
}

static volatile uint32_t _micros = 0;

ISR(TIMER0_COMPA_vect){
	_micros += 32;
}

static volatile uint32_t shoot_res = 0;
static volatile uint8_t shoot_trig = 1;
static volatile bool result_trig = false;

ISR(INT0_vect){
	
	if (shoot_trig == 1){
		printf("\nSHOOT START\n");
		TIMER_START
	}
	
	if (shoot_trig == 2){
		printf("SHOOT STOP\n");
		
		shoot_res = _micros;
		result_trig = true;
		
		shoot_trig = 0;
		TIMER_STOP
	}
	
	shoot_trig++;
}

/*
if (shoot_counter >= 1000000.0) {
	double secs = (shoot_counter) / 1000000.0;
	text = (secs);
	shoot_sf = true;
}
else {
	double speeed = 1000000.0 / (shoot_counter);
	text = speeed;
	shoot_sf = false;
}
*/

int main(void){
	_setup();
	timer_init();
	
	while(1){
		if (result_trig){
			float result = (float)shoot_res / 1000000;
			char str[32];
			
			dtoa(str, result);
			printf("\nSHOOT! ");
			printf(str, "\n");
			
			result_trig = false;
		}
	}
	
}
