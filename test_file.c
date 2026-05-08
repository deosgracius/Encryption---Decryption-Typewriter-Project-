#include <msp430fr6989.h>
#include <stdint.h>

// ===== MAX7219 PINS =====
#define DIN BIT2    // P2.2
#define CLK BIT3    // P2.3
#define CS  BIT0    // P2.0

// ===== KEYBOARD =====
#define ROWS 8
#define COLS 8

// ===== ROTOR DEFAULTS -- change and recompile, or override via wizard =====
#define LEFT_ROTOR   1      // 1=I  2=II  3=III  4=IV  5=V
#define CENTER_ROTOR 2
#define RIGHT_ROTOR  3

#define LEFT_START   'A'
#define CENTER_START 'A'
#define RIGHT_START  'A'

// ===== KEYBOARD MAP =====
char keymap[64] = {
    'j','f','1','l','v','v','l','p',
    'k','q','h','t','m','y','e','v',
    'l','y','l','d','j','g','a','v',
    'b','m','i','p','y','r','w','p',
    'c','x','k','s','h','f','z','w',
    'v','n','u','o','t','e','q','p',
    'y','l','m','l','z','v','k','l',
    'v','n','e','t','v','t','m','l'
};

// ===== LED FONT =====
const uint8_t font[26][8] = {
    {0x7C,0x7E,0x13,0x13,0x7E,0x7C,0x00,0x00}, // A
    {0x41,0x7F,0x7F,0x49,0x49,0x7F,0x36,0x00}, // B
    {0x1C,0x3E,0x63,0x41,0x41,0x63,0x22,0x00}, // C
    {0x41,0x7F,0x7F,0x41,0x63,0x3E,0x1C,0x00}, // D
    {0x41,0x7F,0x7F,0x49,0x5D,0x41,0x63,0x00}, // E
    {0x41,0x7F,0x7F,0x49,0x1D,0x01,0x03,0x00}, // F
    {0x1C,0x3E,0x63,0x41,0x51,0x73,0x72,0x00}, // G
    {0x7F,0x7F,0x08,0x08,0x7F,0x7F,0x00,0x00}, // H
    {0x00,0x41,0x7F,0x7F,0x41,0x00,0x00,0x00}, // I
    {0x30,0x70,0x40,0x41,0x7F,0x3F,0x01,0x00}, // J
    {0x41,0x7F,0x7F,0x08,0x1C,0x77,0x63,0x00}, // K
    {0x41,0x7F,0x7F,0x41,0x40,0x60,0x70,0x00}, // L
    {0x7F,0x7E,0x0C,0x18,0x0C,0x7E,0x7F,0x00}, // M
    {0x7F,0x7F,0x06,0x0C,0x18,0x7F,0x7F,0x00}, // N
    {0x3E,0x7F,0x41,0x41,0x7F,0x3E,0x00,0x00}, // O
    {0x41,0x7F,0x7F,0x49,0x09,0x0F,0x06,0x00}, // P
    {0x1E,0x3F,0x21,0x31,0x7F,0x5E,0x00,0x00}, // Q
    {0x41,0x7F,0x7F,0x09,0x19,0x7F,0x66,0x00}, // R
    {0x26,0x6F,0x4D,0x59,0x7B,0x32,0x00,0x00}, // S
    {0x03,0x41,0x7F,0x7F,0x41,0x03,0x00,0x00}, // T
    {0x7F,0x7F,0x40,0x40,0x7F,0x7F,0x00,0x00}, // U
    {0x1F,0x3F,0x60,0x60,0x3F,0x1F,0x00,0x00}, // V
    {0x7F,0x7F,0x30,0x18,0x30,0x7F,0x7F,0x00}, // W
    {0x63,0x77,0x1C,0x08,0x1C,0x77,0x63,0x00}, // X
    {0x07,0x4F,0x78,0x78,0x4F,0x07,0x00,0x00}, // Y
    {0x67,0x73,0x59,0x4D,0x47,0x63,0x71,0x00}  // Z
};

// ===== ROTOR WIRINGS =====
const char rotors[5][27] = {
    "EKMFLGDQVZNTOWYHXUSPAIBRCJ", // Rotor I
    "AJDKSIRUXBLHWTMCQGZNPYFVOE", // Rotor II
    "BDFHJLCPRTXVZNYEIWGAKMUSQO", // Rotor III
    "ESOVPZJAYQUIRHXLNFTGKDCMWB", // Rotor IV
    "VZBRGITYUPSDNHLXAWMJQOFECK"  // Rotor V
};

