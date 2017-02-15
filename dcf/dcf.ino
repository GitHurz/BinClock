#include <TimerOne.h>
#define DCF_HEADER_LENGTH          15
#define DCF_DLST_INFO_LENGTH      (DCF_HEADER_LENGTH + 6)
#define DCF_MINUTE_INFO_LENGTH    (DCF_DLST_INFO_LENGTH + 8)
#define DCF_HOUR_INFO_LENGTH      (DCF_MINUTE_INFO_LENGTH + 7)
#define DCF_DAYMONTH_INFO_LENGTH  (DCF_HOUR_INFO_LENGTH + 14)
#define DCF_YEAR_INFO_LENGTH      (DCF_DAYMONTH_INFO_LENGTH + 8)
#define DCF_ENDbyte               (DCF_YEAR_INFO_LENGTH + 1)

#define DCF_THRESHOLD             130 //Timer of 10ms * counter >= 130ms for 1, lower for 0
#define DCF_NEW_SECOND_THRESHOLD   1000 //> 1s no HIGH = new Second

struct DCF_INFO_STRUCT
{
  word Header;
  word DayMonth;
  byte DST;
  byte Minute;
  byte Hour;
  byte Year;
  byte Counter;
};

struct DCF_DECODED_STRUCT
{
  byte Minute;
  byte Hour;
  byte DayOfMonth;
  byte DayOfWeek;
  byte Month;
  byte Year;
  bool DST;
  bool IsValid;
};

DCF_INFO_STRUCT DCF_Info;
DCF_DECODED_STRUCT DCF_Decoded;
DCF_DECODED_STRUCT DCF_Last_Decoded;

volatile int pulseCount = 0;
volatile int noSignalCount = 0;
volatile bool doCount = false;
const byte dcfPin = 2;
const byte dcfOnPin = 7;

int DecodedCount = 0;

void setup() 
{
  memset(&DCF_Info, 0, sizeof(DCF_INFO_STRUCT));
  memset(&DCF_Decoded, 0, sizeof(DCF_DECODED_STRUCT));
  memset(&DCF_Last_Decoded, 0, sizeof(DCF_DECODED_STRUCT));
  
  pinMode(dcfOnPin,OUTPUT);
  pinMode(dcfPin,INPUT);
  digitalWrite(dcfOnPin, LOW);
  
  Timer1.initialize(1000);
  Timer1.attachInterrupt(TimerInterrupt);
  attachInterrupt(digitalPinToInterrupt(dcfPin), DCF_Signal, CHANGE);  
  Serial.begin(9600);
}

void DCF_Signal()
{  
  
  if(digitalRead(dcfPin) == HIGH)
  {        
    if(noSignalCount >= DCF_NEW_SECOND_THRESHOLD)
    {      
      ResetDCFInfo();
    }
    doCount = true;
    pulseCount = 0;
    noSignalCount = 0;
  }
  else
  {
    doCount = false;    
    EncodeDCF_Telegram((pulseCount < DCF_THRESHOLD) ? 0 : 1);
  }
}

void loop() 
{
  // put your main code here, to run repeatedly:  
  if(DecodedCount > 0)
  {    
    if(DCF_Last_Decoded.IsValid)
    {
      Serial.print(DCF_Last_Decoded.Hour); Serial.print(":"); Serial.print(DCF_Last_Decoded.Minute); Serial.print(" "); Serial.print(DCF_Last_Decoded.DayOfMonth); Serial.print("."); Serial.print(DCF_Last_Decoded.Month); Serial.print("."); Serial.print(DCF_Last_Decoded.Year);
    }
    else
      Serial.println("Signal Invalid");
  }
  else
  {
    Serial.println("No Signal yet");    
  }  
  delay(1500);
}

void TimerInterrupt()
{
  //Serial.println("HitInterrupt");
  if(doCount)
  {
    pulseCount++;  
    //Serial.println("DidCount");
  }
  else
    noSignalCount++;
}

void ResetDCFInfo()
{
  Serial.println("HitReset");
  DecodeDCF_Telegram(); 
  
  DCF_Last_Decoded = DCF_Decoded;
  DecodedCount++;
  
  memset(&DCF_Info, 0, sizeof(DCF_INFO_STRUCT));
  memset(&DCF_Decoded, 0, sizeof(DCF_DECODED_STRUCT));
}

