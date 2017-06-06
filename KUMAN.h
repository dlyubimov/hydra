#ifndef KUMAN_H_INCLUDED
#define KUMAN_H_INCLUDED

#include <LiquidCrystal.h>

#define RIGHT 0x04
#define UP 0x08
#define DOWN 0x10
#define LEFT 0x02
#define SELECT 0x01
#define BUTTON_PIN 0

#define KUMAN_ADDR 8,9,4,5,6,7


class KUMAN: public LiquidCrystal {

  public :

  KUMAN () : LiquidCrystal(KUMAN_ADDR) { }

  void setBacklight(uint8_t status) { }
  
  uint8_t readButtons() { 
    int r = analogRead(BUTTON_PIN);

    if ( r > 1000 ) return 0; //  no button

    // otherwise, work with ranges which roughly follow powers of 2.
    
    if ( (r >>= 6) == 0) return RIGHT;
    if ( (r >>= 1) == 0) return UP;
    if ( (r >>= 1) == 0) return DOWN;
    if ( (r >>= 1) == 0) return LEFT;
    return SELECT;
  }
    

  void setMCPType(uint8_t mcptype) {}

  
  
};


#endif // KUMAN_H_INCLUDED