// ===== REFLECTORS =====
const char reflectors[3][27] = {
    "EJMZALYXVBWFCRQUONTSPIKHGD", // UKW-A
    "YRUHQSLDPXNGOKMIEBFZCWVJAT", // UKW-B (default -- same as original)
    "FVPJIAOYEDRZXWGCTKUQSBNMHL"  // UKW-C
};

// ===== NOTCH POSITIONS (A=0): I=Q(16) II=E(4) III=V(21) IV=J(9) V=Z(25) =====
const int rotor_notches[5] = { 16, 4, 21, 9, 25 };

// ===== RUNTIME STATE -- set by wizard =====
int left_rotor_num   = LEFT_ROTOR;
int center_rotor_num = CENTER_ROTOR;
int right_rotor_num  = RIGHT_ROTOR;

int left_pos   = LEFT_START   - 'A';
int center_pos = CENTER_START - 'A';
int right_pos  = RIGHT_START  - 'A';

int left_start   = LEFT_START   - 'A';  // saved starting positions
int center_start = CENTER_START - 'A';
int right_start  = RIGHT_START  - 'A';

int left_ring   = 0;  // ring settings (0=A, 1=B, ...)
int center_ring = 0;
int right_ring  = 0;

int  reflector_num = 1;     // 0=UKW-A  1=UKW-B  2=UKW-C
char enigma_mode   = 'E';   // 'E'=encrypt  'D'=decrypt  'P'=passthrough

char plugboard[26];         // plugboard substitution table (identity by default)

// ===== PLUGBOARD =====
void plugboard_init(void)
{
    int i;
    for(i = 0; i < 26; i++)
        plugboard[i] = (char)('A' + i);  // identity -- no substitution
}

void plugboard_add(char a, char b)
{
    if(a >= 'a' && a <= 'z') a = (char)(a - 32);
    if(b >= 'a' && b <= 'z') b = (char)(b - 32);

    if(a < 'A' || a > 'Z' || b < 'A' || b > 'Z' || a == b)
        return;

    // only add if neither letter is already paired
    if(plugboard[a - 'A'] != (char)('A' + (a - 'A'))) return;
    if(plugboard[b - 'A'] != (char)('A' + (b - 'A'))) return;

    plugboard[a - 'A'] = b;
    plugboard[b - 'A'] = a;
}

// ===== CIPHER =====
int A2I(char i)
{
    return i - 'A';
}

char I2A(int i)
{
    return (char)(i + 'A');
}

int wrap26(int x)
{
    while(x < 0)   x += 26;
    while(x >= 26) x -= 26;
    return x;
}

// ring parameter added -- set to 0 for no ring offset (same as original)
int rotor_forward(int input, int rotor_num, int position, int ring)
{
    int shifted;
    int wired;
    int output;

    shifted = wrap26(input + position - ring);
    wired   = A2I(rotors[rotor_num - 1][shifted]);
    output  = wrap26(wired - position + ring);

    return output;
}

int rotor_backward(int input, int rotor_num, int position, int ring)
{
    int shifted;
    int i;
    int output;

    shifted = wrap26(input + position - ring);

    for(i = 0; i < 26; i++)
    {
        if(A2I(rotors[rotor_num - 1][i]) == shifted)
        {
            output = wrap26(i - position + ring);
            return output;
        }
    }

    return input;
}

// double-step anomaly -- checks notch positions before stepping
void step_rotors(void)
{
    int cn = rotor_notches[center_rotor_num - 1];
    int rn = rotor_notches[right_rotor_num  - 1];

    if(center_pos == cn)
    {
        left_pos   = (left_pos   + 1) % 26;  // left steps
        center_pos = (center_pos + 1) % 26;  // center double-steps
    }
    else if(right_pos == rn)
    {
        center_pos = (center_pos + 1) % 26;  // center steps
    }

    right_pos = (right_pos + 1) % 26;        // right always steps
}

char encrypt_char(char input)
{
    int  signal;
    char out;

    if(input >= 'a' && input <= 'z')
        input = (char)(input - 32);

    if(input < 'A' || input > 'Z')
        return 0;

    step_rotors();

    input  = plugboard[input - 'A'];       // plugboard input
    signal = A2I(input);

    signal = rotor_forward (signal, right_rotor_num,  right_pos,  right_ring);
    signal = rotor_forward (signal, center_rotor_num, center_pos, center_ring);
    signal = rotor_forward (signal, left_rotor_num,   left_pos,   left_ring);

    signal = A2I(reflectors[reflector_num][signal]);

    signal = rotor_backward(signal, left_rotor_num,   left_pos,   left_ring);
    signal = rotor_backward(signal, center_rotor_num, center_pos, center_ring);
    signal = rotor_backward(signal, right_rotor_num,  right_pos,  right_ring);

    out = I2A(signal);
    return plugboard[out - 'A'];           // plugboard output
}

