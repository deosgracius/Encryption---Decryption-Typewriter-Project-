/******************************************************************************
 * main.c -- Enigma Cipher -- MSP430FR6989
 *
 * Input  : type letters in PuTTY terminal (115200 baud, 8N1)
 * Output : encrypted / decrypted result printed in PuTTY
 *
 * At boot a setup wizard configures all parameters.
 * Type '?' at any time during operation to re-enter the wizard.
 * Enter key resets rotors to starting positions.
 *
 * Stepping: historical double-step anomaly (correct Enigma behaviour).
 * Ring settings, starting positions, plugboard all configurable at runtime.
 ******************************************************************************/

#include <msp430fr6989.h>
#include <stdint.h>

/* ============================================================================
 * ROTOR DATA -- same strings as MSP430F5529
 * ========================================================================== */
static const char rotor_wiring[5][27] = {
    "EKMFLGDQVZNTOWYHXUSPAIBRCJ",  /* Rotor I   */
    "AJDKSIRUXBLHWTMCQGZNPYFVOE",  /* Rotor II  */
    "BDFHJLCPRTXVZNYEIWGAKMUSQO",  /* Rotor III */
    "ESOVPZJAYQUIRHXLNFTGKDCMWB",  /* Rotor IV  */
    "VZBRGITYUPSDNHLXAWMJQOFECK"   /* Rotor V   */
};

/* Notch positions (A=0 based): Q=16 E=4 V=21 J=9 Z=25 */
static const int rotor_notches[5] = { 16, 4, 21, 9, 25 };

/* ============================================================================
 * REFLECTOR DATA
 * ========================================================================== */
static const char reflectors[3][27] = {
    "EJMZALYXVBWFCRQUONTSPIKHGD",  /* UKW-A */
    "YRUHQSLDPXNGOKMIEBFZCWVJAT",  /* UKW-B */
    "FVPJIAOYEDRZXWGCTKUQSBNMHL"   /* UKW-C */
};

/* ============================================================================
 * KEYBOARD MAP -- from MSP430F5529 implementation (kept for reference)
 * ========================================================================== */
static const char keymap[64] = {
 /* row 0 */ 'j','f','1','l','v','v','l','p',
 /* row 1 */ 'k','q','h','t','m','y','e','v',
 /* row 2 */ 'l','y','l','d','j','g','a','v',
 /* row 3 */ 'b','m','i','p','y','r','w','p',
 /* row 4 */ 'c','x','k','s','h','f','z','w',
 /* row 5 */ 'v','n','u','o','t','e','q','p',
 /* row 6 */ 'y','l','m','l','z','v','k','l',
 /* row 7 */ 'v','n','e','t','v','t','m','l'
};

/* ============================================================================
 * RUNTIME STATE -- all configurable via wizard
 * ========================================================================== */
static int  left_id,   center_id,   right_id;    /* 0-based rotor indices  */
static int  left_pos,  center_pos,  right_pos;   /* current positions 0-25 */
static int  left_ring, center_ring, right_ring;  /* ring settings   0-25   */
static int  left_start,center_start,right_start; /* Grundstellung   0-25   */
static int  reflector_id;                        /* 0=A 1=B 2=C            */
static char enigma_mode;                         /* 'E' 'D' 'P'            */
static char plugboard[26];                       /* substitution table      */

/* ============================================================================
 * UART -- eUSCI_A1  P3.4=TX  P3.5=RX  115200 baud  8MHz DCO
 * ========================================================================== */
static void uart_init(void)
{
    CSCTL0_H  = CSKEY_H;
    CSCTL1    = DCOFSEL_6;                        /* 8 MHz DCO              */
    CSCTL2    = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3    = DIVA__1 | DIVS__1 | DIVM__1;
    CSCTL0_H  = 0;

    P3SEL0   |= BIT4 | BIT5;
    P3SEL1   &= ~(BIT4 | BIT5);

    UCA1CTLW0 = UCSWRST;
    UCA1CTLW0|= UCSSEL__SMCLK;
    UCA1BRW   = 4;                               /* 8MHz/115200            */
    UCA1MCTLW = ((uint16_t)0x55<<8)|((uint16_t)5<<4)|UCOS16;
    UCA1CTLW0&= ~UCSWRST;
}

static void uart_putchar(char c)
{
    while(!(UCA1IFG & UCTXIFG));
    UCA1TXBUF = (unsigned char)c;
}

