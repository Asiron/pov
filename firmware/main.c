#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdlib.h>


#define CLOCK (1<<PB1)
#define DATA (1<<PB2)
#define BLANK (1<<PB5)
#define LATCH (1<<PB6)

#define NUM_TLC5947 5

#define ANGLE 120

volatile uint8_t need_to_run, set_white;
volatile uint8_t j;
volatile uint8_t buffer[120];
volatile uint8_t translated_buffer[180];
volatile uint8_t last_rpm[3];
volatile uint8_t last_rpm_index = 0;
volatile uint8_t angle_choose = 0;


uint8_t buffer_position = 0;

inline void send_led(uint8_t a) {
    SPDR = a;
    // czekaj az sie wysle bufor
    while(!(SPSR & (1<<SPIF)));
}

inline void commit() {
    PORTB |= LATCH | BLANK;
    PORTB &= ~(LATCH | BLANK);
}

inline uint16_t translate(uint8_t a) {
    return (a * 4095) / 255;
}

void send_debug_number_16bit( uint16_t num){
    uint8_t i;
    char num_s[6];
    itoa(num, num_s, 10);
    for (i=0; i<strlen(num_s); ++i) {
        USART_Transmit(num_s[i]);
    }
    USART_Transmit('\n');
    USART_Transmit('\r');
}

void send_debug_number( uint8_t num) {
    uint8_t i;
    char num_s[4];
    itoa(num, num_s, 10);
    for (i=0; i<strlen(num_s); ++i) {
        USART_Transmit(num_s[i]);
    }
    USART_Transmit('\n');
    USART_Transmit('\r');
    
}
void USART_Transmit( uint8_t data ) {
    while ( !( UCSR1A & (1<<UDRE1)))    ;
    UDR1 = data;
}

inline uint8_t calc_rpm_avg(){
    return (last_rpm[0] + last_rpm[1] + last_rpm[2]) / 3 ;
}

void send_translate() {
    
    static uint8_t mode = 1, to_send = 0;
    uint16_t tmp=0;
    uint8_t i=0;
    
    for (i = 0; i < 120; i++) {
        tmp = translate(buffer[i]);
        // wysylka prawych 8 bitow
        if (mode) {
            send_led((uint8_t) (tmp >> 4));
            
            to_send = ((tmp << 12) >> 8);
        } else {
            to_send |= tmp >> 8;
            
            send_led(to_send);
            
            send_led((tmp << 8) >> 8);
        }
        mode = !mode;
    }
}

void send_translated_buffer(){
    uint8_t i;
    for (i=0; i<180; ++i) {
        send_led(translated_buffer[i]);
    }
}

void translate_buffer() {
    static uint8_t mode = 1, to_translate = 0;
    uint16_t tmp=0;
    uint8_t i=0, j=0;
    
    for (i=0, j=0; i < 120; i++, j++) {
        tmp = translate(buffer[i]);
        // wysylka prawych 8 bitow
        if (mode) {
            translated_buffer[j] = (uint8_t) (tmp >> 4);
            to_translate = ((tmp << 12) >> 8);
        } else {
            to_translate |= tmp >> 8;
            translated_buffer[j] = to_translate;
            ++j;
            translated_buffer[j] = ((tmp << 8) >> 8);
        }
        
        mode = !mode;
    }
}

//// Wcisniecie guziczka
//ISR(PCINT1_vect) {
//     if (!(PINJ & (1<<PJ5))) {
//         set_white = 1;
//         for (i = 0; i < 120; i++) {
//             //buffer[i] = (i%3 == j) ? 255 : 0;
//             buffer[i] = 255;
//         }
//         send_translate();
//         commit();
//         _delay_ms(500);
//         for (i = 0; i < 120; i++) {
//             //buffer[i] = (i%3 == j) ? 255 : 0;
//             buffer[i] = 0;
//         }
//         send_translate();
//         commit();
//         _delay_ms(500);
//     
//     } else if ( !(PINJ & (1<<PJ6)) ) {
//         rpm_counter++;
//         //TCNT0
////         set_white = 1;
////         for (i = 0; i < 120; i++) {
////             //buffer[i] = (i%3 == j) ? 255 : 0;
////             buffer[i] = 255;
////         }
////         send_translate();
////         commit();
//     } else {
////         set_white = 0;
////         for (i = 0; i < 120; i++) {
////            //buffer[i] = (i%3 == j) ? 255 : 0;
////            buffer[i] = 0;
////         }
////         send_translate();
////         commit();
//     }
//}

