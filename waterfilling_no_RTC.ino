/* The general functionality of the system is as follows: 
(1) The system pumps water at 8am daily to fill the tank at the start of the day, an I2C RTC (DS1307) is used to keep track of time
* EEPROM is updated daily with the data of motor running state for the day. If the motor has pumped water at 8am the 
* run_today flag is True and this the motor only polls the sensor throughout the day to check for any water shortage.
* If the flag is False it means the motor has not run today and thus it is turned on automatically to fill water

(2) After that system polls the float sensors on regular interval using an debounce algorithm 
    to remove any false data and turn on the motor if needed.
    
(3)Incase RTC is not available the system still functions but the start and stop timing of the motor to conserve electricity bill will not work
 */ 

#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <EEPROM.h>

#define motor_relay 6
#define pri_low_sen 11
#define pri_high_sen 12
#define sec_low_sen 10
#define sec_low_led A2
#define pri_low_led A1
#define pri_high_led A0
#define interrupt_button 2
#define start_time 8
#define end_time 17
#define full LOW
#define empty HIGH
#define buzzer 3

byte date_today_addr = 0;                     //declaring eeprom locations for saving motor run dates and status
byte run_today_addr = 1;

void setup() {
  Serial.begin(9600);
  while (!Serial) ; // wait for serial
  Serial.println("DS1307RTC Read Test");
  Serial.println("-------------------");
  pinMode(pri_low_sen, INPUT);
  pinMode(pri_high_sen, INPUT);
  pinMode(sec_low_sen, INPUT);
  pinMode(motor_relay, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(sec_low_led, OUTPUT);
  pinMode(pri_low_led, OUTPUT);
  pinMode(pri_high_led, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(interrupt_button, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), buttonPressed, LOW);

  digitalWrite(sec_low_led, HIGH);        // leds on sequence (helps diagnose if the system is running properly)
  delay(400);
  digitalWrite(pri_low_led, HIGH);
  delay(400);
  digitalWrite(pri_high_led, HIGH);
  delay(400);

  digitalWrite(sec_low_led, LOW);
  digitalWrite(pri_low_led, LOW);
  digitalWrite(pri_high_led, LOW);

  tmElements_t tm_tmp;

  if (RTC.read(tm_tmp)) {
    Serial.print("Ok, Time = ");
    print2digits(tm_tmp.Hour);
    Serial.write(':');
    print2digits(tm_tmp.Minute);
    Serial.write(':');
    print2digits(tm_tmp.Second);
    Serial.print(", Date (D/M/Y) = ");
    Serial.print(tm_tmp.Day);
    Serial.write('/');
    Serial.print(tm_tmp.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(tm_tmp.Year));
    Serial.println();
  }
  else
  {
    if (RTC.chipPresent())
    {
      Serial.println("The DS1307 is stopped.  Please run the SetTime");
      Serial.println("example to initialize the time and begin running.");
      Serial.println();
      for (byte i = 0; i < 2; i++)
      {
        digitalWrite(buzzer, HIGH);                 //test led shows system is functional
        delay(2000);
        digitalWrite(buzzer, LOW);
        delay(2000);
      }
    }
    else
    {
      Serial.println("DS1307 read error!  Please check the circuitry.");
      Serial.println();
      for (byte i = 0; i < 5; i++)
      {
        digitalWrite(buzzer, HIGH);
        delay(500);
        digitalWrite(buzzer, LOW);
        delay(500);
      }

    }
    delay(1000);
  }
}

void loop() {
  Serial.println("Main Loop Entered");
  Serial.println();
  Serial.println("EEPROM DATE");                        //displaying last motor run date and status here
  Serial.println(EEPROM.read(date_today_addr));
  Serial.println();
  Serial.println("EEPROM STATUS");
  Serial.println(EEPROM.read(run_today_addr));
  Serial.println();
  tmElements_t tm;
  digitalWrite(LED_BUILTIN, HIGH);                     //test led shows system is functional
  digitalWrite(sec_low_led, !digitalRead(sec_low_sen));        //leds show current sensor values
  digitalWrite(pri_low_led, !digitalRead(pri_low_sen));
  digitalWrite(pri_high_led, !digitalRead(pri_high_sen));

  if (RTC.read(tm))
  {
    Serial.print("Today Date:");
    Serial.println(tm.Day);
    Serial.println();

    if ((EEPROM.read(date_today_addr) == tm.Day))                                   // function check if motor has been run today , turns it on at the start of the day incase it has not
    {
      if (!(EEPROM.read(run_today_addr)) && ((tm.Hour) >= start_time) && ((tm.Hour) < end_time))
      {
        while ((func_sen_val(pri_high_sen, digitalRead(pri_high_sen)) == empty) && (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full))
        {
          digitalWrite(motor_relay, HIGH);
          Serial.println("motor started with daily 8AM");
          digitalWrite(sec_low_led, !digitalRead(sec_low_sen));
          digitalWrite(pri_low_led, !digitalRead(pri_low_sen));
          digitalWrite(pri_high_led, !digitalRead(pri_high_sen));
        }
        digitalWrite(motor_relay, LOW);
        Serial.println("motor stopped");
        EEPROM.update(run_today_addr, true);
      }
    }
    else
    {
      EEPROM.update(date_today_addr, tm.Day);
      EEPROM.update(run_today_addr, false);
    }
    if (((tm.Hour) >= start_time) && ((tm.Hour) < end_time))                      // normal operation mode of the system checks sensors to see if there is water shortage in primary tank and turn on motor if needed
    {
      if (func_sen_val(pri_low_sen, digitalRead(pri_low_sen)) == empty)
      {
        while ((func_sen_val(pri_high_sen, digitalRead(pri_high_sen)) == empty) && (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full))      //seperate recursive function is used which double checks the sensor value and return it if they are equal
        {
          digitalWrite(motor_relay, HIGH);
          Serial.println("motor started with RTC");
          digitalWrite(sec_low_led, !digitalRead(sec_low_sen));
          digitalWrite(pri_low_led, !digitalRead(pri_low_sen));
          digitalWrite(pri_high_led, !digitalRead(pri_high_sen));
          if (digitalRead(pri_high_sen) == full)
          {
            Serial.write("Primary Tank FULL \n");
          }
          if (digitalRead(sec_low_sen) == empty)
          {
            Serial.write("Secondary Tank LOW \n");
          }
        }
        digitalWrite(motor_relay, LOW);
        Serial.println("motor stopped");
      }
    }
  }

  else
  {
    digitalWrite(buzzer, HIGH);                         //test led shows system is functional
    delay(1000);
    digitalWrite(buzzer, LOW);
    delay(1000);
    Serial.println("RTC Error");

    if (func_sen_val(pri_low_sen, digitalRead(pri_low_sen)) == empty)       //seperate recursive function is used which double checks the sensor value and return it if they are equal
    {
      while ((func_sen_val(pri_high_sen, digitalRead(pri_high_sen)) == empty) & (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full))        //even if RTC is not available or not working the system neglects the start and stop timing of the motor
      {
        digitalWrite(motor_relay, HIGH);
        Serial.println("motor started without RTC");
        digitalWrite(sec_low_led, !digitalRead(sec_low_sen));
        digitalWrite(pri_low_led, !digitalRead(pri_low_sen));
        digitalWrite(pri_high_led, !digitalRead(pri_high_sen));
        digitalWrite(buzzer, HIGH);                 //test led shows system is functional
        delay(1000);
        digitalWrite(buzzer, LOW);
        delay(1000);
        if (digitalRead(pri_high_sen) == full)
        {
          Serial.write("Primary Tank FULL \n");
        }
        if (digitalRead(sec_low_sen) == empty)
        {
          Serial.write("Secondary Tank LOW \n");
        }
      }
      digitalWrite(motor_relay, LOW);
      Serial.println("motor stopped");
    }
  }

  digitalWrite(LED_BUILTIN, LOW);
  delay(2000);
}

void buttonPressed()                      //interrupt service rountine to force the microcontroller using external push button
{
  while ((func_sen_val(pri_high_sen, digitalRead(pri_high_sen)) == empty) & (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full))
  {
    Serial.write("Interrupt entered \n");
    digitalWrite(motor_relay, HIGH);
    digitalWrite(sec_low_led, !digitalRead(sec_low_sen));
    digitalWrite(pri_low_led, !digitalRead(pri_low_sen));
    digitalWrite(pri_high_led, !digitalRead(pri_high_sen));

    if (digitalRead(pri_high_sen) == full)
    {
      Serial.write("Interrupt Primary Tank FULL \n");
    }
    if (digitalRead(sec_low_sen) == empty)
    {
      Serial.write("Interrupt Secondary Tank LOW \n");
    }
  }
  digitalWrite(motor_relay, LOW);
}

boolean func_sen_val(int pin, boolean state)          //recursive function checks the value twice to avoid bounce issues/change sampling interval
{
  delay(200);
  boolean stateNow = digitalRead(pin);
  if (state != stateNow)
  {
    bool x;
    x = func_sen_val(pin, digitalRead(pin));
    return x;
  }
  return stateNow;
}

void print2digits(int number)
{
  if (number >= 0 && number < 10)
  {
    Serial.write('0');
  }
  Serial.print(number);
}