static void uart_puts(const char *s)
{ while(*s) uart_putchar(*s++); }

static char uart_getchar(void)
{
    while(!(UCA1IFG & UCRXIFG));
    return (char)UCA1RXBUF;
}

/* ============================================================================
 * STRING HELPERS (no stdlib needed)
 * ========================================================================== */
static void str_upper(char *s)
{ while(*s){ if(*s>='a'&&*s<='z') *s=(char)(*s-32); s++; } }

static int str_eq(const char *a, const char *b)
{
    while(*a && *b) if(*a++ != *b++) return 0;
    return (*a=='\0' && *b=='\0');
}

/* Read line from UART into buf (max len-1 chars), echoes, handles backspace */
static void read_line(char *buf, int len)
{
    int i=0; char c;
    while(1){
        c=uart_getchar();
        if(c=='\r'||c=='\n'){ uart_puts("\r\n"); break; }
        if((c==0x08||c==0x7F)&&i>0){ i--; uart_puts("\x08 \x08"); continue; }
        if(c>=0x20&&c<0x7F&&i<len-1){ buf[i++]=c; uart_putchar(c); }
    }
    buf[i]='\0';
    str_upper(buf);
}

static void put_rotor_name(int id)
{
    if(id==0) uart_puts("I");
    else if(id==1) uart_puts("II");
    else if(id==2) uart_puts("III");
    else if(id==3) uart_puts("IV");
    else uart_puts("V");
}

static int parse_rotor(const char *buf)
{
    if(str_eq(buf,"I")||str_eq(buf,"1"))   return 0;
    if(str_eq(buf,"II")||str_eq(buf,"2"))  return 1;
    if(str_eq(buf,"III")||str_eq(buf,"3")) return 2;
    if(str_eq(buf,"IV")||str_eq(buf,"4"))  return 3;
    if(str_eq(buf,"V")||str_eq(buf,"5"))   return 4;
    return -1;
}

static void put_int(int n)
{
    if(n>=10) uart_putchar((char)('0'+n/10));
    uart_putchar((char)('0'+n%10));
}

/* ============================================================================
 * ENIGMA CIPHER -- A2I/I2A with 'R' base (matches MSP430F5529)
 * ========================================================================== */
static int A2I(char c)
{ return (int)c-(int)'A'; }

static char I2A(int i)
{ return (char)(i+'A'); }

static int wrap26(int x)
{
    while(x< 0) x+=26;
    while(x>=26) x-=26;
    return x;
}

/* rotor_forward with ring setting (ring is A-based 0=no offset) */
static int rotor_forward(int input, int r_idx, int pos, int ring)
{
    int shifted = wrap26(input + pos - ring);
    int wired   = A2I(rotor_wiring[r_idx][shifted]);
    return wrap26(wired - pos + ring);
}

/* rotor_backward with ring setting (linear search, same as MSP430F5529) */
static int rotor_backward(int input, int r_idx, int pos, int ring)
{
    int shifted = wrap26(input + pos - ring);
    int i;
    for(i=0;i<26;i++){
        if(A2I(rotor_wiring[r_idx][i])==shifted)
            return wrap26(i - pos + ring);
    }
    return input;
}

/* step_rotors -- historical double-step anomaly
 *
 * Standard Enigma stepping (checked BEFORE any rotor moves):
 *   1. If center is at its notch -> step left AND center (double-step)
 *   2. Else if right is at its notch -> step center
 *   3. Always step right
 *
 * The double-step: center rotor steps a second time when it is itself
 * at the notch position, causing it to advance one extra position.
 * This is a mechanical quirk of the Enigma design.
 * ========================================================================== */
static void step_rotors(void)
{
    int cn = rotor_notches[center_id];
    int rn = rotor_notches[right_id];

    if(center_pos == cn) {
        left_pos   = (left_pos   + 1) % 26;  /* left steps    */
        center_pos = (center_pos + 1) % 26;  /* center double-steps */
    } else if(right_pos == rn) {
        center_pos = (center_pos + 1) % 26;  /* center steps  */
    }
    right_pos = (right_pos + 1) % 26;        /* right always steps */
}

