#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>


// Servo Configuration
Servo gripperServo, baseServo, prismServo;
const int gripperPin = 13, basePin = 12, prismPin = 14;


// Network Configuration
const char* ssid = "ESP32-Cam";
const char* password = "12345678";


// HiveMQ Cloud MQTT Broker settings
const char* mqtt_server = "681ac02d3b284d24b82cd9ed5e167b3c.s1.eu.hivemq.cloud";
const int   mqtt_port = 8883;
const char* mqtt_user = "mnagy";
const char* mqtt_password = "19919690mN";
const char* manual_topic 	= "arm/manual";
const char* automatic_topic = "arm/automatic";

char searchFor = 'r';
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

enum ArmState { IDLE, SEARCHING, APPROACHING, GRASPING, RETURNING };
ArmState currentState = IDLE;

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void connectToMQTT() {
  espClient.setInsecure();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      mqttClient.subscribe(manual_topic);
      mqttClient.subscribe(automatic_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("[MQTT] Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (strcmp(topic, manual_topic) == 0 || strcmp(topic, automatic_topic) == 0) {
    if (message.length() > 0) {
      char command = message[0];
      handleMQTTCommand(command);
    }
  }
}

void handleMQTTCommand(char command) {
  Serial.print("[handleMQTTCommand] Received command: ");
  Serial.println(command);
  Serial.print("[handleMQTTCommand] Current state before command: ");
  Serial.println(currentState);
  Serial.print("[handleMQTTCommand] currentPosition before command: ");
  Serial.println(currentPosition);

  switch(command) {
    case 'r':
      Serial.println("[handleMQTTCommand] Searching for red object");
      searchFor = 'r';
      currentState = SEARCHING;
      startSearchPattern();
      break;
    case 'b':
      Serial.println("[handleMQTTCommand] Searching for blue object");
      searchFor = 'b';
      currentState = SEARCHING;
      startSearchPattern();
      break;
    case 'g':
      Serial.println("[handleMQTTCommand] Searching for green object");
      searchFor = 'g';
      currentState = SEARCHING;
      startSearchPattern();
      break;
    case 'c':
      Serial.println("[handleMQTTCommand] Grasping command received");
      currentState = GRASPING;
      graspObject();
      break;
    // GUI Controls
    case 'x':  controlBaseAngleLeft();  break;
    case 'y':  controlBaseAngleRight(); break;
    case 'a':  controlPrismAngleUp();   break;
    case 'd':  controlPrismAngleDown(); break;
    case 'm':  controlGripperOpen();    break;
    case 'n':  controlGripperClose();   break;
    default:
      Serial.print("[handleMQTTCommand] Unknown command: ");
      Serial.println(command);
      sendStatus("error:unknown_command");
      break;
  }

  Serial.print("[handleMQTTCommand] Current state after command: ");
  Serial.println(currentState);
  Serial.print("[handleMQTTCommand] currentPosition after command: ");
  Serial.println(currentPosition);
}

void sendStatus(const char* status) {
  if (mqttClient.connected()) {
    mqttClient.publish("arm/status", status);
  }
}


// Automated Control Functions
int currentPosition = 0;
void startSearchPattern() {
  Serial.println("[startSearchPattern] Starting search pattern");
  sendStatus("searching");
  Serial.print("[startSearchPattern] Initial currentPosition: ");
  Serial.println(currentPosition);

  for (int pos = currentPosition; pos <= 180; pos += 3) {
    Serial.print("[startSearchPattern] Sweeping right, pos: ");
    Serial.println(pos);
    baseServo.write(pos);
    delay(250);
    currentPosition = pos;
    Serial.print("[startSearchPattern] State in right sweep: ");
    Serial.println(currentState);
    if (currentState != SEARCHING){
      Serial.println("[startSearchPattern] Exiting right sweep due to state change");
      break;
    }
  }

  currentPosition = 180;

  for (int pos = currentPosition; pos >= 0; pos -= 3) {
    Serial.print("[startSearchPattern] Sweeping left, pos: ");
    Serial.println(pos);
    baseServo.write(pos);
    delay(250);
    currentPosition = pos;
    Serial.print("[startSearchPattern] State in left sweep: ");
    Serial.println(currentState);
    if (currentState != SEARCHING) {
      Serial.println("[startSearchPattern] Exiting left sweep due to state change");
      break;
    }
  }

  currentPosition = 0;
  Serial.print("[startSearchPattern] Finished search, currentPosition reset to: ");
  Serial.println(currentPosition);

  if (currentState == SEARCHING) {
    currentState = IDLE;
    sendStatus("search_complete");
    Serial.println("[startSearchPattern] Done Searching, state set to IDLE");
  }
}

void testAproach(){
    for (int pos = 180; pos >= 0; pos -= 5) {
        prismServo.write(pos);
        delay(100);
    }
    for (int pos = 0 ; pos <= 180 ; pos += 5) {
        prismServo.write(pos);
        delay(100);
    }
}

void approachObject() {
  Serial.print("[approachObject] State at start: ");
  Serial.println(currentState);
  Serial.print("[approachObject] Position at start: ");
  Serial.println(currentPosition);

  for (int pos = 90; pos >= 50; pos -= 5) {
    baseServo.write(pos);
    currentPosition = pos;
    Serial.print("[approachObject] Moving to pos: ");
    Serial.println(pos);
    delay(100);
    if (currentState != APPROACHING) {
      Serial.println("[approachObject] State changed, breaking loop");
      break;
    }
  }
  
  if (currentState == APPROACHING) {
    sendStatus("centered");
  }
  Serial.print("[approachObject] State at end: ");
  Serial.println(currentState);
  Serial.print("[approachObject] Position at end: ");
  Serial.println(currentPosition);
}

void deApproachObject() {
  Serial.println("Deapproaching object");
  
  for (int pos = 50; pos <= 90; pos += 5) {
	baseServo.write(pos);
	delay(100);
	if (currentState != APPROACHING) break;
  }
  
  if (currentState == APPROACHING) {
	currentState = IDLE;
	sendStatus("deapproached");
  }
}

void graspObject() {
  Serial.println("Grasping object");
  sendStatus("grasping");
  approachObject();
  closeGripper();
  deApproachObject();

  switch (searchFor) {
	case 'r':
	  baseServo.write(45);
	  break;
	case 'b':
	  baseServo.write(90);
	  break;
	case 'g':
	  baseServo.write(135);
	  break;
	default:
	  baseServo.write(0); 
  }
  openGripper();

  currentState = SEARCHING;
  startSearchPattern();
}

void closeGripper() {
	  Serial.println("Closing gripper");
  for (int pos = 100; pos <= 180; pos++) {
    gripperServo.write(pos);
    delay(15);
  }
}

void openGripper() {
	Serial.println("Opening gripper");
	approachObject();
	for (int pos = 180; pos >= 100; pos--) {
		gripperServo.write(pos);
		delay(15);
	}
	deApproachObject();
	currentState = IDLE;
}

void returnHome(){
  gripperServo.write(100); // Open position
  baseServo.write(0);     // Center position
  prismServo.write(180);    // Mid position
}


// GUI Control Functions
void controlBaseAngleLeft(){
    Serial.println("Turning base left");
    Serial.print("[controlBaseAngleLeft] Current position before turn: ");
    Serial.println(currentPosition);
    currentPosition += 5;
    if (currentPosition > 180) currentPosition = 180;
    baseServo.write(currentPosition);
    Serial.print("[controlBaseAngleLeft] Current position after turn: ");
    Serial.println(currentPosition);
}

void controlBaseAngleRight(){
    Serial.println("Turning base right");
    Serial.print("[controlBaseAngleRight] Current position before turn: ");
    Serial.println(currentPosition);
    currentPosition -= 5;
    if (currentPosition < 0) currentPosition = 0;
    baseServo.write(currentPosition);
    Serial.print("[controlBaseAngleRight] Current position after turn: ");
    Serial.println(currentPosition);
}

void controlPrismAngleUp(){
	Serial.println("Moving prism up");
	prismServo.write(90);
}

void controlPrismAngleDown(){
	Serial.println("Moving prism down");
	prismServo.write(50);
}

void controlGripperOpen(){
	Serial.println("Opening gripper");
	gripperServo.write(100);
}

void controlGripperClose(){
	Serial.println("Closing gripper");
	gripperServo.write(180);
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  
  // Initialize servos
  gripperServo.attach(gripperPin);
  baseServo.attach(basePin);
  prismServo.attach(prismPin);
  
  // Set initial positions
  gripperServo.write(100); 	// Open position=100 , close position=180
  baseServo.write(0);     	// Center position
  prismServo.write(180);    // Mid position
  
  connectToMQTT();
}

void loop() {
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();
}
