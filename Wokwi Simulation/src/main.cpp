#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET 0
#define UTC_OFFSET_DST 0

#define BUZZER 5
#define LED_1 15
#define LED_2 2
#define CANCEL 34
#define UP 33
#define OK 32
#define DOWN 35
#define DHT 12
#define LDR_PIN 39 // VN pin on ESP32
#define SERVO_PIN 18

#define SNOOZE_DURATION 5 // Snooze duration in minutes
#define MQTT_SERVER "broker.hivemq.com"
#define MQTT_PORT 1883

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dht_sensor;
WiFiClient espClient;
PubSubClient client(espClient);
Servo servo;

// Function declarations
void print_line(String message, int text_size, int row, int column);
void print_time_now();
void update_time_over_wifi();
void update_time_with_check_alarm();
void ring_alarm(int alarm_num);
int wait_for_button_press();
void go_to_menu();
void set_time_zone();
void set_alarms();
void view_alarms();
void delete_alarm();
void run_mode(int mode);
void check_environment();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
void reconnect();
float read_ldr();
void update_servo_angle();

// Global variables
int num_notes = 8;
int notes[] = {220, 294, 330, 349, 494, 440, 450, 523};

int days = 0, month = 0, year = 0, hours = 0, minutes = 0, seconds = 0;
bool alarm_enabled = true;
int num_alarms = 2;
int alarm_hours[] = {0, 0};
int alarm_minutes[] = {0, 0};
bool alarm_triggered[] = {false, false};
bool alarm_active[] = {false, false};
int current_mode = 0;
int max_modes = 4;
String options[] = {"Set Time Zone", "Set Alarms", "View Alarms", "Delete Alarm"};
int utc_offset = 19800;
unsigned long snooze_time = 0;

// Light and servo control variables
float intensity_sum = 0;
int intensity_count = 0;
float last_average_intensity = 0;
unsigned long last_sample_time = 0;
unsigned long last_send_time = 0;
int sampling_interval = 5;  // Default 5 seconds
int sending_interval = 120; // Default 2 minutes (120 seconds)
float theta_offset = 30;    // Default minimum angle (degrees)
float gamm = 0.75;          // Default controlling factor
float Tmed = 30;            // Default ideal storage temperature (°C)

void setup()
{
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(CANCEL, INPUT);
  pinMode(UP, INPUT);
  pinMode(OK, INPUT);
  pinMode(DOWN, INPUT);
  pinMode(LDR_PIN, INPUT);

  dht_sensor.setup(DHT, DHTesp::DHT22);
  servo.attach(SERVO_PIN, 500, 2400); // Attach servo with min/max pulse widths (500µs, 2400µs)
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;
  }

  display.display();
  delay(2000);
  display.clearDisplay();
  print_line("MediBox Ready", 0, 0, 2);
  delay(1000);

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    display.clearDisplay();
    print_line("Connecting...", 2, 0, 0);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqtt_callback);
  Serial.println("Attempting to connect to MQTT broker...");
  reconnect(); // Initial MQTT connection attempt
}