/* encrypt_char -- same logic as MSP430F5529 + ring settings + double-step */
static char encrypt_char(char input)
{
    int signal;

    if(input>='a'&&input<='z') input=(char)(input-32);
    if(input<'A'||input>'Z') return 0;

    step_rotors();

    input = plugboard[input-'A'];      /* plugboard in */
    signal = A2I(input);

    signal = rotor_forward (signal, right_id,  right_pos,  right_ring);
    signal = rotor_forward (signal, center_id, center_pos, center_ring);
    signal = rotor_forward (signal, left_id,   left_pos,   left_ring);

    signal = A2I(reflectors[reflector_id][signal]);

    signal = rotor_backward(signal, left_id,   left_pos,   left_ring);
    signal = rotor_backward(signal, center_id, center_pos, center_ring);
    signal = rotor_backward(signal, right_id,  right_pos,  right_ring);

    {
        char out = I2A(signal);
        /* I2A with A-base always produces A-Z -- safety clamp kept */
        if(out<'A'||out>'Z'){
            int v=signal;
            while(v< 0) v+=26;
            while(v>=26) v-=26;
            out=(char)('A'+v);
        }
        out = plugboard[out-'A'];      /* plugboard out */
        return out;
    }
}

/* ============================================================================
 * PLUGBOARD INIT
 * ========================================================================== */
static void plugboard_clear(void)
{ int i; for(i=0;i<26;i++) plugboard[i]=(char)('A'+i); }

static int plugboard_add(char a, char b)
{
    if(a<'A'||a>'Z'||b<'A'||b>'Z'||a==b) return 0;
    if(plugboard[a-'A']!=(char)('A'+(a-'A'))) return 0;  /* already paired */
    if(plugboard[b-'A']!=(char)('A'+(b-'A'))) return 0;
    plugboard[a-'A']=b;
    plugboard[b-'A']=a;
    return 1;
}

/* ============================================================================
 * PRINT SUMMARY
 * ========================================================================== */
static void print_summary(void)
{
    int i; int found=0;
    uart_puts("\r\n  +----------------------------------+\r\n");
    uart_puts("  |     CURRENT ENIGMA SETTINGS      |\r\n");
    uart_puts("  +----------------------------------+\r\n");

    uart_puts("  Rotors    : ");
    put_rotor_name(left_id);   uart_puts(" (L)  ");
    put_rotor_name(center_id); uart_puts(" (M)  ");
    put_rotor_name(right_id);  uart_puts(" (R)\r\n");

    uart_puts("  Ring      : ");
    uart_putchar((char)('A'+left_ring));
    uart_putchar((char)('A'+center_ring));
    uart_putchar((char)('A'+right_ring));
    uart_puts("\r\n");

    uart_puts("  Start     : ");
    uart_putchar((char)('A'+left_start));
    uart_putchar((char)('A'+center_start));
    uart_putchar((char)('A'+right_start));
    uart_puts("\r\n");

    uart_puts("  Curr pos  : ");
    uart_putchar((char)('A'+left_pos));
    uart_putchar((char)('A'+center_pos));
    uart_putchar((char)('A'+right_pos));
    uart_puts("\r\n");

    uart_puts("  Notches   : ");
    uart_putchar((char)('A'+rotor_notches[left_id]));
    uart_putchar((char)('A'+rotor_notches[center_id]));
    uart_putchar((char)('A'+rotor_notches[right_id]));
    uart_puts("  (L M R)\r\n");

    uart_puts("  Reflector : UKW-");
    uart_putchar((char)('A'+reflector_id));
    uart_puts("\r\n");

    uart_puts("  Plugboard : ");
    for(i=0;i<26;i++){
        if(plugboard[i]!=(char)('A'+i)&&plugboard[i]>(char)('A'+i)){
            uart_putchar((char)('A'+i));
            uart_putchar(plugboard[i]);
            uart_putchar(' ');
            found++;
        }
    }
    if(!found) uart_puts("none");
    uart_puts("\r\n");

    uart_puts("  Mode      : ");
    if(enigma_mode=='E') uart_puts("ENCRYPT");
    else if(enigma_mode=='D') uart_puts("DECRYPT");
    else uart_puts("PASSTHROUGH");
    uart_puts("\r\n");
    uart_puts("  +----------------------------------+\r\n");
}

/* ============================================================================
 * SETUP WIZARD
 * Runs at boot and whenever user types '?' during operation.
 * ========================================================================== */