ISR(INT7_vect) {
    last_rpm[last_rpm_index]++;
    TCNT2 = 0;
}

// Odbior danych z uarta
//ISR(USART3_RX_vect)
//{
//    char ReceivedByte;
//    ReceivedByte = UDR3;
//    UDR3 = ReceivedByte;
//}

ISR(TIMER1_COMPA_vect) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        TCNT2 = 0;
        OCR2A = 15625 / ANGLE / calc_rpm_avg();
    }
    
    send_debug_number(calc_rpm_avg());
    
    last_rpm_index++;
    last_rpm_index %= 3;
    
    if (calc_rpm_avg() > 20){
        TCCR2B = (1<<CS12) | (1<<CS10);
    }
    
//    for (i = 0; i < 120; i++) {
//         buffer[i] = (i%3 == 0) ? (buffer[i] == 255 ? 0 : 255 ) : 0;
//    }
//    send_translate();
//    commit();

}

ISR(TIMER2_COMPA_vect) {
    uint8_t i;
    for (i=0; i<180; ++i) {
        if (angle_choose == 0)
            send_led(255);
        else
            send_led(0);
    }
    commit();
    angle_choose = !angle_choose;
}

int main(void)
{
    //sei();
    
    UCSR1B = (1<<RXEN1) | (1<<TXEN1);// | (1<<RXCIE1);// | (1<<TXCIE1); //0x18;      //reciever enable , transmitter enable
    UBRR1H = 0;
    UBRR1L = 8;
    
    // outputy + SS
    DDRB |= (CLOCK | DATA | BLANK | LATCH | (1<<PB0));
    // stan niski
    PORTB &= ~(CLOCK | DATA | BLANK | LATCH);
    
    // SPI
    SPCR = (1<<SPE) | (1<<MSTR); //| (1<<DORD);
    // max predkoscq
    SPSR = (1<<SPI2X);
    
    // blank na wysoki, diody gasna
    PORTB |= BLANK;
    // czekamy dwie sekundy
    //_delay_ms(2000);
    // blank na niski
    PORTB &= ~BLANK;
    
    DDRD |= (1<<PD7);
    DDRJ &= ~(1<<PJ5 | 1<<PJ6);
    
    EIMSK |= (1<<INT6) | (1<<INT7);
    EICRB |= (1<<ISC71) | (1<<ISC70) | (1<<ISC61) | (1<<ISC60);
    
    //First 16-bit timer - 1 interrupt per sec
    TCCR1A = 0;
    TCCR1B |= (1<<CS12) | (1<<CS10) | (1<<WGM12);
    OCR1A = 15625;
    TIMSK1 |= (1<<OCIE1A) | (1<<OCIE1B); //Set TOIE bit
    
    //Second 16-bit timer - 1 interrupt per angle
    TCCR2A = 0;
    TCCR2B |= (1<<WGM12); // choosing Timer mode - CTC
    OCR2A = 15626 / ANGLE;
    TIMSK2 |= (1<<OCIE2A) | (1<<OCIE2B); //Set TOIE bit

    //Enable xmem
    XMCRA |= (1<<SRE);
    //Using whole port C
    XMCRB = 0;
    
    uint8_t i;
    
    for (i = 0; i < 180; i++)
        send_led(0);
    commit();
    
    //Fill buffer with red diodes only
    for (i=0; i < 120; i++) {
        buffer[i] = (i%3==0) ? 255 : 0;
    }
    
    translate_buffer();
    send_translated_buffer();
    
    commit();
    
    uint8_t* temp = malloc(56000);
    uint16_t j;
    


    while (1) {
    
        _delay_ms(1000);
        for (j=0; j<(56000\;p1`int1; ++j) {
            temp[j] = 46;
            send_debug_number_16bit(j);
            USART_Transmit(' ');
            send_debug_number(temp[j]);
            USART_Transmit('\n');
            USART_Transmit('\r');
        }
    }
    
    return 0;
}