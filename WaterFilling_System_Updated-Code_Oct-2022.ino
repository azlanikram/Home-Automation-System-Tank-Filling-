/* The general functionality of the system is as follows:
  (1) The system pumps water at 8am daily to fill the tank at the start of the day, an I2C RTC (DS1307) is used to keep track of time
    EEPROM is updated daily with the data of motor running state for the day. If the motor has pumped water at 8am the
    run_today flag is true and this the motor only polls the sensor throughout the day to check for any water shortage.
    If the flag is false it means the motor has not run today and thus it is turned on automatically to fill water

  (2) After that system polls the float sensors on regular interval using an debounce algorithm
    to remove any false data and turn on the motor if needed.

  (3) Incase RTC is not available the system still functions but the start and stop timing of the motor to conserve electricity bill will not work
*/

#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <EEPROM.h>

#define MOTOR_RELAY 6
#define PRI_LOW_SEN 11
#define PRI_HIGH_SEN 12
#define SEC_LOW_SEN 10
#define SEC_LOW_LED A2
#define PRI_LOW_LED A1
#define PRI_HIGH_LED A0
#define INTERRUPT_BUTTON 2
#define START_TIME 8
#define END_TIME 17
#define FULL LOW
#define EMPTY HIGH
#define BUZZER 3

byte date_today_addr = 0;                     //declaring eeprom locations for saving motor run dates and status
byte run_today_addr = 1;
bool force_run = false;

void setup() {
  Serial.begin(9600);
  while (!Serial) ; // wait for serial
  Serial.println("Waiting for reset signal");
  pinMode(PRI_LOW_SEN, INPUT);
  pinMode(PRI_HIGH_SEN, INPUT);
  pinMode(SEC_LOW_SEN, INPUT);
  pinMode(MOTOR_RELAY, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SEC_LOW_LED, OUTPUT);
  pinMode(PRI_LOW_LED, OUTPUT);
  pinMode(PRI_HIGH_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(INTERRUPT_BUTTON, INPUT_PULLUP);


  digitalWrite(SEC_LOW_LED, HIGH);        // LEDs on sequence (helps diagnose if the system is running properly)
  delay(250);
  digitalWrite(PRI_LOW_LED, HIGH);
  delay(250);
  digitalWrite(PRI_HIGH_LED, HIGH);
  delay(250);

  digitalWrite(SEC_LOW_LED, LOW);
  digitalWrite(PRI_LOW_LED, LOW);
  digitalWrite(PRI_HIGH_LED, LOW);

  bool resetTime = false;
  unsigned long startTime = millis();

  // Check if the reset button is pressed for 2 seconds within the first 5 seconds
  while (millis() - startTime < 5000)
  {
    if (func_sen_val(INTERRUPT_BUTTON, digitalRead(INTERRUPT_BUTTON), 2000) == LOW)
    {
      resetTime = true;
      break;
    }
  }

  if (resetTime)
  {
    tmElements_t tm_tmp;
    tm_tmp.Hour = 12;  // Set time to 12:00 PM
    tm_tmp.Minute = 0;
    tm_tmp.Second = 0;
    tm_tmp.Day = 1;    // Arbitrary day (e.g., 1st)
    tm_tmp.Month = 1;  // Arbitrary month (e.g., January)
    tm_tmp.Year = CalendarYrToTm(2025); // Set year to 2024

    if (RTC.write(tm_tmp))
    {
      Serial.println("Time reset to 12:00 PM 1st Jan 2025.");
      EEPROM.update(date_today_addr, 1);
      EEPROM.update(run_today_addr, false);
      digitalWrite(BUZZER, HIGH);
      delay(2000);
      digitalWrite(BUZZER, LOW);
    }
    else
    {
      Serial.println("Failed to reset time!");
    }
  }
  else
  {
    tmElements_t tm_tmp;
    Serial.println("DS1307RTC Read Test");
    Serial.println("-------------------");

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
        for (byte i = 0; i < 3; i++)
        {
          digitalWrite(BUZZER, HIGH);
          delay(500);
          digitalWrite(BUZZER, LOW);
          delay(500);
        }
      }
      else
      {
        Serial.println("DS1307 read error!  Please check the circuitry.");
        Serial.println();
        for (byte i = 0; i < 5; i++)
        {
          digitalWrite(BUZZER, HIGH);
          delay(500);
          digitalWrite(BUZZER, LOW);
          delay(500);
        }
      }
    }
  }
  // Attach interrupt after setup logic
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_BUTTON), buttonPressed, LOW);
}