static void run_wizard(void)
{
    char buf[8];
    int  val, id0, id1, id2;
    int  valid;
    int  pair_count;

    uart_puts("\033[2J\033[H");  /* clear terminal */

restart:
    uart_puts("\r\n================================================\r\n");
    uart_puts("   ENIGMA CIPHER -- MSP430FR6989\r\n");
    uart_puts("   Setup Wizard\r\n");
    uart_puts("================================================\r\n\r\n");

    /* -- STEP 1: Rotor selection -- */
    uart_puts("STEP 1 -- ROTORS\r\n");
    uart_puts("  Options: I  II  III  IV  V  (must be unique)\r\n\r\n");

    do {
        uart_puts("  Left rotor   [I-V]: ");
        read_line(buf, sizeof(buf));
        id0 = parse_rotor(buf);
        if(id0<0) uart_puts("  ? Enter I II III IV or V\r\n");
    } while(id0<0);

    do {
        uart_puts("  Middle rotor [I-V]: ");
        read_line(buf, sizeof(buf));
        id1 = parse_rotor(buf);
        if(id1<0){ uart_puts("  ? Enter I II III IV or V\r\n"); continue; }
        if(id1==id0){ uart_puts("  ? Must differ from left rotor\r\n"); id1=-1; }
    } while(id1<0);

    do {
        uart_puts("  Right rotor  [I-V]: ");
        read_line(buf, sizeof(buf));
        id2 = parse_rotor(buf);
        if(id2<0){ uart_puts("  ? Enter I II III IV or V\r\n"); continue; }
        if(id2==id0||id2==id1){ uart_puts("  ? Must differ from other rotors\r\n"); id2=-1; }
    } while(id2<0);

    left_id=id0; center_id=id1; right_id=id2;

    /* -- STEP 2: Ring settings -- */
    uart_puts("\r\nSTEP 2 -- RING SETTINGS (Ringstellung)\r\n");
    uart_puts("  3 letters for L M R e.g. AAA  (Enter = AAA)\r\n");
    do {
        valid=1;
        uart_puts("  Ring [LMR]: ");
        read_line(buf, sizeof(buf));
        if(buf[0]=='\0'){ left_ring=center_ring=right_ring=0; break; }
        if(buf[0]>='A'&&buf[0]<='Z'&&buf[1]>='A'&&buf[1]<='Z'&&
           buf[2]>='A'&&buf[2]<='Z'&&buf[3]=='\0'){
            left_ring  =buf[0]-'A';
            center_ring=buf[1]-'A';
            right_ring =buf[2]-'A';
        } else { uart_puts("  ? Enter exactly 3 letters e.g. AAA\r\n"); valid=0; }
    } while(!valid);

    /* -- STEP 3: Starting positions -- */
    uart_puts("\r\nSTEP 3 -- STARTING POSITIONS (Grundstellung)\r\n");
    uart_puts("  3 letters for L M R e.g. AAA  (Enter = AAA)\r\n");
    do {
        valid=1;
        uart_puts("  Start [LMR]: ");
        read_line(buf, sizeof(buf));
        if(buf[0]=='\0'){ left_start=center_start=right_start=0; break; }
        if(buf[0]>='A'&&buf[0]<='Z'&&buf[1]>='A'&&buf[1]<='Z'&&
           buf[2]>='A'&&buf[2]<='Z'&&buf[3]=='\0'){
            left_start  =buf[0]-'A';
            center_start=buf[1]-'A';
            right_start =buf[2]-'A';
        } else { uart_puts("  ? Enter exactly 3 letters e.g. AAA\r\n"); valid=0; }
    } while(!valid);

    /* Apply starting positions */
    left_pos=left_start; center_pos=center_start; right_pos=right_start;

    /* -- STEP 4: Reflector -- */
    uart_puts("\r\nSTEP 4 -- REFLECTOR\r\n");
    do {
        valid=1;
        uart_puts("  Reflector [A/B/C]  (Enter = B): ");
        read_line(buf, sizeof(buf));
        if(buf[0]=='\0')               reflector_id=1;
        else if(buf[0]=='A'&&buf[1]=='\0') reflector_id=0;
        else if(buf[0]=='B'&&buf[1]=='\0') reflector_id=1;
        else if(buf[0]=='C'&&buf[1]=='\0') reflector_id=2;
        else { uart_puts("  ? Enter A B or C\r\n"); valid=0; }
    } while(!valid);

    /* -- STEP 5: Plugboard -- */
    uart_puts("\r\nSTEP 5 -- PLUGBOARD\r\n");
    uart_puts("  Type a pair e.g. AZ then Enter. Blank Enter when done.\r\n");
    uart_puts("  Max 13 pairs. Each letter can only appear once.\r\n\r\n");

    plugboard_clear();
    pair_count=0;

    while(pair_count<13){
        uart_puts("  Pair "); put_int(pair_count+1); uart_puts(" (Enter to finish): ");
        read_line(buf, sizeof(buf));
        if(buf[0]=='\0') break;
        if(buf[0]>='A'&&buf[0]<='Z'&&buf[1]>='A'&&buf[1]<='Z'&&
           buf[2]=='\0'&&buf[0]!=buf[1]){
            if(plugboard_add(buf[0],buf[1])){
                uart_puts("  + "); uart_putchar(buf[0]);
                uart_puts(" <--> "); uart_putchar(buf[1]); uart_puts("\r\n");
                pair_count++;
            } else {
                uart_puts("  ? Letter already used in another pair\r\n");
            }
        } else {
            uart_puts("  ? Enter exactly 2 different letters e.g. AZ\r\n");
        }
    }
    if(pair_count==0) uart_puts("  (no pairs -- identity)\r\n");

    /* -- STEP 6: Mode -- */
    uart_puts("\r\nSTEP 6 -- MODE\r\n");
    uart_puts("  E = Encrypt   D = Decrypt   P = Passthrough\r\n");
    do {
        valid=1;
        uart_puts("  Mode [E/D/P]: ");
        read_line(buf, sizeof(buf));
        if(buf[0]=='E') enigma_mode='E';
        else if(buf[0]=='D') enigma_mode='D';
        else if(buf[0]=='P') enigma_mode='P';
        else { uart_puts("  ? Enter E D or P\r\n"); valid=0; }
    } while(!valid);

    /* -- CONFIRM -- */
    print_summary();
    uart_puts("\r\nConfirm? [Y = start   N = restart wizard]: ");
    read_line(buf, sizeof(buf));
    if(buf[0]=='N') goto restart;

    uart_puts("\r\n================================================\r\n");
    uart_puts("  Ready. Type A-Z to encrypt.\r\n");
    uart_puts("  Enter = reset rotors to starting positions.\r\n");
    uart_puts("  ?     = re-enter setup wizard.\r\n");
    uart_puts("================================================\r\n\r\n");
}