void EncodeDCF_Telegram(byte p_Signal)
{ 
  if (DCF_Info.Counter < DCF_HEADER_LENGTH)
    DCF_Info.Header |= (p_Signal << DCF_Info.Counter);

  if (DCF_Info.Counter < DCF_DLST_INFO_LENGTH && DCF_Info.Counter >= DCF_HEADER_LENGTH)
    DCF_Info.DST |= (p_Signal << (DCF_Info.Counter - DCF_HEADER_LENGTH));

  if (DCF_Info.Counter < DCF_MINUTE_INFO_LENGTH && DCF_Info.Counter >= DCF_DLST_INFO_LENGTH)
    DCF_Info.Minute |= (p_Signal << (DCF_Info.Counter - DCF_DLST_INFO_LENGTH));

  if (DCF_Info.Counter < DCF_HOUR_INFO_LENGTH && DCF_Info.Counter >= DCF_MINUTE_INFO_LENGTH)
    DCF_Info.Hour |= (p_Signal << (DCF_Info.Counter - DCF_MINUTE_INFO_LENGTH));

  if (DCF_Info.Counter < DCF_DAYMONTH_INFO_LENGTH && DCF_Info.Counter >= DCF_HOUR_INFO_LENGTH)
    DCF_Info.DayMonth |= (p_Signal << (DCF_Info.Counter - DCF_HOUR_INFO_LENGTH));

  if (DCF_Info.Counter < DCF_YEAR_INFO_LENGTH && DCF_Info.Counter >= DCF_DAYMONTH_INFO_LENGTH)
    DCF_Info.Year |= (p_Signal << (DCF_Info.Counter - DCF_DAYMONTH_INFO_LENGTH));

  if (DCF_Info.Counter >= (DCF_ENDbyte - 1))
  {
    DCF_Info.DayMonth |= (p_Signal << 15); //Set MSB of DayMonth for Parity of Day/Month/Year Block, Hack to save one Byte.
    //Telegram finished. Decode and save
    ResetDCFInfo();
  }
  DCF_Info.Counter += 1;    
}

byte ParityCount(byte p_Check)
{
  byte l_Ret = 0;
  for (int i = 0; i < 8; i++)
    l_Ret += ((p_Check >> i) & 1);
  return l_Ret;
}
byte DecodeBCD(byte p_Decode)
{
  return (p_Decode & 15) + ((p_Decode >> 4) * 10);
}

void CheckTelegram()
{
  DCF_Decoded.IsValid = (DCF_Info.Header & 1) == 0; //1st Bit Allways 0
  DCF_Decoded.IsValid = (DCF_Info.DST & 32) == 32; //20th Bit  Allways 1
  DCF_Decoded.IsValid &= ((DCF_Info.DST & 4) == 4) || ((DCF_Info.DST & 8) == 8); //Only true when one of bot CET or CEST is set 
  DCF_Decoded.IsValid &= (ParityCount(DCF_Info.Minute) % 2 == 0);
  DCF_Decoded.IsValid &= (ParityCount(DCF_Info.Hour) % 2 == 0);
  DCF_Decoded.IsValid &= ((ParityCount(highByte(DCF_Info.DayMonth)) + ParityCount(lowByte(DCF_Info.DayMonth)) + ParityCount(DCF_Info.Year)) % 2 == 0); 
  DCF_Decoded.IsValid &= DCF_Decoded.Minute < 60;
  DCF_Decoded.IsValid &= DCF_Decoded.Hour < 24;
  DCF_Decoded.IsValid &= (DCF_Decoded.DayOfMonth > 0 && DCF_Decoded.DayOfMonth < 32);
  DCF_Decoded.IsValid &= DCF_Decoded.DayOfWeek > 0;
  DCF_Decoded.IsValid &= (DCF_Decoded.Month > 0 && DCF_Decoded.Month < 13);
  DCF_Decoded.IsValid &= DCF_Decoded.Year < 100;
}
void DecodeDCF_Telegram()
{
  byte l_Buf;
    
  DCF_Decoded.Minute = DecodeBCD(DCF_Info.Minute & 127);
  DCF_Decoded.Hour = DecodeBCD(DCF_Info.Hour & 63);

  l_Buf = DCF_Info.DayMonth &63;
  DCF_Decoded.DayOfMonth = DecodeBCD(l_Buf);

  l_Buf = DCF_Info.DayMonth >> 6;
  DCF_Decoded.DayOfWeek = l_Buf & 7; 

  l_Buf = DCF_Info.DayMonth >> 9;
  DCF_Decoded.Month = DecodeBCD(l_Buf & 31);

  DCF_Decoded.Year = DecodeBCD(DCF_Info.Year);

  CheckTelegram();  
}
