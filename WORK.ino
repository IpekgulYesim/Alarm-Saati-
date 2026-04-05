//---------------- KUTUPHANELER -----------
#include <Wire.h> //LCD iletisimi
#include <RTClib.h> //DS3231 için
#include <LiquidCrystal.h> //LCD icin (LCD = LiquidCrystalDisplay)

// ---------------- PINLER ----------------
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); 
RTC_DS3231 rtc;

const int BTN_ARTIRMA   = A0;  
const int BTN_AZALT   = A1;  
const int BTN_DUZENLE  = A2;  
const int BTN_ALARM = A3;  
const int BUZZER    = 3;   
const int KESME_PIN   = 2;   

// ---------------- DEGISKENLER --------------
volatile bool rtcAlarmISR_Flag = false; //kesmeyi belirleyecek
bool alarmCaliyorMu = false;

int alarmSaati = 6;
int alarmDakika = 30;
bool alarmAktifMi = true;

int duzenlemeModu = 0;

// buzzerın ana durumu
unsigned long enSonkiTonDegisimi = 0;
int alarmToneState = 0;

// LCD yanıp sönmesi
unsigned long lcdFlashTimer = 0;
bool lcdVisible = true;

// 10 saniye sonra otomatik alarm kapatma için
unsigned long alarmBaslamaZamani = 0; 

// ---------------- FONKSIYONLAR -----------------
void setRTCAlarmNext();
void clearDS3231AlarmFlag();
bool buttonPressed(int pin);
void handleAlarmTone();
void handleLCDFlash(bool ringing);

// ---------------- ISR -----------------
void rtcAlarmISR() 
{
  rtcAlarmISR_Flag = true;
}