// ===== DELAY =====
void delay_ms(unsigned int ms)
{
    // 8 MHz DCO: 8000 cycles = 1 ms
    while(ms--)
        __delay_cycles(8000);
}

// ===== MAX7219 =====
uint8_t reverse_bits(uint8_t b)
{
    uint8_t r = 0;
    int i;

    for(i = 0; i < 8; i++)
    {
        r <<= 1;
        if(b & 0x01) r |= 1;
        b >>= 1;
    }

    return r;
}

void max_send(uint8_t address, uint8_t data)
{
    uint16_t packet;
    int i;

    packet = ((uint16_t)address << 8) | data;

    P2OUT &= ~CS;

    for(i = 0; i < 16; i++)
    {
        P2OUT &= ~CLK;
        if(packet & 0x8000) P2OUT |= DIN;
        else                 P2OUT &= ~DIN;
        P2OUT |= CLK;
        packet <<= 1;
    }

    P2OUT |= CS;
}

void max_clear(void)
{
    int row;
    for(row = 1; row <= 8; row++)
        max_send(row, 0x00);
}

void max_init(void)
{
    P2DIR |= DIN | CLK | CS;

    P2OUT |= CS;
    P2OUT &= ~DIN;
    P2OUT &= ~CLK;

    delay_ms(100);

    max_send(0x0F, 0x00);
    max_send(0x0C, 0x01);
    max_send(0x09, 0x00);
    max_send(0x0B, 0x07);
    max_send(0x0A, 0x02);

    max_clear();
}

void display_letter(char letter)
{
    int     row;
    int     index;
    uint8_t data;

    if(letter >= 'a' && letter <= 'z')
        letter = (char)(letter - 32);

    if(letter < 'A' || letter > 'Z')
        return;

    index = letter - 'A';

    for(row = 0; row < 8; row++)
    {
        data = reverse_bits(font[index][row]);
        max_send(row + 1, data);
    }
}

// ===== UART -- eUSCI_A1  P3.4=TX  P3.5=RX  115200 baud
// PuTTY: 115200 baud  8N1  no flow control
// FR6989 uses eUSCI registers (UCA1CTLW0 / UCA1BRW / UCA1MCTLW)
// which are different from F5529 USCI (UCA1CTL1 / UCA1BR0 / UCA1MCTL) =====
void uart_init(void)
{
    // Configure DCO to 8 MHz for reliable UART
    CSCTL0_H  = CSKEY_H;
    CSCTL1    = DCOFSEL_6;                        // 8 MHz
    CSCTL2    = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3    = DIVA__1 | DIVS__1 | DIVM__1;
    CSCTL0_H  = 0;

    // P3.4 = UCA1TXD   P3.5 = UCA1RXD
    P3SEL0   |= BIT4 | BIT5;
    P3SEL1   &= ~(BIT4 | BIT5);

    UCA1CTLW0 = UCSWRST;                          // hold in reset
    UCA1CTLW0|= UCSSEL__SMCLK;                    // SMCLK source
    UCA1BRW   = 4;                                // 8MHz / 115200
    UCA1MCTLW = ((uint16_t)0x55 << 8) | ((uint16_t)5 << 4) | UCOS16;
    UCA1CTLW0&= ~UCSWRST;                         // release reset
}

void uart_putchar(char c)
{
    while(!(UCA1IFG & UCTXIFG));
    UCA1TXBUF = (unsigned char)c;
}

void uart_puts(const char *s)
{
    while(*s) uart_putchar(*s++);
}

// read one char (blocking, echo to terminal, uppercase)
char uart_getchar(void)
{
    char c;
    while(!(UCA1IFG & UCRXIFG));
    c = (char)UCA1RXBUF;
    if(c >= 'a' && c <= 'z') c = (char)(c - 32);
    return c;
}

