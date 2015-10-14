// Rough-and-ready Arduino code for testing, calibrating and using core memory shield.
//
// Copyright 2011 Ben North and Oliver Nash.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#define ADDRSIZE      5
#define WORDSIZE      (1 << ADDRSIZE)
#define ENABLE        B00000100 // PORTD
#define DRD           B00000010 // PORTB
#define DWR           B00000001 // PORTB

#define SPACE         26
#define DOT           27
#define EXCL          28
#define QUEST         29
#define HASH          30
#define AT            31

static unsigned int WRITE_ON_US  = 2;
static unsigned int WRITE_OFF_US = 2; // May not be necessary.
static char trace_core_calls_p = 0;

void write_bit(byte n, const int v)
{
  if (trace_core_calls_p)
  {
    char buf[64];
    sprintf(buf, "x[0%02o] <- %d", n, v);
    Serial.println(buf);
  }

  // Assert 0 <= n <= 31, v == 0 or 1.
  noInterrupts();
  if(v == 0)
  {
    PORTB &= (~DWR);
  }
  else
  {
    PORTB |= DWR;
  }
  PORTD = ((n << 3) & (~ENABLE));
  PORTD |= ENABLE; // Enable separately to be safe.
  delayMicroseconds(WRITE_ON_US);
  PORTD &= (~ENABLE);
  delayMicroseconds(WRITE_OFF_US);
  interrupts();
}

int exchg_bit(byte n, const int v)
{
  write_bit(n, v);

  if(PINB & DRD)
  {
    // Switching occurred so core held opposite of
    // what we just set it to.

    if (trace_core_calls_p)
      Serial.println("___ -> CHANGE");

    return !v;
  }
  else
  {
    // No switching occurred so core held whatever
    // we just wrote to it.

    if (trace_core_calls_p)
      Serial.println("___ -> NO-CHANGE");

    return v;
  }
}

int read_bit(const int n)
{
  char buf[64];
  if (trace_core_calls_p)
  {
    sprintf(buf, "x[0%02o] -> ___", n);
    Serial.println(buf);
  }

  write_bit(n, 0);
  if(PINB & DRD)
  {
    // Switching occurred so core held 1.

    if (trace_core_calls_p)
      Serial.println("___ -> 1");

    write_bit(n, 1); // Put it back!
    return 1;
  }
  else
  {
    // No switching occurred so core held 0 (and still holds it).

    if (trace_core_calls_p)
      Serial.println("___ -> 0");

    return 0;
  }
}

void write_word(unsigned long v)
{
  int n;
  for(n = 0; n < WORDSIZE; n++)
  {
    write_bit(n, v & 1);
    v >>= 1;
  }
}

unsigned long read_word(void)
{
  unsigned long v = 0;
  int n;
  for(n = WORDSIZE-1; n >= 0; n--)
  {
    v <<= 1;
    v |= read_bit(n);
  }
  return v;
}

unsigned long exchg_word(unsigned long v)
{
  unsigned long v_prev;
  int n;
  for (n = 0; n < WORDSIZE; ++n)
  {
    unsigned long b = exchg_bit(n, v & 1);
    v >>= 1;
    v_prev >>= 1;
    v_prev |= (b << 31);
  }
  return v_prev;
}

static void toggle_tracing()
{
  char buf[64];
  trace_core_calls_p = !trace_core_calls_p;
  sprintf(buf, "trace-core-calls-p now %d", trace_core_calls_p);
  Serial.println(buf);
}

void print_5bit_string(unsigned long d)
{
  int i;
  char buf[7];

  for (i=0; i<6; i++) {
    unsigned char c = (unsigned char)(d >> (25 - (i*5)) & 0x1f);
    if (c == SPACE) {
      c = ' ';
    } else if (c == DOT) {
      c = '.';
    } else if (c == EXCL) {
      c = '!';
    } else if (c == HASH) {
      c = '#';
    } else if (c == QUEST) {
      c = '?';
    } else if (c == AT) {
      c = '@';
    } else {
      c += 0x61;
    }
    
    buf[i] = c;
  }
   
  buf[6] = '\0';

  Serial.print(buf); 
}