void loop()
{
  if (!client.connected())
  {
    Serial.println("MQTT disconnected, attempting to reconnect...");
    reconnect();
  }
  client.loop();

  update_time_with_check_alarm();

  if (digitalRead(OK) == LOW)
  {
    delay(200);
    go_to_menu();
  }

  check_environment();

  // Sample LDR at sampling_interval
  unsigned long current_time = millis();
  if (current_time - last_sample_time >= sampling_interval * 1000)
  {
    float intensity = read_ldr();
    intensity_sum += intensity;
    intensity_count++;
    last_sample_time = current_time;
  }

  // Send average intensity at sending_interval
  if (current_time - last_send_time >= sending_interval * 1000)
  {
    if (intensity_count > 0)
    {
      last_average_intensity = intensity_sum / intensity_count;
      char buffer[10];
      dtostrf(last_average_intensity, 4, 2, buffer);
      client.publish("medibox_220146A/intensity", buffer);
      intensity_sum = 0;
      intensity_count = 0;
    }
    last_send_time = current_time;
    update_servo_angle();
  }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  String message;
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  Serial.print("Received MQTT message on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(message);

  if (String(topic) == "medibox_220146A/Sampling")
  {
    sampling_interval = message.toInt();
    Serial.print("Updated sampling_interval to: ");
    Serial.println(sampling_interval);
  }
  else if (String(topic) == "medibox_220146A/Sending")
  {
    sending_interval = message.toInt() * 60; // Convert minutes to seconds
    Serial.print("Updated sending_interval to: ");
    Serial.println(sending_interval);
  }
  else if (String(topic) == "medibox_220146A/offset")
  {
    theta_offset = message.toFloat();
    Serial.print("Updated theta_offset to: ");
    Serial.println(theta_offset);
  }
  else if (String(topic) == "medibox_220146A/control")
  {
    gamm = message.toFloat();
    Serial.print("Updated gamm to: ");
    Serial.println(gamm);
  }
  else if (String(topic) == "medibox_220146A/temp")
  {
    Tmed = message.toFloat();
    Serial.print("Updated Tmed to: ");
    Serial.println(Tmed);
  }
}

void reconnect()
{
  while (!client.connected())
  {
    String clientId = "MediBoxClient-";
    clientId += String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT broker with Client ID: ");
    Serial.println(clientId);
    if (client.connect(clientId.c_str()))
    {
      Serial.println("Connected to MQTT broker");
      client.subscribe("medibox_220146A/Sampling");
      client.subscribe("medibox_220146A/Sending");
      client.subscribe("medibox_220146A/offset");
      client.subscribe("medibox_220146A/control");
      client.subscribe("medibox_220146A/temp");
      Serial.println("Subscribed to MQTT topics");
    }
    else
    {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.println(client.state());
      Serial.println("Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

float read_ldr()
{
  int ldr_raw = analogRead(LDR_PIN);
  // Map LDR reading (0-4095) to intensity (0-1)
  float intensity = (float)ldr_raw / 4095.0;
  return intensity;
}

void update_servo_angle()
{
  TempAndHumidity data = dht_sensor.getTempAndHumidity();
  float T = data.temperature;
  float I = last_average_intensity;

  // Calculate servo angle using the provided equation
  float theta = theta_offset + (180 - theta_offset) * I * gamm * log((float)sampling_interval / sending_interval) * (T / Tmed);
  theta = constrain(theta, 0, 180); // Ensure angle is within 0-180 degrees
  servo.write((int)theta);
}

void print_line(String message, int text_size, int row, int column)
{
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(message);
  display.display();
}

void print_time_now()
{
  display.clearDisplay();
  print_line(String(days) + "/" + String(month) + "/" + String(year), 1, 0, 0);
  print_line(String(hours) + ":" + String(minutes) + ":" + String(seconds), 2, 10, 0);
}

void update_time_over_wifi()
{
  configTime(utc_offset, UTC_OFFSET_DST, NTP_SERVER);
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char date_str[8], month_str[8], year_str[20], hour_str[8], min_str[8], sec_str[8];
  strftime(month_str, 20, "%m", &timeinfo);
  strftime(date_str, 20, "%d", &timeinfo);
  strftime(year_str, 20, "%Y", &timeinfo);
  strftime(sec_str, 8, "%S", &timeinfo);
  strftime(hour_str, 8, "%H", &timeinfo);
  strftime(min_str, 8, "%M", &timeinfo);

  days = atoi(date_str);
  month = atoi(month_str);
  year = atoi(year_str);
  hours = atoi(hour_str);
  minutes = atoi(min_str);
  seconds = atoi(sec_str);
}

void update_time_with_check_alarm()
{
  display.clearDisplay();
  update_time_over_wifi();
  print_time_now();

  if (alarm_enabled)
  {
    for (int i = 0; i < num_alarms; i++)
    {
      if (alarm_active[i] && !alarm_triggered[i] &&
          hours == alarm_hours[i] && minutes == alarm_minutes[i])
      {
        ring_alarm(i);
      }
    }
  }
}

void ring_alarm(int alarm_num)
{
  display.clearDisplay();
  print_line("Medicine Time!", 2, 0, 0);
  digitalWrite(LED_1, HIGH);

  bool alarm_stopped = false;
  while (!alarm_stopped)
  {
    for (int i = 0; i < num_notes; i++)
    {
      if (digitalRead(CANCEL) == LOW)
      {
        delay(200);
        alarm_stopped = true;
        alarm_triggered[alarm_num] = true;
        alarm_active[alarm_num] = false;
        break;
      }
      if (digitalRead(OK) == LOW)
      {
        delay(200);
        int snooze_minutes = (alarm_minutes[alarm_num] + SNOOZE_DURATION) % 60;
        int snooze_hours = (alarm_hours[alarm_num] + (alarm_minutes[alarm_num] + SNOOZE_DURATION) / 60) % 24;
        alarm_hours[alarm_num] = snooze_hours;
        alarm_minutes[alarm_num] = snooze_minutes;
        print_line("Snoozed to:", 1, 40, 0);
        print_line(String(snooze_hours) + ":" + String(snooze_minutes), 2, 50, 0);
        alarm_triggered[alarm_num] = false;
        alarm_stopped = true;
        delay(1000);
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }
  digitalWrite(LED_1, LOW);
}

int wait_for_button_press()
{
  while (true)
  {
    if (digitalRead(UP) == LOW)
      return UP;
    if (digitalRead(DOWN) == LOW)
      return DOWN;
    if (digitalRead(CANCEL) == LOW)
      return CANCEL;
    if (digitalRead(OK) == LOW)
      return OK;
    delay(10);
  }
}

void go_to_menu()
{
  while (digitalRead(CANCEL) == HIGH)
  {
    display.clearDisplay();
    print_line(options[current_mode], 2, 0, 0);

    int pressed = wait_for_button_press();
    delay(200);

    if (pressed == UP)
    {
      current_mode = (current_mode + 1) % max_modes;
    }
    else if (pressed == DOWN)
    {
      current_mode = (current_mode - 1 + max_modes) % max_modes;
    }
    else if (pressed == OK)
    {
      run_mode(current_mode);
    }
  }
}

void set_time_zone()
{
  int temp_hour = 0;
  int temp_minute = 0;
  bool setting_hour = true;

  while (true)
  {
    display.clearDisplay();
    print_line("Set UTC Offset:", 1, 0, 0);
    print_line(String(temp_hour) + "h " + String(temp_minute) + "m", 2, 20, 0);

    int pressed = wait_for_button_press();
    delay(200);

    if (setting_hour)
    {
      if (pressed == UP)
        temp_hour = (temp_hour + 1) % 24;
      if (pressed == DOWN)
        temp_hour = (temp_hour - 1 + 24) % 24;
      if (pressed == OK)
        setting_hour = false;
    }
    else
    {
      if (pressed == UP)
        temp_minute = (temp_minute + 15) % 60;
      if (pressed == DOWN)
        temp_minute = (temp_minute - 15 + 60) % 60;
      if (pressed == OK)
      {
        utc_offset = temp_hour * 3600 + temp_minute * 60;
        configTime(utc_offset, UTC_OFFSET_DST, NTP_SERVER);
        break;
      }
    }
    if (pressed == CANCEL)
      break;
  }
}

void set_alarms()
{
  for (int i = 0; i < num_alarms; i++)
  {
    bool setting_hour = true;
    while (true)
    {
      display.clearDisplay();
      print_line("Set Alarm " + String(i + 1), 1, 0, 0);
      if (setting_hour)
      {
        print_line("Hour: " + String(alarm_hours[i]), 2, 20, 0);
      }
      else
      {
        print_line("Min: " + String(alarm_minutes[i]), 2, 20, 0);
      }
      int pressed = wait_for_button_press();
      delay(200);
      if (setting_hour)
      {
        if (pressed == UP)
          alarm_hours[i] = (alarm_hours[i] + 1) % 24;
        if (pressed == DOWN)
          alarm_hours[i] = (alarm_hours[i] - 1 + 24) % 24;
        if (pressed == OK)
          setting_hour = false;
      }
      else
      {
        if (pressed == UP)
          alarm_minutes[i] = (alarm_minutes[i] + 1) % 60;
        if (pressed == DOWN)
          alarm_minutes[i] = (alarm_minutes[i] - 1 + 60) % 60;
        if (pressed == OK)
        {
          alarm_active[i] = true;
          alarm_triggered[i] = false;
          break;
        }
      }
      if (pressed == CANCEL)
        break;
    }
  }
}

void view_alarms()
{
  display.clearDisplay();
  print_line("Active Alarms:", 1, 0, 0);
  for (int i = 0; i < num_alarms; i++)
  {
    if (alarm_active[i])
    {
      print_line(String(i + 1) + ": " + String(alarm_hours[i]) + ":" + String(alarm_minutes[i]), 1, 10 + (i * 10), 0);
    }
  }
  delay(3000);
}

void delete_alarm()
{
  int selected_alarm = 0;
  while (true)
  {
    display.clearDisplay();
    print_line("Delete Alarm:", 1, 0, 0);
    print_line("Alarm " + String(selected_alarm + 1), 2, 20, 0);
    int pressed = wait_for_button_press();
    delay(200);
    if (pressed == UP)
      selected_alarm = (selected_alarm + 1) % num_alarms;
    if (pressed == DOWN)
      selected_alarm = (selected_alarm - 1 + num_alarms) % num_alarms;
    if (pressed == OK)
    {
      alarm_active[selected_alarm] = false;
      break;
    }
    if (pressed == CANCEL)
      break;
  }
}

void run_mode(int mode)
{
  switch (mode)
  {
  case 0:
    set_time_zone();
    break;
  case 1:
    set_alarms();
    break;
  case 2:
    view_alarms();
    break;
  case 3:
    delete_alarm();
    break;
  }
}

void check_environment()
{
  TempAndHumidity data = dht_sensor.getTempAndHumidity();
  bool warning = false;
  if (data.temperature < 24 || data.temperature > 32)
  {
    print_line("Temp Alert!", 1, 40, 0);
    warning = true;
  }
  if (data.humidity < 65 || data.humidity > 80)
  {
    print_line("Humidity Alert!", 1, 50, 0);
    warning = true;
  }
  digitalWrite(LED_2, warning ? HIGH : LOW);
}