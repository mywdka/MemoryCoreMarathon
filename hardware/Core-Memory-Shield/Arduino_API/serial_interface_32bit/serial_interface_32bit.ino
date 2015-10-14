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

static unsigned int WRITE_ON_US  = 2;
static unsigned int WRITE_OFF_US = 2; // May not be necessary.

static unsigned long errs = 0;
static unsigned int sense_test_addr = 0;
static int n_test_iters = 1000;
static char report_errors_p = 0;
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

static void maybe_report_error(int i, int j, const char * lbl)
{
  char buf[64];

  if (!report_errors_p)
    return;

  sprintf(buf, "(%o, %o): %s", i, j, lbl);
  Serial.println(buf);
}

static void toggle_error_reporting()
{
  char buf[64];
  report_errors_p = !report_errors_p;
  sprintf(buf, "report-errors-p now %d", report_errors_p);
  Serial.println(buf);
}

static void toggle_tracing()
{
  char buf[64];
  trace_core_calls_p = !trace_core_calls_p;
  sprintf(buf, "trace-core-calls-p now %d", trace_core_calls_p);
  Serial.println(buf);
}

static void current_calibration()
{
  Serial.print("pulsing for current calibration...");
  while (Serial.available() == 0)
  {
    int i;
    for (i = 0; i < WORDSIZE; ++i)
    {
      write_bit(i, 0);
      write_bit(i, 1);
    }
    delayMicroseconds(100);
  }
  Serial.println(" stopped");
  Serial.read(); /* discard typed character */
}

static void timing_exchange_word()
{
  unsigned long val_0 = read_word();
  unsigned long i;
  
  Serial.print("start...");
  for (i = 0; i < 12000; ++i)
  {
    exchg_word(val_0);
    exchg_word(val_0);
    exchg_word(val_0);
    exchg_word(val_0);
    exchg_word(val_0);
    exchg_word(val_0);
    exchg_word(val_0);
    exchg_word(val_0);
    exchg_word(val_0);
    exchg_word(val_0);
  }
  Serial.println("stop");
}

static void echo_comment()
{
  Serial.print("# ");
  char c;
  do {
    while (Serial.available() == 0);
    c = Serial.read();
    Serial.print(c, HEX);
  } while (c != '\r');
  Serial.println("");
}

static void corewise_set_test()
{
  char buf[64];
  for (int b = 0; b < 2; ++b)
  {
    errs = 0;
    for (int n = 0; n < n_test_iters; ++n)
      for (int i = 0; i < WORDSIZE; ++i)
      {
        write_bit(i, b);
        if (read_bit(i) != b) ++errs;
      }
    sprintf(buf, "set %d %lu %d", b, errs, n_test_iters);
    Serial.println(buf);
  }
}

static void corewise_interfere_test()
{
  char buf[64];
  for (int b0 = 0; b0 < 2; ++b0)
  {
    for (int b1 = 0; b1 < 2; ++b1)
    {
      errs = 0;
      for (int n = 0; n < n_test_iters; ++n)
      {
        for (int i = 0; i < WORDSIZE; ++i)
        {
          for (int j = 0; j < WORDSIZE; ++j)
          {
            if (j == i) continue;
            
            write_bit(i, b0);
            write_bit(j, b1);
            if (read_bit(i) != b0) {
              if (report_errors_p)
              {
                // Redundant test but sprintf() takes time.
                sprintf(buf, "%d %d", b0, b1);
                maybe_report_error(i, j, buf);
              }
              ++errs;
            }
          }
        }
      }
      sprintf(buf, "interfere %d %d %lu %d", b0, b1, errs, n_test_iters);
      Serial.println(buf);
    }
  }
}

static void corewise_tests()
{
  corewise_set_test();
  corewise_interfere_test();
}

static void R_test()
{
  unsigned long d = read_word();
  Serial.print("Core data read: 0x");
  Serial.print(d, HEX);
  Serial.print(" = B");
  Serial.println(d, BIN);
}

static void s_test()
{
  char buf[64];
  write_bit(sense_test_addr, 0);
  write_bit(sense_test_addr, 0);
  write_bit(sense_test_addr, 1);
  write_bit(sense_test_addr, 1);
  sprintf(buf, "Pulsed core 0%02o", sense_test_addr);
  Serial.println(buf);
}

static void report_sense_addr()
{
  char buf[64];
  sprintf(buf, "Ready to pulse core 0%02o", sense_test_addr);
  Serial.println(buf);
}

