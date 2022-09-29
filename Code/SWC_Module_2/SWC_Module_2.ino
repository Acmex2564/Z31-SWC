#include <mcp_can.h>
#include <SPI.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#define INPUT_PIN 2
//#define LED_PIN 13
#define BIT_TIME 1000 //microseconds
#define BIT_COUNT 12
#define TIMEOUT 15000 //microseconds
#define PRINT_PERIOD 1000000
#define HISTORY_COUNT 5

#define CAN_INT 8
#define CAN_CS 10

#define SHIFT_CS 5

#define MODULE_TEMP A6
#define MODULE_VOLTAGE A7

#define AUX_OUT_1 18
#define AUX_OUT_2 3 //pwm enabled
#define SWC_POWER 17
#define CRUISE_ACCEL 15 //L/Y --
#define CRUISE_SET 14 //L/OR
#define CRUISE_RESUME 16 //L/G
#define CRUISE_RELAY 19
#define CAN_RESET 9

#define BRIGHTNESS_ID 0x202
#define SWC_ID 0x1D6


volatile bool servicing = false;
int bitCounter = 0;
int serialData[12];
volatile long timeLastFall;
long timeCurrent;
unsigned long timeCurrent2;
long timeLastBit;
long timeLastPrint;
byte pinState;

const byte defaultCruise = 0b00000101;
byte stateCruise         = 0b00000101;

const byte defaultMedia  = 0b00000000;
byte stateMedia          = 0b00000000;

byte stateVolume         = 0b00000000;

bool stateValidButtons = false;

const uint32_t CAN_FREQ = 8000000; // 8 MHz
const uint32_t CAN_RATE = 500000; // 500 kbit/s
MCP_CAN CAN0(CAN_CS);
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
byte txBufSWC[3];
byte lastBufSWC[3];
char msgString[128];

int histCounter = 0;
int history[3][HISTORY_COUNT];

int histSum[3] = { defaultCruise * HISTORY_COUNT, defaultMedia * HISTORY_COUNT, 0} ;

const int interval_max_ign = 10 * 1000;
long timeLastIgn;
long timeLastCan;
boolean broadcast = false;

const int interval_swc_repeat = 500;
long timeLastSwc;

void setup() {
  // put your setup code here, to run once:
  pinMode(INPUT_PIN, INPUT);
  pinMode(CAN_INT, INPUT);
  pinMode(CAN_CS, OUTPUT);
  pinMode(CAN_RESET, OUTPUT);
  digitalWrite(CAN_RESET, LOW);
  pinMode(SHIFT_CS, OUTPUT);
  pinMode(AUX_OUT_1, OUTPUT);
  pinMode(AUX_OUT_2, OUTPUT);
  pinMode(SWC_POWER, OUTPUT);
  pinMode(CRUISE_ACCEL, OUTPUT);
  pinMode(CRUISE_SET, OUTPUT);
  pinMode(CRUISE_RESUME, OUTPUT);
  pinMode(CRUISE_RELAY, OUTPUT);


  Serial.begin(115200);
  Serial.println("Alive");


  for (int i = 0; i <= (HISTORY_COUNT - 1); i++) {
    history[0][i] = defaultCruise;
    history[1][i] = defaultMedia;
    history[2][i] = 0;
  }


  digitalWrite(CAN_RESET, HIGH);
  delay(10);

  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
  {
    Serial.println("MCP2515 Initialized Successfully!");
  }
  else
  {
    Serial.println("Error Initializing MCP2515...");
  }
  CAN0.setMode(MCP_NORMAL);
  // set can transciever into normal mode
  CAN0.setGPO(0b00000011); //EN and STB HIGH

  attachInterrupt(digitalPinToInterrupt(INPUT_PIN), interrupt_svc, FALLING);
  digitalWrite(SWC_POWER, HIGH);

  wdt_enable(WDTO_8S);
  Serial.println("Awaiting SWC");
}

