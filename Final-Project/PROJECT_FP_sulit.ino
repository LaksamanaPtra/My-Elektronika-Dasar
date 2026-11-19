#include <I2C_RTC.h>
#include <RTClib.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Bonezegei_DS3231.h>
#include <SPI.h>
#include <SD.h>

// =====================================================
// LCD
// =====================================================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// =====================================================
// RTC
// =====================================================
Bonezegei_DS3231 rtc(0x68);

// =====================================================
// WIND VANE
// =====================================================
SoftwareSerial dataserial(7, 6); // RX, TX

String data;
String s_angin;
String arah_angin = "Unknown";

// =====================================================
// ANEMOMETER
// =====================================================
volatile byte rpmcount = 0;
volatile unsigned long last_micros = 0;

const byte GPIO_pulse = 2;

float rotasi_per_detik = 0;
float kecepatan_meter_per_detik = 0;
float kecepatan_kilometer_per_jam = 0;

// =====================================================
// DUST SENSOR
// =====================================================
const int measurePin = A0;
const int ledPower = 3;

const int samplingTime = 280;
const int deltaTime = 40;
const int sleepTime = 9680;

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

// =====================================================
// SD CARD
// =====================================================
const int chipSelect = 10;

// =====================================================
// TIMER
// =====================================================
unsigned long lastDisplay = 0;
unsigned long lastLog = 0;

const unsigned long displayInterval = 1000;
const unsigned long logInterval = 10000;

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(9600);

  dataserial.begin(9600);

  pinMode(GPIO_pulse, INPUT_PULLUP);

  pinMode(ledPower, OUTPUT);
  digitalWrite(ledPower, HIGH);

  attachInterrupt(
    digitalPinToInterrupt(GPIO_pulse),
    rpm_anemometer,
    RISING
  );

  // LCD
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Weather Station");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // RTC
  rtc.begin();

  // SD CARD
  pinMode(10, OUTPUT);

  if (SD.begin(chipSelect))
  {
    lcd.setCursor(0, 2);
    lcd.print("SD Card : OK");
  }
  else
  {
    lcd.setCursor(0, 2);
    lcd.print("SD Card : ERROR");
  }

  delay(2000);
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  readWindDirection();

  unsigned long now = millis();

  // Update LCD setiap 1 detik
  if (now - lastDisplay >= displayInterval)
  {
    updateLCD();
    lastDisplay = now;
  }

  // Logging setiap 10 detik
  if (now - lastLog >= logInterval)
  {
    calculateWindSpeed();
    readDustSensor();

    saveToSD();

    printSerial();

    lastLog = now;
  }
}

// =====================================================
// WIND VANE
// =====================================================
void readWindDirection()
{
  if (dataserial.available())
  {
    data = dataserial.readString();

    int a = data.indexOf("*");
    int b = data.indexOf("#");

    if (a >= 0 && b > a)
    {
      s_angin = data.substring(a + 1, b);

      if (s_angin == "1") arah_angin = "Utara";
      else if (s_angin == "2") arah_angin = "Timur Laut";
      else if (s_angin == "3") arah_angin = "Timur";
      else if (s_angin == "4") arah_angin = "Tenggara";
      else if (s_angin == "5") arah_angin = "Selatan";
      else if (s_angin == "6") arah_angin = "Barat Daya";
      else if (s_angin == "7") arah_angin = "Barat";
      else if (s_angin == "8") arah_angin = "Barat Laut";
    }
  }
}

// =====================================================
// ANEMOMETER
// =====================================================
void calculateWindSpeed()
{
  noInterrupts();

  byte pulseCount = rpmcount;
  rpmcount = 0;

  interrupts();

  rotasi_per_detik = pulseCount / 10.0;

  kecepatan_meter_per_detik =
    (-0.0181 * rotasi_per_detik * rotasi_per_detik) +
    (1.3859 * rotasi_per_detik) +
    1.4055;

  if (kecepatan_meter_per_detik <= 1.5)
  {
    kecepatan_meter_per_detik = 0;
  }

  kecepatan_kilometer_per_jam =
    kecepatan_meter_per_detik * 3.6;
}

void rpm_anemometer()
{
  if ((long)(micros() - last_micros) >= 5000)
  {
    rpmcount++;
    last_micros = micros();
  }
}