// read a line with echo and backspace support (uppercases automatically)
void uart_readline(char *buf, int maxlen)
{
    int  i = 0;
    char c;

    while(1)
    {
        while(!(UCA1IFG & UCRXIFG));
        c = (char)UCA1RXBUF;

        if(c == '\r' || c == '\n')
        {
            uart_puts("\r\n");
            break;
        }

        if((c == 0x08 || c == 0x7F) && i > 0)
        {
            i--;
            uart_puts("\x08 \x08");
            continue;
        }

        if(c >= 'a' && c <= 'z') c = (char)(c - 32);

        if(c >= 0x20 && c < 0x7F && i < maxlen - 1)
        {
            buf[i++] = c;
            uart_putchar(c);
        }
    }

    buf[i] = '\0';
}

// ===== SETUP WIZARD =====
// Runs at boot via PuTTY. Sets all cipher parameters before main loop.
void setup_wizard(void)
{
    char buf[32];
    int  valid;
    int  i;
    char a, b;

restart:
    uart_puts("\r\n");
    uart_puts("================================================\r\n");
    uart_puts("   ENIGMA CIPHER -- MSP430F5529\r\n");
    uart_puts("   Setup Wizard\r\n");
    uart_puts("================================================\r\n\r\n");

    // ----- STEP 1: Rotor selection -----
    uart_puts("ROTORS  (1=I  2=II  3=III  4=IV  5=V)\r\n");
    uart_puts("  Rotors must be unique.\r\n\r\n");

    do
    {
        valid = 1;

        uart_puts("  Left   rotor [1-5]: ");
        uart_readline(buf, 4);
        if(buf[0] >= '1' && buf[0] <= '5' && buf[1] == '\0')
            left_rotor_num = buf[0] - '0';
        else { uart_puts("  ? Enter 1 2 3 4 or 5\r\n"); valid = 0; continue; }

        uart_puts("  Center rotor [1-5]: ");
        uart_readline(buf, 4);
        if(buf[0] >= '1' && buf[0] <= '5' && buf[1] == '\0')
            center_rotor_num = buf[0] - '0';
        else { uart_puts("  ? Enter 1 2 3 4 or 5\r\n"); valid = 0; continue; }

        uart_puts("  Right  rotor [1-5]: ");
        uart_readline(buf, 4);
        if(buf[0] >= '1' && buf[0] <= '5' && buf[1] == '\0')
            right_rotor_num = buf[0] - '0';
        else { uart_puts("  ? Enter 1 2 3 4 or 5\r\n"); valid = 0; continue; }

        if(left_rotor_num == center_rotor_num ||
           left_rotor_num == right_rotor_num  ||
           center_rotor_num == right_rotor_num)
        {
            uart_puts("  ! Rotors must be unique. Try again.\r\n");
            valid = 0;
        }
    }
    while(!valid);

    // ----- STEP 2: Ring settings -----
    uart_puts("\r\nRING SETTINGS  (e.g. AAA = no ring offset, Enter = AAA)\r\n");
    do
    {
        valid = 1;
        uart_puts("  Ring L M R [3 letters]: ");
        uart_readline(buf, 6);

        if(buf[0] == '\0')
        {
            left_ring = center_ring = right_ring = 0;
            break;
        }

        if(buf[0] >= 'A' && buf[0] <= 'Z' &&
           buf[1] >= 'A' && buf[1] <= 'Z' &&
           buf[2] >= 'A' && buf[2] <= 'Z' && buf[3] == '\0')
        {
            left_ring   = buf[0] - 'A';
            center_ring = buf[1] - 'A';
            right_ring  = buf[2] - 'A';
        }
        else { uart_puts("  ? Enter exactly 3 letters e.g. AAA\r\n"); valid = 0; }
    }
    while(!valid);

    // ----- STEP 3: Starting positions -----
    uart_puts("\r\nSTARTING POSITIONS  (e.g. AAA, Enter = AAA)\r\n");
    do
    {
        valid = 1;
        uart_puts("  Start L M R [3 letters]: ");
        uart_readline(buf, 6);

        if(buf[0] == '\0')
        {
            left_start = center_start = right_start = 0;
            break;
        }

        if(buf[0] >= 'A' && buf[0] <= 'Z' &&
           buf[1] >= 'A' && buf[1] <= 'Z' &&
           buf[2] >= 'A' && buf[2] <= 'Z' && buf[3] == '\0')
        {
            left_start   = buf[0] - 'A';
            center_start = buf[1] - 'A';
            right_start  = buf[2] - 'A';
        }
        else { uart_puts("  ? Enter exactly 3 letters e.g. AAA\r\n"); valid = 0; }
    }
    while(!valid);

    left_pos   = left_start;
    center_pos = center_start;
    right_pos  = right_start;

    // ----- STEP 4: Reflector -----
    uart_puts("\r\nREFLECTOR  (A=UKW-A  B=UKW-B  C=UKW-C, Enter = B)\r\n");
    do
    {
        valid = 1;
        uart_puts("  Reflector [A/B/C]: ");
        uart_readline(buf, 4);

        if(buf[0] == '\0')              reflector_num = 1;
        else if(buf[0] == 'A' && buf[1] == '\0') reflector_num = 0;
        else if(buf[0] == 'B' && buf[1] == '\0') reflector_num = 1;
        else if(buf[0] == 'C' && buf[1] == '\0') reflector_num = 2;
        else { uart_puts("  ? Enter A B or C\r\n"); valid = 0; }
    }
    while(!valid);

    // ----- STEP 5: Plugboard -----
    uart_puts("\r\nPLUGBOARD  (enter letter pairs separated by spaces e.g. AZ BM)\r\n");
    uart_puts("  Up to 13 pairs. Enter blank for no plugboard.\r\n");
    uart_puts("  Plugboard pairs: ");
    uart_readline(buf, 32);

    plugboard_init();

    i = 0;
    while(buf[i] != '\0')
    {
        while(buf[i] == ' ') i++;           // skip spaces
        if(buf[i] == '\0') break;

        a = buf[i++];
        b = buf[i++];

        if(a >= 'A' && a <= 'Z' && b >= 'A' && b <= 'Z' && a != b)
            plugboard_add(a, b);
    }

    // ----- STEP 6: Mode -----
    uart_puts("\r\nMODE  (E=Encrypt  D=Decrypt  P=Passthrough, Enter = E)\r\n");
    do
    {
        valid = 1;
        uart_puts("  Mode [E/D/P]: ");
        uart_readline(buf, 4);

        if(buf[0] == '\0') enigma_mode = 'E';
        else if(buf[0] == 'E' && buf[1] == '\0') enigma_mode = 'E';
        else if(buf[0] == 'D' && buf[1] == '\0') enigma_mode = 'D';
        else if(buf[0] == 'P' && buf[1] == '\0') enigma_mode = 'P';
        else { uart_puts("  ? Enter E D or P\r\n"); valid = 0; }
    }
    while(!valid);

    // ----- CONFIRM -----
    uart_puts("\r\n  +-------------------------------+\r\n");
    uart_puts("  |     ENIGMA SETTINGS           |\r\n");
    uart_puts("  +-------------------------------+\r\n");

    uart_puts("  Rotors   : ");
    uart_putchar((char)('0' + left_rotor_num));
    uart_putchar('-');
    uart_putchar((char)('0' + center_rotor_num));
    uart_putchar('-');
    uart_putchar((char)('0' + right_rotor_num));
    uart_puts("\r\n");

    uart_puts("  Ring     : ");
    uart_putchar((char)('A' + left_ring));
    uart_putchar((char)('A' + center_ring));
    uart_putchar((char)('A' + right_ring));
    uart_puts("\r\n");

    uart_puts("  Start    : ");
    uart_putchar((char)('A' + left_start));
    uart_putchar((char)('A' + center_start));
    uart_putchar((char)('A' + right_start));
    uart_puts("\r\n");

    uart_puts("  Reflector: UKW-");
    uart_putchar((char)('A' + reflector_num));
    uart_puts("\r\n");

    uart_puts("  Plugboard: ");
    valid = 0;
    for(i = 0; i < 26; i++)
    {
        if(plugboard[i] != (char)('A' + i) && plugboard[i] > (char)('A' + i))
        {
            uart_putchar((char)('A' + i));
            uart_putchar(plugboard[i]);
            uart_putchar(' ');
            valid++;
        }
    }
    if(!valid) uart_puts("none");
    uart_puts("\r\n");

    uart_puts("  Mode     : ");
    if(enigma_mode == 'E') uart_puts("ENCRYPT");
    else if(enigma_mode == 'D') uart_puts("DECRYPT");
    else uart_puts("PASSTHROUGH");
    uart_puts("\r\n");
    uart_puts("  +-------------------------------+\r\n\r\n");

    uart_puts("Confirm? [Y=start  N=restart wizard]: ");
    uart_readline(buf, 4);

    if(buf[0] == 'N') goto restart;

    uart_puts("\r\n================================================\r\n");
    uart_puts("  Ready. Press keys on the typewriter.\r\n");
    uart_puts("================================================\r\n\r\n");
}

