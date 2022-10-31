#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>

#define motor_relay 6
#define pri_low_sen 11
#define pri_high_sen 12
#define sec_low_sen 10
#define sec_low_led A2
#define pri_low_led A1
#define pri_high_led A0
#define interrupt_button 2
#define start_time 8
#define end_time 19
#define full LOW
#define empty HIGH
#define buzzer 3

short date_today;       //stores todays date
bool run_today = false; //flag showing if the motor turned on today.


void setup() {
  Serial.begin(9600);
  while (!Serial) ; // wait for serial
  delay(200);
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

  tmElements_t tm1;

  if (RTC.read(tm1)) {
    Serial.print("Ok, Time = ");
    print2digits(tm1.Hour);
    Serial.write(':');
    print2digits(tm1.Minute);
    Serial.write(':');
    print2digits(tm1.Second);
    Serial.print(", Date (D/M/Y) = ");
    Serial.print(tm1.Day);
    Serial.write('/');
    Serial.print(tm1.Month);
    Serial.write('/');
    Serial.print(tmYearToCalendar(tm1.Year));
    Serial.println();
    date_today = int(tm1.Day);
    Serial.print("Today Date:");
    Serial.println(date_today);
  }
  else
  {
    if (RTC.chipPresent())
    {
      Serial.println("The DS1307 is stopped.  Please run the SetTime");
      Serial.println("example to initialize the time and begin running.");
      Serial.println();
      for (int i = 0; i < 2; i++)
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
      for (int i = 0; i < 5; i++)
      {
        digitalWrite(buzzer, HIGH);
        delay(500);
        digitalWrite(buzzer, LOW);
        delay(500);
      }

    }
    delay(2000);
  }
}

void loop() {
  Serial.println("Main Loop Entered");
  Serial.println();
  digitalWrite(LED_BUILTIN, HIGH);                     //test led shows system is functional
  delay(500);
  digitalWrite(sec_low_led, !digitalRead(sec_low_sen));        //leds show current sensor values
  digitalWrite(pri_low_led, !digitalRead(pri_low_sen));
  digitalWrite(pri_high_led, !digitalRead(pri_high_sen));

  tmElements_t tm;

  if (date_today != tm.Day)
  {
    run_today = false;
    date_today = tm.Day;
  }
  if ((!run_today) && ((tm.Hour) >= start_time) && ((tm.Hour) < end_time))                      //function for turning on machine daily at the start of the day
  {
    while ((func_sen_val(pri_high_sen, digitalRead(pri_high_sen)) == empty) & (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full))
    {
      digitalWrite(motor_relay, HIGH);
      Serial.println("motor started with daily 8AM");
      digitalWrite(sec_low_led, !digitalRead(sec_low_sen));
      digitalWrite(pri_low_led, !digitalRead(pri_low_sen));
      digitalWrite(pri_high_led, !digitalRead(pri_high_sen));
    }
    digitalWrite(motor_relay, LOW);
    run_today = true;
  }

  if (RTC.read(tm))
  {
    if (((tm.Hour) >= start_time) && ((tm.Hour) < end_time))
    {
      if (func_sen_val(pri_low_sen, digitalRead(pri_low_sen)) == empty)  //seperate recursive function is used which double checks the sensor value and return it if they are equal
      {
        if (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full)
        {
          while ((func_sen_val(pri_high_sen, digitalRead(pri_high_sen)) == empty) & (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full))
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
  }

  else
  {
    digitalWrite(buzzer, HIGH);                 //test led shows system is functional
    delay(1000);
    digitalWrite(buzzer, LOW);
    delay(1000);
    if (func_sen_val(pri_low_sen, digitalRead(pri_low_sen)) == empty)  //seperate recursive function is used which double checks the sensor value and return it if they are equal
    {
      if (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full)
      {
        while ((func_sen_val(pri_high_sen, digitalRead(pri_high_sen)) == empty) & (func_sen_val(sec_low_sen, digitalRead(sec_low_sen)) == full))
        {
          digitalWrite(motor_relay, HIGH);
          Serial.println("motor started without RTC");
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

  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
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
{ delay(200);
  boolean stateNow = digitalRead(pin);
  if (state != stateNow)
  {
    bool x;
    x = func_sen_val(pin, digitalRead(pin));
    return x;
  }
  return stateNow;
}

void print2digits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}