void print_ascii_data(unsigned long d)
{
  char buf[5];
  
  buf[0] = (unsigned char)((d  >> 24) & 0xff); 
  buf[1] = (unsigned char)((d >> 16) & 0xff); 
  buf[2] = (unsigned char)((d >> 8) & 0xff);
  buf[3] = (unsigned char)(d & 0xff); 
  buf[4] = '\0';

  Serial.print(buf); 
}

static void read_all_memory()
{
  unsigned long d = read_word();

  Serial.print("Core data read: B");
  Serial.print(d, BIN);
  Serial.print(" = 0x");
  Serial.println(d, HEX);
  Serial.print("8 bit ascii: "); print_ascii_data(d);
  Serial.println("");
  //Serial.print("6 bit ascii: "); print_6bit_string(d);
  //Serial.println("");
  Serial.print("5 bit ascii: "); print_5bit_string(d);
  Serial.println("");
}

void write_5bit_string(void)
{
  char buf[64];
  int i, x;
  unsigned long d1, d = 0;
  
  Serial.print("5 bit write\nenter 6 lowercase characters or .,!#? and space: ");
  
  for(i = 0; i < 6; i++)
  {
    d <<= 5;
    
    while(Serial.available() == 0) ;
    x = Serial.read();

    if (x == ' ') {
      d += SPACE;
    } else if (x == '.') {
      d += DOT;
    } else if (x == '!') {
      d += EXCL;
    } else if (x == '#') {
      d += HASH;
    } else if (x == '?') {
      d += QUEST;
    } else if (x == '@') {
      d += AT;
    } else {
      d += (x - 0x61);
    }
  }
  
  d1 = exchg_word(d);

  Serial.print("replaced: ");
  print_5bit_string(d1);
  Serial.print(" with: ");
  print_5bit_string(d);
  Serial.println("");    
}

void write_string(void)
{
  char buf[64];
  int i, x;
  unsigned long d1, d = 0;
  
  Serial.print("enter 4 characters: ");
  
  for(i = 0; i < 4; i++)
  {
    d <<= 8;
    
    while(Serial.available() == 0) ;
    x = Serial.read();
    d += x;
  }
  
  d1 = exchg_word(d);

  Serial.print("replaced: ");
  print_ascii_data(d1);
  Serial.print(" with: ");
  print_ascii_data(d);
  Serial.println("");
}

void read_string(int bits)
{
  unsigned long d = read_word();
  Serial.print("read: ");
  if (bits == 5) {
    print_5bit_string(d);
  } else if (bits == 6) {
    //print_6bit_data(d);
  } else if (bits == 8) {
    print_ascii_data(d);
  }
  Serial.println("");
}

void setup(void) 
{
  DDRD |= B11111100;
  PORTD = (~ENABLE);
  DDRB |= DWR;
  DDRB &= (~DRD);
  Serial.begin(115200);
  randomSeed(0xdeadbeef);
  Serial.println("UNRAVEL THE CODE IV");
  Serial.println("Core Memory Marathon");
  Serial.println("WDKA Digital Craft, Rotterdam & MICA, Baltimore");
  Serial.println("w to write 5 bit string, W to write 8 bit string");
  Serial.println("\nr to read 8 bit string, 5 to read 5 bit string, R to read raw");
  Serial.println("==============================================================");
}

void loop(void)
{
  byte c;
  if(Serial.available() > 0)
  {
    c = Serial.read();
    switch(c)
    {
    case 'W':
      write_string();
      break;
    case 'w':
      write_5bit_string();
      break;
    case 'r':
      read_string(8);
      break;
    case 'R':
      read_all_memory();
      break;
    case '5':
      read_string(5);
      break;
    case '6':
      read_string(6);
      break;
    default:
      Serial.print("Ignoring unknown command: ");
      Serial.print(c, HEX);
      Serial.print(" ");
      Serial.println(c, HEX);
    }
  }
}
