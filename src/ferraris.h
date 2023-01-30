#pragma once

#include <Arduino.h>


// how many Ferraris meters do we want
#define FERRARIS_NUM 4

// define logic level of pin input
#define FERRARIS_RED     HIGH
#define FERRARIS_SILVER  LOW


class Ferraris {
  public:
  /*
  * Used pins          NodeMCU V2+V3   Wemos D1    Wemos D1 mini
  * Internal LED       <D0> GPIO 16    (IO16)      (D0)
  * IR Pin Messure 1   (D1) GPIO 05    (IO5)       (D1)
  * IR Pin Messure 2   (D2) GPIO 04    (IO4)       (D2)
  * IR Pin Messure 3   (D3) GPIO 00    (IO0)       (D3)
  * ESP8266 LED        <D4> GPIO 02    <IO2>       <D4>
  * IR Pin Messure 4   (D5) GPIO 14    (IO14)      (D5)
  * IR Pin Messure 5   (D6) GPIO 12    (IO12)      (D6)
  * IR Pin Messure 6   (D7) GPIO 13    (IO13)      (D7)
  * IR Pin Messure 7   (D8) GPIO 15                (D8)
  */
    const u_int8_t PINS[7] = {D1, D2, D3, D5, D6, D7, D8};

  private: // singleton pattern: prevent access to constructors
    Ferraris();
    Ferraris(const Ferraris &);
    Ferraris &operator=(const Ferraris &);

  public:
    static Ferraris& getInstance(unsigned char F);
    
    void            begin();
    bool            loop();

    void            IRQhandler();

    bool            get_state() const;  // current PIN state
    int             get_W() const;      // current consumption in W
    float           get_kW() const;     // current consumption in kW
    float           get_kWh() const;    // total reading of meter

    // total revolution count
    unsigned long   get_revolutions() const;
    void            set_revolutions(unsigned long value);

    // config: revolutions per kWh
    unsigned int    get_U_kWh() const;
    void            set_U_kWh(unsigned int value);

    // config: debounce time [ms]
    unsigned int    get_debounce() const;
    void            set_debounce(unsigned int value);

    enum states {startup, silver_debounce, silver, red_debounce, red};

  private:
    uint8_t           m_PIN;
    void              (*m_handler)();
    Ferraris::states  m_state;
    unsigned long     m_timestamp;
    unsigned long     m_timestampLast1;
    unsigned long     m_timestampLast2;
    unsigned long     m_revolutions;
    unsigned int      m_rev_kWh;
    unsigned int      m_debounce;
    bool              m_changed;    // something has changed -> give info in loop()

    static uint8_t    FINSTANCE; // used to identify instance
};

#if (FERRARIS_NUM < 1) || (FERRARIS_NUM > 7)
  #error "Maximum of 7 Farraris meters allowed"
#endif