/* ============================================================================
 * MAIN
 * ========================================================================== */
int main(void)
{
    char key;
    char result;

    WDTCTL   = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;

    uart_init();
    plugboard_clear();

    /* Set safe defaults before wizard */
    left_id=0; center_id=1; right_id=2;
    left_pos=center_pos=right_pos=0;
    left_start=center_start=right_start=0;
    left_ring=center_ring=right_ring=0;
    reflector_id=1;
    enigma_mode='E';

    run_wizard();

    /* -- Main loop --
     * MSP430F5529 equivalent:
     *   key = scan_keyboard()      -> uart_getchar()
     *   display_letter(key)        -> echo to PuTTY
     *   enc = encrypt_char(key)    -> encrypt_char(key)  (same function)
     *   display_letter(enc)        -> print result to PuTTY
     */
    while(1)
    {
        key = uart_getchar();

        /* '?' -- re-enter wizard */
        if(key=='?'){
            run_wizard();
            continue;
        }

        /* Uppercase */
        if(key>='a'&&key<='z') key=(char)(key-32);

        /* Enter -- reset rotors to starting positions */
        if(key=='\r'){
            left_pos  =left_start;
            center_pos=center_start;
            right_pos =right_start;
            uart_puts("--- msg end (rotors reset to ");
            uart_putchar((char)('A'+left_start));
            uart_putchar((char)('A'+center_start));
            uart_putchar((char)('A'+right_start));
            uart_puts(") ---\r\n\r\n");
            continue;
        }

        if(key<'A'||key>'Z') continue;   /* ignore non-letters */

        if(enigma_mode=='P'){
            uart_puts("[PASS] "); uart_putchar(key); uart_puts("\r\n");
        } else {
            result = encrypt_char(key);
            uart_puts(enigma_mode=='E' ? "[ENC] " : "[DEC] ");
            uart_putchar(key);
            uart_puts(" => ");
            uart_putchar(result);
            uart_puts("  L:"); uart_putchar((char)('A'+left_pos));
            uart_puts(" M:"); uart_putchar((char)('A'+center_pos));
            uart_puts(" R:"); uart_putchar((char)('A'+right_pos));
            uart_puts("\r\n");
        }
    }
}