void setup() 
{
  Wire.begin();
  lcd.begin(16, 2);

  pinMode(BTN_ARTIRMA, INPUT_PULLUP);
  pinMode(BTN_AZALT, INPUT_PULLUP);
  pinMode(BTN_DUZENLE, INPUT_PULLUP);
  pinMode(BTN_ALARM, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(KESME_PIN, INPUT_PULLUP);

  if (!rtc.begin()) 
  {
    lcd.clear();
    lcd.print("RTC ERROR");
    while (1);
  }

  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.writeSqwPinMode(DS3231_OFF);

  setRTCAlarmNext();
  //ALARM KISMI 
  attachInterrupt(digitalPinToInterrupt(KESME_PIN), rtcAlarmISR, FALLING); 
  //D2deki saat alarma eşitlenince Alarm 1 flagi etkinleştirilecek, rtcAlarmISR = true, akış kesilerek A3te bağlanan butona basılana kadar 
  //(veya 10 sn boyunca) alarm çalmaya devam edecek

  lcd.clear();
}

void loop() 
{
  DateTime now = rtc.now();

  // --------- KESME ISLEMI ---------
  if (rtcAlarmISR_Flag) {
    rtcAlarmISR_Flag = false;

    if (alarmAktifMi) 
    {
      alarmCaliyorMu = true;
      alarmBaslamaZamani = millis();  
    }

    clearDS3231AlarmFlag();
    setRTCAlarmNext();
  }

  // --------- INTERRUPT FALLBACK ---------
  if (!alarmCaliyorMu && alarmAktifMi &&
      now.hour() == alarmSaati &&
      now.minute() == alarmDakika &&
      now.second() == 0) {

    alarmCaliyorMu = true;
    alarmBaslamaZamani = millis(); 

    clearDS3231AlarmFlag();
    setRTCAlarmNext();
  }

  // --------- 10 SANIYE SONRA OTOMATIK KAPAT ---------
  if (alarmCaliyorMu) {
    if (millis() - alarmBaslamaZamani >= 10000) { // 10 saniye
      alarmCaliyorMu = false;
      noTone(BUZZER);
    }
  } //opsiyonel, uzatilabilir, kisaltilabilir veya cikarilabilir

  // ---------------- BUTTON YONETIMI ----------------
  if (buttonPressed(BTN_DUZENLE)) {
    duzenlemeModu++;
    if (duzenlemeModu > 4) duzenlemeModu = 0;
  }

  // AYARLAMALAR
  if (duzenlemeModu == 1) { 
    if (buttonPressed(BTN_ARTIRMA)) {
      DateTime t = rtc.now();
      t = DateTime(t.year(), t.month(), t.day(), (t.hour() + 1) % 24, t.minute(), t.second());
      rtc.adjust(t);
    }
    if (buttonPressed(BTN_AZALT)) {
      DateTime t = rtc.now();
      t = DateTime(t.year(), t.month(), t.day(), (t.hour() + 23) % 24, t.minute(), t.second());
      rtc.adjust(t);
    }
  } else if (duzenlemeModu == 2) {
    if (buttonPressed(BTN_ARTIRMA)) {
      DateTime t = rtc.now();
      t = DateTime(t.year(), t.month(), t.day(), t.hour(), (t.minute() + 1) % 60, t.second());
      rtc.adjust(t);
    }
    if (buttonPressed(BTN_AZALT)) {
      DateTime t = rtc.now();
      t = DateTime(t.year(), t.month(), t.day(), t.hour(), (t.minute() + 59) % 60, t.second());
      rtc.adjust(t);
    }
  } else if (duzenlemeModu == 3) {
    if (buttonPressed(BTN_ARTIRMA)) {
      alarmSaati = (alarmSaati + 1) % 24;
      setRTCAlarmNext();
    }
    if (buttonPressed(BTN_AZALT)) {
      alarmSaati = (alarmSaati + 23) % 24;
      setRTCAlarmNext();
    }
  } else if (duzenlemeModu == 4) {
    if (buttonPressed(BTN_ARTIRMA)) {
      alarmDakika = (alarmDakika + 1) % 60;
      setRTCAlarmNext();
    }
    if (buttonPressed(BTN_AZALT)) {
      alarmDakika = (alarmDakika + 59) % 60;
      setRTCAlarmNext();
    }
  }

  // Butonla alarmı kapatma/Açma
  if (buttonPressed(BTN_ALARM)) {
    if (alarmCaliyorMu) {
      alarmCaliyorMu = false;
      noTone(BUZZER);
    } else {
      alarmAktifMi = !alarmAktifMi;
      if (alarmAktifMi) setRTCAlarmNext();
      else rtc.clearAlarm(1);
    }
  }

  // buzzer yönetimi
  handleAlarmTone();

  // LCD flash yönetimi
  handleLCDFlash(alarmCaliyorMu);

  // ---------------- LCD ----------------
  if (lcdVisible) {
    lcd.setCursor(0, 0);
    lcd.print("Saat ");
    if (now.hour() < 10) lcd.print('0'); 
    lcd.print(now.hour());
    lcd.print(':');
    if (now.minute() < 10) lcd.print('0'); 
    lcd.print(now.minute());
    lcd.print(':');
    if (now.second() < 10) lcd.print('0'); 
    lcd.print(now.second());
    lcd.print("  ");

    lcd.setCursor(0, 1);
    if (alarmCaliyorMu) {
      lcd.print("ALARM KESMESI ");
    } 
    else if (duzenlemeModu == 1) {
      lcd.print(" Zaman Saati ");
      if(now.hour() < 10) lcd.print('0');
      lcd.print(now.hour());
    }
    else if (duzenlemeModu == 2) {
      lcd.print("Zaman Dakika ");
      if(now.minute() < 10) lcd.print('0');
      lcd.print(now.minute());
    }
    else if (duzenlemeModu == 3) {
      lcd.print(" Alarm Saati ");
      if (alarmSaati < 10) lcd.print('0');
      lcd.print(alarmSaati);
    }
    else if (duzenlemeModu == 4) {
      lcd.print("Alarm Dakika ");
      if (alarmDakika < 10) lcd.print('0');
      lcd.print(alarmDakika);
    }
    else 
    {
      lcd.print("Alarm ");
      if(alarmSaati < 10) lcd.print('0');  
      lcd.print(alarmSaati);
      lcd.print(":");
      if(alarmDakika < 10) lcd.print('0');
      lcd.print(alarmDakika);
      lcd.print(alarmAktifMi ? " ACK " : " KPL ");
    }
  }
  delay(100);
}

// ---------------- BUTTON HELPER -----------------
bool buttonPressed(int pin) {
  if (digitalRead(pin) == LOW) {
    delay(60);
    while (digitalRead(pin) == LOW);
    delay(30);
    return true;
  }
  return false;
}

// ---------------- BUZZER -----------------
void handleAlarmTone() {
  if (!alarmCaliyorMu) {
    noTone(BUZZER);
    return;
  }

  unsigned long now = millis();
  if (now - enSonkiTonDegisimi > 80) {
    enSonkiTonDegisimi = now;
    if (alarmToneState == 0) {
      tone(BUZZER, 3500);
      alarmToneState = 1;
    } else {
      tone(BUZZER, 4200);
      alarmToneState = 0;
    }
  }
}

// ---------------- LCD FLASHING -----------------
void handleLCDFlash(bool ringing) {
  if (!ringing) {
    lcdVisible = true;
    return;
  }

  if (millis() - lcdFlashTimer > 300) {
    lcdFlashTimer = millis();
    lcdVisible = !lcdVisible;
    if (!lcdVisible) lcd.clear();
  }
}

// ---------------- RTC ALARM -----------------
void setRTCAlarmNext() {
  DateTime now = rtc.now();
  DateTime alarmZamani(now.year(), now.month(), now.day(), alarmSaati, alarmDakika, 0);

  if (alarmZamani.unixtime() <= now.unixtime()) {
    alarmZamani = alarmZamani + TimeSpan(1, 0, 0, 0);
  }

  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.setAlarm1(alarmZamani, DS3231_A1_Date); 
  rtc.writeSqwPinMode(DS3231_OFF);
}

void clearDS3231AlarmFlag() {
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
}