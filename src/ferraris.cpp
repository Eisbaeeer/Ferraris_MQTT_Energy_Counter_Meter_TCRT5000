#include "ferraris.h"
#include <Arduino.h>


// ugly stuff to get interrupt callable
static void IRAM_ATTR staticInterruptHandler0() { Ferraris::getInstance(0).IRQhandler(); }
static void IRAM_ATTR staticInterruptHandler1() { Ferraris::getInstance(1).IRQhandler(); }
static void IRAM_ATTR staticInterruptHandler2() { Ferraris::getInstance(2).IRQhandler(); }
static void IRAM_ATTR staticInterruptHandler3() { Ferraris::getInstance(3).IRQhandler(); }
static void IRAM_ATTR staticInterruptHandler4() { Ferraris::getInstance(4).IRQhandler(); }
static void IRAM_ATTR staticInterruptHandler5() { Ferraris::getInstance(5).IRQhandler(); }
static void IRAM_ATTR staticInterruptHandler6() { Ferraris::getInstance(6).IRQhandler(); }

void (*staticInterruptHandlers[FERRARIS_NUM])() =
{
  &staticInterruptHandler0,
#if FERRARIS_NUM > 1
  &staticInterruptHandler1,
#endif
#if FERRARIS_NUM > 2
  &staticInterruptHandler2,
#endif
#if FERRARIS_NUM > 3
  &staticInterruptHandler3,
#endif
#if FERRARIS_NUM > 4
  &staticInterruptHandler4,
#endif
#if FERRARIS_NUM > 5
  &staticInterruptHandler5,
#endif
#if FERRARIS_NUM > 6
  &staticInterruptHandler6
#endif
};


// initialize private instance counter
uint8_t Ferraris::FINSTANCE = 0;

// private constructor
Ferraris::Ferraris()
  : m_state(startup)
  , m_timestamp(millis())
  , m_revolutions(0)
  , m_rev_kWh(75)
  , m_debounce(80)
  , m_changed(false)
{
  uint8_t F = Ferraris::FINSTANCE++;
  m_PIN = Ferraris::PINS[F];
  pinMode(m_PIN, INPUT_PULLUP);
  switch (F) {
    case 0: m_handler = staticInterruptHandler0; break;
    case 1: m_handler = staticInterruptHandler1; break;
    case 2: m_handler = staticInterruptHandler2; break;
    case 3: m_handler = staticInterruptHandler3; break;
    case 4: m_handler = staticInterruptHandler4; break;
    case 5: m_handler = staticInterruptHandler5; break;
    case 6: m_handler = staticInterruptHandler6; break;
    default: abort();
  }
}

Ferraris& Ferraris::getInstance(unsigned char F)
{
  static Ferraris singleton[FERRARIS_NUM];
  assert(F < FERRARIS_NUM);
  return singleton[F];
}


// ----------------------------------------------------------------------------
// main interface
// ----------------------------------------------------------------------------

void Ferraris::begin()
{
  u_int8_t value = digitalRead(m_PIN);

  if (value == FERRARIS_SILVER) {
    m_state = Ferraris::states::silver_debounce;
  }

  if (value == FERRARIS_RED) {
    m_state = Ferraris::states::red_debounce;
    m_timestampLast1 = m_timestamp;
    m_timestampLast2 = m_timestamp;
  }
  m_changed = true;
}

// define IRQ mode to get to PIN state
#if (FERRARIS_RED == HIGH)
  #define FERRARIS_IRQMODE_RED     RISING
  #define FERRARIS_IRQMODE_SILVER  FALLING
#else
  #define FERRARIS_IRQMODE_RED     FALLING
  #define FERRARIS_IRQMODE_SILVER  RISING
#endif

bool Ferraris::loop()
{
  // wait for debounce time
  int timestamp = millis();
  if (m_state == Ferraris::states::silver_debounce)
    if ((timestamp - m_timestamp) >= 4*m_debounce) {
      m_state = Ferraris::states::silver;
      detachInterrupt(digitalPinToInterrupt(m_PIN));
      attachInterrupt(digitalPinToInterrupt(m_PIN), m_handler, FERRARIS_IRQMODE_RED);
    }  

  if (m_state == Ferraris::states::red_debounce)
    if ((timestamp - m_timestamp) >= m_debounce) {
      if (digitalRead(m_PIN) == FERRARIS_RED) {
        m_state = Ferraris::states::red;
        detachInterrupt(digitalPinToInterrupt(m_PIN));
        attachInterrupt(digitalPinToInterrupt(m_PIN), m_handler, FERRARIS_IRQMODE_SILVER);
      } else {
        // we missed RED -> SILVER !
        m_state = Ferraris::states::silver_debounce;
        m_timestamp = timestamp;
        Serial.println("Missed RED->SILVER !");
      }
    }  

  // return "something has changed" state
  bool retval = m_changed;
  m_changed = false;
  return retval;
}

void Ferraris::IRQhandler()
{
  // disable interrupt during debounce time
  //detachInterrupt(digitalPinToInterrupt(m_PIN));
  m_timestamp = millis();

  // silver -> red
  if (m_state == Ferraris::states::silver) {
    m_state = Ferraris::states::red_debounce;
    m_revolutions++;
    m_timestampLast2 = m_timestampLast1;
    m_timestampLast1 = m_timestamp;
    m_changed = true;
  }  

  // red -> silver
  if (m_state == Ferraris::states::red) {
    m_state = Ferraris::states::silver_debounce;
  }  
}


// ----------------------------------------------------------------------------
// calculation functions
// ----------------------------------------------------------------------------

bool Ferraris::get_state() const
{
  switch (m_state) {
    case Ferraris::states::silver:
    case Ferraris::states::silver_debounce:
      return FERRARIS_SILVER;
      break;
    default:
      return FERRARIS_RED;
  }
}

int Ferraris::get_W() const
{
  unsigned long elapsedtime = m_timestampLast1 - m_timestampLast2;  // last full cycle
  if (elapsedtime == 0) return 0;
  unsigned long runningtime = millis() - m_timestampLast1;          // current open cycle
  // [60 min * 60 sec * 1000 ms] * [1 kW -> 1000 W] / [time for 1kWh]
  return 3600000000 / (std::max(elapsedtime, runningtime) * m_rev_kWh);  
}

float Ferraris::get_kW() const
{
  unsigned long elapsedtime = m_timestampLast1 - m_timestampLast2;  // last full cycle
  if (elapsedtime == 0) return 0.0f;
  unsigned long runningtime = millis() - m_timestampLast1;          // current open cycle
  // [60 min * 60 sec * 1000 ms] * [1 kW] / [time for 1kWh]
  return 3600000.0f / (std::max(elapsedtime, runningtime) * m_rev_kWh);  
}

float Ferraris::get_kWh() const
{
  return float(m_revolutions) / float(m_rev_kWh);
}


// ----------------------------------------------------------------------------
// simple getter and setter
// ----------------------------------------------------------------------------

unsigned long Ferraris::get_revolutions() const
{
  return m_revolutions;
}

void Ferraris::set_revolutions(unsigned long value)
{
  m_revolutions = value;
  m_changed = true;
}

unsigned int Ferraris::get_U_kWh() const
{
  return m_rev_kWh;
}

void Ferraris::set_U_kWh(unsigned int value)
{
  m_rev_kWh = value;
  m_changed = true;
}

unsigned int Ferraris::get_debounce() const
{
  return m_debounce;
}

void Ferraris::set_debounce(unsigned int value)
{
  m_debounce = value;
}