void loop() {
  // put your main code here, to run repeatedly:
  timeCurrent = micros();


  if (servicing) {

    if (timeCurrent - timeLastBit >= BIT_TIME) {
      //      digitalWrite(LED_PIN, HIGH);
      pinState = digitalRead(INPUT_PIN);
      //      digitalWrite(LED_PIN, LOW);
      if (pinState == HIGH) {
        serialData[bitCounter] = 1;
      }
      else {
        serialData[bitCounter] = 0;
      }
      bitCounter++;
      timeLastBit = timeCurrent;
    }

    //If the char has been filled, or if the timeout period has elapsed, reset to nothing
    if  ((bitCounter >= BIT_COUNT) || ((timeCurrent - timeLastFall) >= TIMEOUT)) {
      servicing = false;

      if (bitCounter == BIT_COUNT) {

        bitWrite(stateCruise, 2, serialData[1]);
        bitWrite(stateCruise, 1, serialData[2]);
        bitWrite(stateCruise, 0, serialData[3]);

        bitWrite(stateMedia, 2, serialData[4]);
        bitWrite(stateMedia, 1, serialData[5]);
        bitWrite(stateMedia, 0, serialData[6]);

        stateVolume = 0;

        bitWrite(stateVolume, 4, serialData[7]);
        bitWrite(stateVolume, 3, serialData[8]);
        bitWrite(stateVolume, 2, serialData[9]);
        bitWrite(stateVolume, 1, serialData[10]);
        bitWrite(stateVolume, 0, serialData[11]);

        stateVolume = map(stateVolume, 0, 32, 100, 0);

        stateValidButtons = true;
      }
      if ((timeCurrent - timeLastFall) >= TIMEOUT) {
        Serial.println("Timeout");
        stateCruise = defaultCruise;
        stateMedia = defaultMedia;
        stateValidButtons = true;
      }
      bitCounter = 0;
    }

  }

  //canbus shit goes here
  else {
    if ((analogRead(MODULE_VOLTAGE)*5/1023.0 * ((35.7+150) / 35.7)) > 5.0) {
      timeLastIgn = millis(); 
    }

    if (((millis() - timeLastIgn > interval_max_ign) && (millis() - timeLastCan > interval_max_ign )) || (millis() - timeLastIgn > 5 * interval_max_ign)) {
      poweroff();
    }

    if (millis() - timeLastIgn < interval_max_ign/2) {
      broadcast = true;
    }
    else {
      broadcast = false;
    }
      

    if (stateValidButtons) {

      histSum[0] = (histSum[0] - history[0][histCounter]) + stateCruise;
      history[0][histCounter] = stateCruise;
      histSum[1] = (histSum[1] - history[1][histCounter]) + stateMedia;
      history[1][histCounter] = stateMedia;
      histSum[2] = (histSum[2] - history[2][histCounter]) + stateVolume;
      history[2][histCounter] = stateVolume;
      //Serial.println(histCounter);
      //Serial.print(history[0][0]); Serial.print(history[0][1]); Serial.print(history[0][2]); Serial.print(history[0][3]); Serial.println(history[0][4]);
      //Serial.println(histSum[0]);
      if (histCounter >= (HISTORY_COUNT - 1))
      {
        histCounter = 0;
      }
      else
      {
        histCounter++;
      }

      if ((stateVolume == (histSum[2] / HISTORY_COUNT )) & (stateMedia == (histSum[1] / HISTORY_COUNT)) & (stateCruise == (histSum[0] / HISTORY_COUNT)))
      {
        txBufSWC[1] = stateMedia;
        txBufSWC[2] = stateVolume;
        txBufSWC[0] = stateCruise;
      }
      else
      {
        Serial.println("History Mismatch.");
        Serial.print(stateVolume); Serial.println(histSum[2] / HISTORY_COUNT);
        Serial.print(stateMedia); Serial.println(histSum[1] / HISTORY_COUNT);
        Serial.print(stateCruise); Serial.println(histSum[0] / HISTORY_COUNT);
        Serial.println();

        txBufSWC[1] = defaultMedia;
        txBufSWC[2] = defaultCruise;
        stateCruise = defaultCruise;

      }


      if (((txBufSWC[0] != lastBufSWC[0]) || (txBufSWC[1] != lastBufSWC[1]) || (txBufSWC[2] != lastBufSWC[2]) || (millis() - timeLastSwc > interval_swc_repeat)) && broadcast)
      {
        if (CAN0.sendMsgBuf(SWC_ID, 0, 3, txBufSWC) == CAN_OK) {
          lastBufSWC[0] = txBufSWC[0];
          lastBufSWC[1] = txBufSWC[1];
          lastBufSWC[2] = txBufSWC[2];
          timeLastSwc = millis();
        }
        else {
          Serial.println("Tx SWC Failure");
          Serial.println(CAN0.sendMsgBuf(SWC_ID, 0, 3, txBufSWC));
        }
      }

      if (stateCruise == defaultCruise) {
        digitalWrite(CRUISE_ACCEL, LOW);
        digitalWrite(CRUISE_SET, LOW);
        digitalWrite(CRUISE_RESUME, LOW);
      }
      else if (stateCruise == 0b00000010) {
        digitalWrite(CRUISE_ACCEL, HIGH);
        digitalWrite(CRUISE_SET, LOW);
        digitalWrite(CRUISE_RESUME, LOW);
        Serial.println("Cruise Accel Pressed!");
      }
      else if (stateCruise == 0b00000011) {
        digitalWrite(CRUISE_ACCEL, LOW);
        digitalWrite(CRUISE_SET, LOW);
        digitalWrite(CRUISE_RESUME, HIGH);
        Serial.println("Cruise Resume Pressed!");
      }
      else if (stateCruise == 0b00000100) {
        digitalWrite(CRUISE_ACCEL, LOW);
        digitalWrite(CRUISE_SET, HIGH);
        digitalWrite(CRUISE_RESUME, LOW);
        Serial.println("Cruise Set Pressed!");
      }
      else {
        digitalWrite(CRUISE_ACCEL, LOW);
        digitalWrite(CRUISE_SET, LOW);
        digitalWrite(CRUISE_RESUME, LOW);
      }

      stateValidButtons = false;
    }

    //read for brightness message
    if (!digitalRead(CAN_INT))
    {
      CAN0.readMsgBuf(&rxId, &len, rxBuf);
      timeLastCan = millis();

      if (rxId == 0x202) {
        Serial.print("Recv'd brightness msg: ");

        for (byte i = 0; i < len; i++) {
          sprintf(msgString, "0x%.2x", rxBuf[i]);
          Serial.print(msgString);
        }
        Serial.println();
                
        if (rxBuf[0] == 0xFE) {
          analogWrite(AUX_OUT_2, 0xFF);
        }
        else {
          analogWrite(AUX_OUT_2, map(rxBuf[0], 0x0, 0xFD, 0xFF, 0x0));
        }

      }
    }
  }
wdt_reset();
}

void poweroff() {
  Serial.println("Poweroff");
  digitalWrite(SWC_POWER, LOW);
  CAN0.setGPO(0b00000001);
  delay(1000);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_mode();
}

void interrupt_svc() {

  servicing = true;
  timeLastFall = micros();
  delayMicroseconds(BIT_TIME / 2);
}