void loop() {

  int eeprom_date = EEPROM.read(date_today_addr);
  delay(1);
  int eeprom_motor_run = EEPROM.read(run_today_addr);
  delay(1);
  Serial.println("Main Loop Entered\n\n");
  Serial.println("EEPROM DATE");                        //displaying last motor run date and status here
  Serial.println(eeprom_date);
  Serial.println();
  Serial.println("EEPROM STATUS");
  Serial.println(eeprom_motor_run);
  Serial.println();

  tmElements_t tm;
  digitalWrite(SEC_LOW_LED, !digitalRead(SEC_LOW_SEN));        //leds show current sensor values
  digitalWrite(PRI_LOW_LED, !digitalRead(PRI_LOW_SEN));
  digitalWrite(PRI_HIGH_LED, !digitalRead(PRI_HIGH_SEN));

  // Check if force_run flag is set
  if (force_run)
  {
    Serial.println("Force run triggered");
    while ((func_sen_val(PRI_HIGH_SEN, digitalRead(PRI_HIGH_SEN), 200) == EMPTY) && (func_sen_val(SEC_LOW_SEN, digitalRead(SEC_LOW_SEN), 200) == FULL)) 
    {
      digitalWrite(MOTOR_RELAY, HIGH);
      Serial.println("Motor started by force_run");
      digitalWrite(SEC_LOW_LED, !digitalRead(SEC_LOW_SEN));
      digitalWrite(PRI_LOW_LED, !digitalRead(PRI_LOW_SEN));
      digitalWrite(PRI_HIGH_LED, !digitalRead(PRI_HIGH_SEN));
      delay(5000);
    }
    
    digitalWrite(MOTOR_RELAY, LOW);
    Serial.println("Motor stopped after force_run");
    force_run = false; // Reset the flag after motor operation
  }

  if (RTC.read(tm))
  {
    Serial.print("Today RTC Date:");
    Serial.println(tm.Day);
    Serial.println();

    if ((eeprom_date == tm.Day))                                   // function check if motor has been run today , turns it on at the start of the day incase it has not
    {
      if (((tm.Hour) >= START_TIME) && ((tm.Hour) < END_TIME))
      {
        if (!eeprom_motor_run)
        {
          while ((func_sen_val(PRI_HIGH_SEN, digitalRead(PRI_HIGH_SEN), 200) == EMPTY) && (func_sen_val(SEC_LOW_SEN, digitalRead(SEC_LOW_SEN), 200) == FULL))
          {
            digitalWrite(MOTOR_RELAY, HIGH);
            Serial.println("8AM Daily motor started (RTC)");
            digitalWrite(SEC_LOW_LED, !digitalRead(SEC_LOW_SEN));
            digitalWrite(PRI_LOW_LED, !digitalRead(PRI_LOW_SEN));
            digitalWrite(PRI_HIGH_LED, !digitalRead(PRI_HIGH_SEN));
            delay(5000);
          }
          digitalWrite(MOTOR_RELAY, LOW);
          Serial.println("8AM Daily motor stopped (RTC)");
          EEPROM.update(date_today_addr, tm.Day);
          EEPROM.update(run_today_addr, true);
          delay(1);
        }
        else if ((func_sen_val(PRI_HIGH_SEN, digitalRead(PRI_HIGH_SEN), 200) == EMPTY) && (func_sen_val(PRI_LOW_SEN, digitalRead(PRI_LOW_SEN), 200) == EMPTY))
        {
         while ((func_sen_val(PRI_HIGH_SEN, digitalRead(PRI_HIGH_SEN), 200) == EMPTY) && (func_sen_val(SEC_LOW_SEN, digitalRead(SEC_LOW_SEN), 200) == FULL))
          {
            digitalWrite(MOTOR_RELAY, HIGH);
            Serial.println("motor started due to low water level (RTC)");
            digitalWrite(SEC_LOW_LED, !digitalRead(SEC_LOW_SEN));
            digitalWrite(PRI_LOW_LED, !digitalRead(PRI_LOW_SEN));
            digitalWrite(PRI_HIGH_LED, !digitalRead(PRI_HIGH_SEN));
            delay(5000);
          }
          digitalWrite(MOTOR_RELAY, LOW);
          Serial.println("motor stopped due to water level full (RTC)");
        }
      }
    }
    else
    {
      EEPROM.update(date_today_addr, tm.Day);
      delay(1);
      EEPROM.update(run_today_addr, false);
      delay(1);
    }
  }
  
  else
  {
    if ((func_sen_val(PRI_HIGH_SEN, digitalRead(PRI_HIGH_SEN), 200) == EMPTY) && (func_sen_val(PRI_LOW_SEN, digitalRead(PRI_LOW_SEN), 200) == EMPTY))       //seperate recursive function is used which double checks the sensor value and return it if they are equal
    {
      while ((func_sen_val(PRI_HIGH_SEN, digitalRead(PRI_HIGH_SEN), 200) == EMPTY) && (func_sen_val(SEC_LOW_SEN, digitalRead(SEC_LOW_SEN), 200) == FULL))        //even if RTC is not available or not working the system neglects the start and stop timing of the motor
      {
        digitalWrite(MOTOR_RELAY, HIGH);
        Serial.println("motor started (No RTC)");
        digitalWrite(SEC_LOW_LED, !digitalRead(SEC_LOW_SEN));
        digitalWrite(PRI_LOW_LED, !digitalRead(PRI_LOW_SEN));
        digitalWrite(PRI_HIGH_LED, !digitalRead(PRI_HIGH_SEN));
        digitalWrite(BUZZER, HIGH);                         //buzzer for rtc fault
        delay(1000);
        digitalWrite(BUZZER, LOW);
        delay(1000);
        Serial.println("RTC Error");

        if (digitalRead(PRI_HIGH_SEN) == FULL)
        {
          Serial.write("Primary Tank FULL \n");
        }
        if (digitalRead(SEC_LOW_SEN) == EMPTY)
        {
          Serial.write("Secondary Tank LOW \n");
        }
      }
      digitalWrite(MOTOR_RELAY, LOW);
      Serial.println("motor stopped (No RTC)");
    }
  }
  delay(2000);
}

void buttonPressed()                      //interrupt service rountine to force the microcontroller using external push button
{
  Serial.write("Interrupt entered \n");
  if ((func_sen_val(PRI_HIGH_SEN, digitalRead(PRI_HIGH_SEN), 200) == EMPTY) && (func_sen_val(SEC_LOW_SEN, digitalRead(SEC_LOW_SEN), 200) == FULL))
  {
    force_run = true;
  }
}

boolean func_sen_val(int pin, boolean state,int ms)          //recursive function checks the value twice to avoid bounce issues/change sampling interval
{
  delay(ms);
  boolean stateNow = digitalRead(pin);
  if (state != stateNow)
  {
    bool x;
    x = func_sen_val(pin, digitalRead(pin),ms);
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
