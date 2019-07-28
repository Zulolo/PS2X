#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <PS2X_lib.h>  //for v1.6
#include <Adafruit_PWMServoDriver.h>

/******************************************************************
 * set pins connected to PS2 controller:
 *   - 1e column: original 
 *   - 2e colmun: Stef?
 * replace pin numbers by the ones you use
 ******************************************************************/
#define PS2_DAT        27    
#define PS2_CMD        26  //15
#define PS2_SEL        25  //16
#define PS2_CLK        33  //17

#define MIN_PULSE_WIDTH       650
#define MAX_PULSE_WIDTH       2350
#define DEFAULT_PULSE_WIDTH   1500
#define FREQUENCY             50

/******************************************************************
 * select modes of PS2 controller:
 *   - pressures = analog reading of push-butttons 
 *   - rumble    = motor rumbling
 * uncomment 1 of the lines for each mode selection
 ******************************************************************/
//#define pressures   true
#define pressures   false
//#define rumble      true
#define rumble      false

PS2X ps2x; // create PS2 Controller Class
// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

const char *ssid = "zulolo";
const char *password = "leon0873";
WebServer server(80);

TaskHandle_t ps2x_task;
TaskHandle_t webserver_task;

int servo_angles[6];

int pulseWidth(int angle)
{
  int pulse_wide, analog_value;
  pulse_wide   = map(angle, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  analog_value = int(float(pulse_wide) / 1000000 * FREQUENCY * 4096);
  Serial.println(analog_value);
  return analog_value;
}

//right now, the library does NOT support hot pluggable controllers, meaning 
//you must always either restart your Arduino after you connect the controller, 
//or call config_gamepad(pins) again after connecting the controller.

int error = 0;
byte type = 0;
byte vibrate = 0;

void setup(){
 
  Serial.begin(115200);
  
  pwm.begin();
  pwm.setPWMFreq(FREQUENCY);  // Analog servos run at ~60 Hz updates
   
  xTaskCreatePinnedToCore(webserver_task_code, "webserver", 10000, NULL, 1, &webserver_task, 1);          /* pin task to core 1 */    
  delay(300); //added delay to give wireless ps2 module some time to startup, before configuring it
  xTaskCreatePinnedToCore(ps2x_task_code, "ps2x", 10000, NULL, 1, &ps2x_task, 0);          /* pin task to core 0 */   
}

void ps2x_task_code( void * pvParameters ){
  int tmp_angles[6] = {0};
  Serial.print("ps2x task running on core ");
  Serial.println(xPortGetCoreID());

  servo_angles[0] = 45;
  servo_angles[1] = 90;
  servo_angles[2] = 140;
  servo_angles[3] = 120;
  servo_angles[4] = 30;
  servo_angles[5] = 90;
  
  for (int i = 0; i < 6; i++) {
    tmp_angles[i] = servo_angles[i];
    pwm.setPWM(i, 0, pulseWidth(tmp_angles[i]));
  }
  
  //setup pins and settings: GamePad(clock, command, attention, data, Pressures?, Rumble?) check for error
  delay(300);
  error = ps2x.config_gamepad(PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, pressures, rumble);
  
  if(error == 0){
    Serial.println("Found Controller, configured successful");
    type = ps2x.readType(); 
    switch(type) {
      case 0:
        Serial.print("Unknown Controller type found ");
        break;
      case 1:
        Serial.print("DualShock Controller found ");
        break;
      case 2:
        Serial.print("GuitarHero Controller found ");
        break;
      case 3:
        Serial.print("Wireless Sony DualShock Controller found ");
        break;
    }
  } else if(error == 1) {
    Serial.println("No controller found, check wiring, see readme.txt to enable debug. visit www.billporter.info for troubleshooting tips");
  } else if(error == 2) {
    Serial.println("Controller found but not accepting commands. see readme.txt to enable debug. Visit www.billporter.info for troubleshooting tips");
  } else if(error == 3) {
    Serial.println("Controller refusing to enter Pressures mode, may not support it. ");
  }  

  for(;;){
    for (int i = 0; i < 6; i++) {
      if (servo_angles[i] != tmp_angles[i]) {
        tmp_angles[i] = servo_angles[i];
        pwm.setPWM(i, 0, pulseWidth(tmp_angles[i]));
      }
    }
    if((error == 0) && ((type == 1) || (type == 3))) {
      ps2x.read_gamepad(false, vibrate); //read controller and set large motor to spin at 'vibrate' speed
      if(ps2x.ButtonReleased(PSB_SQUARE)) {
        servo_angles[0] = 45;
        servo_angles[1] = 90;
        servo_angles[2] = 140;
        servo_angles[3] = 120;
        servo_angles[4] = 30;
        servo_angles[5] = 90;        
      } 
  
      if(ps2x.Button(PSB_PAD_UP)) {      //will be TRUE as long as button is pressed
        if (servo_angles[1] < 180) {
          servo_angles[1]++;
        }
      }
      if(ps2x.Button(PSB_PAD_DOWN)){
        if (servo_angles[1] > 0) {
          servo_angles[1]--;
        }
      }       
      if(ps2x.Button(PSB_PAD_RIGHT)){
        if (servo_angles[5] < 180) {
          servo_angles[5]++;
        }
      }
      if(ps2x.Button(PSB_PAD_LEFT)){
        if (servo_angles[5] > 0) {
          servo_angles[5]--;
        }
      }
      if(ps2x.Button(PSB_L2)){
        if (servo_angles[0] > 0) {
          servo_angles[0]--;
        }
      }
      if(ps2x.Button(PSB_R2)){
        if (servo_angles[0] < 80) {
          servo_angles[0]++;
        }
      }  
  
      vibrate = ps2x.Analog(PSAB_CROSS);  //this will set the large motor vibrate speed based on how hard you press the blue (X) button
      if (ps2x.NewButtonState()) {        //will be TRUE if any button changes state (on to off, or off to on)
        if(ps2x.Button(PSB_L3))
          Serial.println("L3 pressed");
        if(ps2x.Button(PSB_R3))
          Serial.println("R3 pressed");
        if(ps2x.Button(PSB_L2))
          Serial.println("L2 pressed");
        if(ps2x.Button(PSB_R2))
          Serial.println("R2 pressed");
        if(ps2x.Button(PSB_TRIANGLE))
          Serial.println("Triangle pressed");        
      }
  
      if(ps2x.ButtonPressed(PSB_CIRCLE))               //will be TRUE if button was JUST pressed
        Serial.println("Circle just pressed");
      if(ps2x.NewButtonState(PSB_CROSS))               //will be TRUE if button was JUST pressed OR released
        Serial.println("X just changed");
      if(ps2x.ButtonReleased(PSB_SQUARE))              //will be TRUE if button was JUST released
        Serial.println("Square just released");     
  
      if(ps2x.Button(PSB_L1) || ps2x.Button(PSB_R1)) { //print stick values if either is TRUE
        Serial.print("Stick Values:");
        Serial.print(ps2x.Analog(PSS_LY), DEC); //Left stick, Y axis. Other options: LX, RY, RX  
        Serial.print(",");
        Serial.print(ps2x.Analog(PSS_LX), DEC); 
        Serial.print(",");
        Serial.print(ps2x.Analog(PSS_RY), DEC); 
        Serial.print(",");
        Serial.println(ps2x.Analog(PSS_RX), DEC); 
      }        
    }
    delay(50);  
  } 
}

void webserver_task_code( void * pvParameters ){
  Serial.print("webserver running on core ");
  Serial.println(xPortGetCoreID());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("gamepad")) {
    Serial.println("MDNS responder started");
  }

  server.on("/set_servo", handle_set_servo);
  server.onNotFound(handle_not_found);
  server.begin();
  Serial.println("HTTP server started");
  for(;;){
    server.handleClient();
  }
}

void handle_set_servo() {
  int servo;
  if (server.method() == HTTP_GET) { // normally should be POST to set (RESTful), but just for test
    for (uint8_t i = 0; i < server.args(); i++) {
      servo = server.argName(i).toInt();
      if (servo > 0 && servo < 7) {
        servo_angles[servo - 1] = server.arg(i).toInt();
      }
    }
    server.send(200, "text/plain", "servo change");
  } else {
    handle_not_found();
  }
}

void handle_not_found() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void loop() { 

}