// =====================================================
// DUST SENSOR
// =====================================================
void readDustSensor()
{
  digitalWrite(ledPower, LOW);

  delayMicroseconds(samplingTime);

  voMeasured = analogRead(measurePin);

  delayMicroseconds(deltaTime);

  digitalWrite(ledPower, HIGH);

  delayMicroseconds(sleepTime);

  calcVoltage = voMeasured * (5.0 / 1024.0);

  dustDensity = 170.0 * calcVoltage - 0.1;

  if (dustDensity < 0)
  {
    dustDensity = 0;
  }
}

// =====================================================
// LCD
// =====================================================
void updateLCD()
{
  rtc.getTime();

  lcd.clear();

  String tanggal =
    twoDigit(rtc.getDate()) + "/" +
    twoDigit(rtc.getMonth()) + "/20" +
    String(rtc.getYear());

  String waktu =
    twoDigit(rtc.getHour()) + ":" +
    twoDigit(rtc.getMinute()) + ":" +
    twoDigit(rtc.getSeconds());

  lcd.setCursor(0, 0);
  lcd.print(tanggal + "" + waktu);

  lcd.setCursor(0, 1);
  lcd.print("Arah : ");
  lcd.print(arah_angin);

  lcd.setCursor(0, 2);
  lcd.print("Angin:");
  lcd.print(kecepatan_meter_per_detik, 2);
  lcd.print(" m/s");

  lcd.setCursor(0, 3);
  lcd.print("Debu :");
  lcd.print(dustDensity, 0);
  lcd.print(" ug/m3");
}

// =====================================================
// SD CARD
// =====================================================
void saveToSD()
{
  rtc.getTime();

  String fileName =
      "20" +
      String(rtc.getYear()) +
      twoDigit(rtc.getMonth()) +
      twoDigit(rtc.getDate()) +
      ".csv";

  bool fileExists = SD.exists(fileName);

  File dataFile = SD.open(fileName, FILE_WRITE);

  if (!dataFile)
  {
    Serial.println("Gagal membuka file SD!");
    return;
  }

  if (!fileExists)
  {
    dataFile.println(
      "Tanggal,Waktu,Arah_Angin,Kecepatan_ms,Kecepatan_kmh,Dust_ugm3"
    );
  }

  dataFile.print(twoDigit(rtc.getDate()));
  dataFile.print("/");

  dataFile.print(twoDigit(rtc.getMonth()));
  dataFile.print("/20");

  dataFile.print(rtc.getYear());

  dataFile.print(",");

  dataFile.print(twoDigit(rtc.getHour()));
  dataFile.print(":");

  dataFile.print(twoDigit(rtc.getMinute()));
  dataFile.print(":");

  dataFile.print(twoDigit(rtc.getSeconds()));

  dataFile.print(",");

  dataFile.print(arah_angin);

  dataFile.print(",");

  dataFile.print(kecepatan_meter_per_detik, 2);

  dataFile.print(",");

  dataFile.print(kecepatan_kilometer_per_jam, 2);

  dataFile.print(",");

  dataFile.println(dustDensity, 0);

  dataFile.close();

  Serial.println("Data tersimpan ke SD");
}

// =====================================================
// SERIAL MONITOR
// =====================================================
void printSerial()
{
  rtc.getTime();

  Serial.println();
  Serial.println("===================================");

  Serial.print("Tanggal : ");
  Serial.print(twoDigit(rtc.getDate()));
  Serial.print("/");
  Serial.print(twoDigit(rtc.getMonth()));
  Serial.print("/20");
  Serial.println(rtc.getYear());

  Serial.print("Waktu   : ");
  Serial.print(twoDigit(rtc.getHour()));
  Serial.print(":");
  Serial.print(twoDigit(rtc.getMinute()));
  Serial.print(":");
  Serial.println(twoDigit(rtc.getSeconds()));

  Serial.print("Arah    : ");
  Serial.println(arah_angin);

  Serial.print("Angin   : ");
  Serial.print(kecepatan_meter_per_detik, 2);
  Serial.println(" m/s");

  Serial.print("Dust    : ");
  Serial.print(dustDensity, 0);
  Serial.println(" ug/m3");

  Serial.println("===================================");
}

// =====================================================
// FORMAT 2 DIGIT
// =====================================================
String twoDigit(int angka)
{
  if (angka < 10)
    return "0" + String(angka);

  return String(angka);
}