// ===== KEYBOARD SCANNING =====
void keyboard_init(void)
{
    P6DIR |= BIT0 | BIT1 | BIT2 | BIT3 | BIT4;
    P7DIR |= BIT0;
    P3DIR |= BIT6 | BIT5;

    P6OUT |= BIT0 | BIT1 | BIT2 | BIT3 | BIT4;
    P7OUT |= BIT0;
    P3OUT |= BIT6 | BIT5;

    P6DIR &= ~(BIT5 | BIT6);
    P3DIR &= ~(BIT4 | BIT3 | BIT2);
    P1DIR &= ~BIT6;
    P2DIR &= ~BIT7;
    P4DIR &= ~BIT2;

    P6REN |= BIT5 | BIT6;
    P3REN |= BIT4 | BIT3 | BIT2;
    P1REN |= BIT6;
    P2REN |= BIT7;
    P4REN |= BIT2;

    P6OUT |= BIT5 | BIT6;
    P3OUT |= BIT4 | BIT3 | BIT2;
    P1OUT |= BIT6;
    P2OUT |= BIT7;
    P4OUT |= BIT2;
}

void set_row_high(int row)
{
    switch(row)
    {
        case 0: P6OUT |= BIT0; break;
        case 1: P6OUT |= BIT1; break;
        case 2: P6OUT |= BIT2; break;
        case 3: P6OUT |= BIT3; break;
        case 4: P6OUT |= BIT4; break;
        case 5: P7OUT |= BIT0; break;
        case 6: P3OUT |= BIT6; break;
        case 7: P3OUT |= BIT5; break;
    }
}

