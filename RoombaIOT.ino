

#include <PubSubClient.h>
#include <WiFiNINA.h>
#include <SimpleTimer.h>
//#include <Roomba.h>





//USER CONFIGURED SECTION START//
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const int mqtt_port = 1883;
const char *mqtt_user = "";
const char *mqtt_pass = "";
const char *mqtt_client_name = ""; // Client connections can't have the same connection name
//USER CONFIGURED SECTION END//

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
SimpleTimer timer;
//Roomba roomba(&Serial1, Roomba::Baud115200);


// Variables
bool boot = true;
long battery_Current_mAh = 0;
long battery_Voltage = 0;
long battery_Total_mAh = 0;
long battery_percent = 0;
char battery_percent_send[50];
char battery_Current_mAh_send[50];
uint8_t tempBuf[10];
static unsigned long currentMillis = 0;





void reconnect()
{
  // Loop until we're reconnected

  while (!mqttClient.connected())
  {
    if (mqttClient.connect(mqtt_client_name, mqtt_user, mqtt_pass, "roomba/status", 0, 0, "Dead Somewhere"))
    {
      // Once connected, publish an announcement...
      if (boot == false)
      {
        mqttClient.publish("checkIn/roomba", "Reconnected");
      }
      if (boot == true)
      {
        mqttClient.publish("checkIn/roomba", "Rebooted");
        boot = false;
      }
      // ... and resubscribe
      mqttClient.subscribe("roomba/commands");
    }
    else
    {
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length)
{
  String newTopic = topic;
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  if (newTopic == "roomba/commands")
  {
    if (newPayload == "start")
    {
      startCleaning();
    }
    if (newPayload == "stop")
    {
      stopCleaning();
    }
  }
}


void startCleaning()
{
  Serial1.write(128);
  delay(50);
  Serial1.write(131);
  delay(50);
  Serial1.write(135);

  mqttClient.publish("roomba/status", "Cleaning");
}

void stopCleaning()
{
  Serial1.write(128);
  delay(50);
  Serial1.write(131);
  delay(50);
  Serial1.write(143);

  mqttClient.publish("roomba/status", "Returning");

}


bool getData(uint8_t* dest, uint8_t len)
{
  while (len-- > 0)
  {
    unsigned long startTime = millis();
    while (!Serial1.available())
    {
      // Look for a timeout
      if (millis() > (startTime + 200))
        return false; // Timed out
    }
    *dest++ = Serial1.read();
  }
  return true;
}

bool get_sensors(uint8_t packetID, uint8_t* dest, uint8_t len)
{
  Serial1.write(142);
  Serial1.write(packetID);
  return getData(dest, len);
}


void sendInfoRoomba()
{
  Serial1.write(128);//roomba.start
  //delay(50);
  get_sensors(21, tempBuf, 1);
  battery_Voltage = tempBuf[0];
  delay(50);
  get_sensors(25, tempBuf, 2);
  battery_Current_mAh = tempBuf[1] + 256 * tempBuf[0];
  delay(50);
  get_sensors(26, tempBuf, 2);
  battery_Total_mAh = tempBuf[1] + 256 * tempBuf[0];
  if (battery_Total_mAh != 0)
  {
    int nBatPcent = 100 * battery_Current_mAh / battery_Total_mAh;
    String temp_str2 = String(nBatPcent);
    temp_str2.toCharArray(battery_percent_send, temp_str2.length() + 1); //packaging up the data to publish to mqtt

    mqttClient.publish("roomba/battery", battery_percent_send);

  }
  if (battery_Total_mAh == 0)
  {
    mqttClient.publish("roomba/battery", "NO DATA");
  }
  String temp_str = String(battery_Voltage);
  temp_str.toCharArray(battery_Current_mAh_send, temp_str.length() + 1); //packaging up the data to publish to mqtt

  mqttClient.publish("roomba/charging", battery_Current_mAh_send);

  //  roomba_21();
}




void setup() {
  //Initialize Serial1 and wait for port to open:
  Serial1.begin(115200);
  delay(200);
  Serial1.write(129);
  delay(50);
  Serial1.write(11);
  delay(50);

  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    delay(5000);
  }


  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  // mqttClient.setId("clientId");

  // You can provide a username and password for authentication
  mqttClient.setCallback(callback);
  mqttClient.setServer(mqtt_server, mqtt_port);


  timer.setInterval(5000, sendInfoRoomba);

  //wdt_enable(WDTO_2S);
}

void loop()
{
  if (!wifiClient.connected())
  {
    reconnect();
  }
  mqttClient.loop();
  timer.run();

}