static void a_test()
{
  sense_test_addr += 1;
  sense_test_addr %= 32;
  report_sense_addr();
}

static void A_test()
{
  sense_test_addr -= 1;
  sense_test_addr %= 32;
  report_sense_addr();
}

static void W_test()
{
  int i, x;
  unsigned long d = 0;
  Serial.print("Please enter 8 hexadecimal digits: ");
  for(i = 0; i < 8; i++)
  {
    d <<= 4;
    while(Serial.available() == 0) ;
    x = Serial.read();
    if('0' <= x && x <= '9')
    {
      d += x - '0'; 
    }
    else if('A' <= x && x <= 'F')
    {
      d += x - 'A' + 10;
    }
    else if('a' <= x && x <= 'f')
    {
      d += x - 'a' + 10;
    }
    else
    {
      Serial.print("Assuming 0 for non-hexadecimal digit: ");
      Serial.println(x, HEX);
    }
  }
  write_word(d);
  Serial.print("\r\nCore data write: 0x");
  Serial.print(d, HEX);
  Serial.print(" = B");
  Serial.println(d, BIN);
}

static void X_test()
{
  char buf[64];
  int i, x;
  unsigned long d1, d = 0;
  Serial.print("Please enter 8 hexadecimal digits: ");
  for(i = 0; i < 8; i++)
  {
    d <<= 4;
    while(Serial.available() == 0) ;
    x = Serial.read();
    if('0' <= x && x <= '9')
    {
      d += x - '0'; 
    }
    else if('A' <= x && x <= 'F')
    {
      d += x - 'A' + 10;
    }
    else if ('a' <= x && x <= 'f')
    {
      d += x - 'a' + 10;
    }
    else
    {
      Serial.print("Assuming 0 for non-hexadecimal digit: ");
      Serial.println(x, HEX);
    }
  }
  d1 = exchg_word(d);
  Serial.print("\r\nCore data write: 0x");
  sprintf(buf, "%08lx; read 0x%08lx", d, d1);
  Serial.println(buf);
}

static void r_test()
{
  int i, x, a = 0;
  Serial.print("Please enter address of bit to read: ");
  for(i = 0; i < ADDRSIZE; i++)
  {
    a <<= 1;
    while(Serial.available() == 0) ;
    x = Serial.read();
    if(x != '0') // Assert x == '1'
    {
      a += 1;
    }
  }
  Serial.print("\r\nCore data read from address: ");
  Serial.print(a, BIN);
  Serial.print(" found: ");
  Serial.println(read_bit(a));
}

static void w_test()
{
  int i, x, a = 0;
  Serial.print("Please enter address of bit to write: ");
  for(i = 0; i < ADDRSIZE; i++)
  {
    a <<= 1;
    while(Serial.available() == 0) ;
    x = Serial.read();
    if(x != '0') // Assert x == '1'
    {
      a += 1;
    }
  }
  Serial.print("\r\nPlease enter bit to write: ");
  while(Serial.available() == 0) ;
  x = Serial.read();
  Serial.print("\r\nCore data write to address: ");
  Serial.print(a, BIN);
  Serial.print(" value: ");
  if(x == '0')
  {
    write_bit(a, 0);
    Serial.println("0");
  }
  else // Assert x == '1'
  {
    write_bit(a, 1);
    Serial.println("1");
  }
}

static void t_test()
{
  int i;
  unsigned long d;
  for(i = 0; i < WORDSIZE; i++)
  {
    write_bit(i, 0);
    d = read_bit(i);
    Serial.print("Address (");
    Serial.print(i >> 3);
    Serial.print(i & 7);
    Serial.print(") ");
    Serial.print(i, BIN);
    Serial.print("   wrote 0 read ");
    Serial.print(d);
    write_bit(i, 1);
    d = read_bit(i);
    Serial.print("   wrote 1 read ");
    Serial.println(d);
  }
}