void set_row_low(int row)
{
    switch(row)
    {
        case 0: P6OUT &= ~BIT0; break;
        case 1: P6OUT &= ~BIT1; break;
        case 2: P6OUT &= ~BIT2; break;
        case 3: P6OUT &= ~BIT3; break;
        case 4: P6OUT &= ~BIT4; break;
        case 5: P7OUT &= ~BIT0; break;
        case 6: P3OUT &= ~BIT6; break;
        case 7: P3OUT &= ~BIT5; break;
    }
}

int read_col(int col)
{
    switch(col)
    {
        case 0: return !(P6IN & BIT5);
        case 1: return !(P3IN & BIT4);
        case 2: return !(P3IN & BIT3);
        case 3: return !(P1IN & BIT6);
        case 4: return !(P6IN & BIT6);
        case 5: return !(P3IN & BIT2);
        case 6: return !(P2IN & BIT7);
        case 7: return !(P4IN & BIT2);
    }

    return 0;
}

char scan_keyboard(void)
{
    int  r;
    int  c;
    char key;

    for(r = 0; r < ROWS; r++)
    {
        set_row_low(r);
        delay_ms(2);

        for(c = 0; c < COLS; c++)
        {
            if(read_col(c))
            {
                key = keymap[r * 8 + c];

                while(read_col(c))
                    delay_ms(1);

                delay_ms(25);
                set_row_high(r);

                return key;
            }
        }

        set_row_high(r);
    }

    return 0;
}

// ===== MAIN =====
int main(void)
{
    char key;
    char encrypted;

    WDTCTL   = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;          // unlock GPIO (required on FR6989)

    plugboard_init();
    max_init();
    keyboard_init();
    uart_init();

    max_clear();

    // Run setup wizard via PuTTY before accepting keyboard input
    setup_wizard();

    while(1)
    {
        key = scan_keyboard();

        if(key)
        {
            display_letter(key);    // show key on LED matrix (her original)

            if(enigma_mode == 'P')
            {
                uart_puts("[PASS] ");
                uart_putchar(key);
                uart_puts("\r\n");
            }
            else
            {
                encrypted = encrypt_char(key);

                if(encrypted)
                {
                    display_letter(encrypted);   // show result on LED matrix

                    uart_puts(enigma_mode == 'E' ? "[ENC] " : "[DEC] ");
                    uart_putchar(key);
                    uart_puts(" => ");
                    uart_putchar(encrypted);
                    uart_puts("  L:");
                    uart_putchar((char)('A' + left_pos));
                    uart_puts(" M:");
                    uart_putchar((char)('A' + center_pos));
                    uart_puts(" R:");
                    uart_putchar((char)('A' + right_pos));
                    uart_puts("\r\n");
                }
            }
        }
    }
}