static void T_test()
{
  unsigned long d;
  int i, j;
  Serial.print("log10(iters)? ");
  while(Serial.available() == 0) ;
  int n = Serial.read() - '0';
  if (n < 0 || n > 9)
      Serial.println("bad power-of-10 arg to T");
  else
  {
    unsigned long n_iters = 1, n_iters_done = 0;
    while (n--) n_iters *= 10;
    errs = 0;
    for (n_iters_done = 0; n_iters_done < n_iters; ++n_iters_done)
    {
      for(i = 0; i < WORDSIZE; i++)
      {
        for(j = 0; j < WORDSIZE; j++)
        {
          if(j == i) { continue; }

          write_bit(i, 0);
          d = read_bit(i);
          if(d != 0) { errs++; maybe_report_error(i, j, "0-0"); }
          d = read_bit(i);
          if(d != 0) { errs++; maybe_report_error(i, j, "0-1"); }

          write_bit(i, 0);
          write_bit(j, 0);
          d = read_bit(i);
          if(d != 0) { errs++; maybe_report_error(i, j, "000"); }
          d = read_bit(i);
          if(d != 0) { errs++; maybe_report_error(i, j, "001"); }

          write_bit(i, 0);
          write_bit(j, 1);
          d = read_bit(i);
          if(d != 0) { errs++; maybe_report_error(i, j, "010"); }
          d = read_bit(i);
          if(d != 0) { errs++; maybe_report_error(i, j, "011"); }

          write_bit(i, 1);
          d = read_bit(i);
          if(d != 1) { errs++; maybe_report_error(i, j, "1-0"); }
          d = read_bit(i);
          if(d != 1) { errs++; maybe_report_error(i, j, "1-1"); }

          write_bit(i, 1);
          write_bit(j, 0);
          d = read_bit(i);
          if(d != 1) { errs++; maybe_report_error(i, j, "100"); }
          d = read_bit(i);
          if(d != 1) { errs++; maybe_report_error(i, j, "101"); }

          write_bit(i, 1);
          write_bit(j, 1);
          d = read_bit(i);
          if(d != 1) { errs++; maybe_report_error(i, j, "110"); }
          d = read_bit(i);
          if(d != 1) { errs++; maybe_report_error(i, j, "111"); }
        }
      }
      if (n_iters_done % 100 == 0)
      {
        Serial.print(n_iters_done, DEC);
        Serial.print(" iters; ");
        Serial.print(errs, DEC);
        Serial.println(" errors");
      }
    }
    Serial.print(n_iters_done, DEC);
    Serial.print(" iters; ");
    Serial.print(errs, DEC);
    Serial.println(" errors");
  }
}

static void U_test()
{  
  unsigned long n_iters_done = 0;
  unsigned long prev_datum = 0UL;
  errs = 0;
  write_word(prev_datum);
  while (1)
  {
    if (Serial.available() > 0)
    {
      (void)Serial.read();
      Serial.println("stopping");
      break;
    }
    unsigned rnd_0 = random(0x10000);
    unsigned rnd_1 = random(0x10000);
    unsigned long rnd = (unsigned long)(rnd_1) << 16 | rnd_0;
    unsigned long read_datum = exchg_word(rnd);
//          sprintf(buf, "rnd %08lx; read %08lx", rnd, read_datum);
//          Serial.println(buf);
    if (read_datum != prev_datum)
      ++errs;
    prev_datum = rnd;
    if (n_iters_done % 20000 == 0)
    {
      char buf[64];
      sprintf(buf, "%lu iters-done; %lu errors", n_iters_done, errs);
      Serial.println(buf);
    }
    ++n_iters_done;
  }
}

void setup(void) 
{
  DDRD |= B11111100;
  PORTD = (~ENABLE);
  DDRB |= DWR;
  DDRB &= (~DRD);
  Serial.begin(115200);
  randomSeed(0xdeadbeef);
  Serial.println("Welcome! If you're new, try using the commands 'r', 'w', 't' and 'R', 'W', 'T' to get started.");
}

void loop(void)
{
  byte c;
  if(Serial.available() > 0)
  {
    c = Serial.read();
    switch(c)
    {
    case 'c':
      current_calibration();
      break;
    case '#':
      echo_comment();
      break;
    case 'e':
      corewise_tests();
      break;
    case 'v':
      toggle_tracing();
      break;
    case 'f':
      toggle_error_reporting();
      break;
    case 'm':
      timing_exchange_word();
      break;
    case 'R':
      R_test();
      break;
    case 's':
      s_test();
      break;
    case 'a':
      a_test();
      break;  
    case 'A':
      A_test();
      break;
    case 'W':
      W_test();
      break;
    case 'X':
      X_test();
      break;
    case 'r':
      r_test();
      break;
    case 'w':
      w_test();
      break;
    case 't':
      t_test();
      break;
    case 'T':
      T_test();
      break;
    case 'U':
      U_test();
      break;
    case 'z':
      errs = 0;
      Serial.println("error-count = 0");
      break;
    default:
      Serial.print("Ignoring unknown command: ");
      Serial.print(c, HEX);
      Serial.print(" ");
      Serial.println(c, HEX);
    }
  }
}
