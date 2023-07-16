#include <Arduino_JSON.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <MFRC522.h>

#define SS_PIN 53
#define RST_PIN 5




// Declare Files
File Deliveries;
File Orders;
File returnTables;


const byte numRows= 4;          //number of rows on the keypad
const byte numCols= 4;          //number of columns on the keypad
char keymap[numRows][numCols]= 
{
{'1', '2', '3', 'A'}, 
{'4', '5', '6', 'B'}, 
{'7', '8', '9', 'C'},
{'*', '0', '#', 'D'}
};

const int tableRows = 4;
const int tableCols = 4;
const String Table[tableRows][tableCols] = {
                  {"0", "1", "2", "3"},
                  {"4", "5", "6", "7"},
                  {"8", "9" , "10", "11"},
                  {"12", "13", "14", "15"}
                };
String currentTable = "";
int currentX, currentY = 0; // Current Position
int tableY, tableX = 0; // Destination
char nextDirection = 'R';
int currentChamber = 0;

int dx, dy = 0;
// int dx = tableX - currentX;
// int dy = tableY - currentY;
char keyPressed;
const char ENTER_KEY = 'D';
const char BACK_KEY = '*';
const char EXIT_KEY = '=';
int i;
int j;
int k;
int waitingQueue = 0;

#define EN1 2
#define IN1 3
#define IN2 4
#define IN3 5
#define IN4 6
#define EN2 7
int speed = 1023;
int t_speed = 1023;

#define pin1A 10
#define pin1B 11
#define pin2A 12
#define pin2B 13


// Declare Menu;
const char mainMenu[] = "[\"Enter Admin Mode\", \"Enter Delivery Mode\"]";
const char deliveryMenu[] = "[\"Add Delivery\", \"View Delivery\", \"Clear Deliveries\"]";
const char ordersMenu[] = "[\"View Orders\", \"Clear Orders\"]";
const char menu_[] = "{\"main\": 0, \"delivery\": 0, \"orders\": 0}";
JSONVar menu = JSON.parse(menu_);

byte rowPins[numRows] = {A0,A1,A2,A3}; 
byte colPins[numCols]= {A4,A5,3,2}; 


// Declare Classes
MFRC522 rfid(SS_PIN, RST_PIN);  // Instance of the class
LiquidCrystal_I2C lcd(0x27, 16, 4); // I2C address 0x27, 16 column and 2 rows
Keypad myKeypad= Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);
void setup() {

  Serial.begin(9600);
  SPI.begin();      // Init SPI bus
  rfid.PCD_Init();  // Init MFRC522

  // Declare motor pins as outputs
  pinMode(EN1, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(EN2, OUTPUT);
  pinMode(13, OUTPUT);


  // Make Initializations
  parseMenu();
  initializeSD();
  displayMenu("main");
  displayMenu("delivery");
  displayMenu("orders");
  
}

void loop() {
  // put your main code here, to run repeatedly:

}

// Initialization function
void initializeSD(){
  if (!SD.begin(4)) {
    Serial.println("SD Card initialization failed!");
    while (1);
  }
}
void parseMenu(){
  JSONVar m = JSON.parse(mainMenu);
  menu["main"] = m;
  m = JSON.parse(deliveryMenu);
  menu["delivery"] = m;
  m = JSON.parse(ordersMenu);
  menu["orders"] = m;
}

//Motors 
//Motors
void _turn(int angle, char direction = '-') {

  int RATE = 1500;
  float RATIO = angle / 360;
  int DURATION = RATIO * RATE;
  // Serial.println(DURATION);

  if (direction == "R") {
    analogWrite(EN2, t_speed);

    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  }

  if (direction == "L") {
    analogWrite(EN1, t_speed);

    digitalWrite(IN4, HIGH);
    digitalWrite(IN3, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN1, LOW);
  }
  delay(DURATION);
  return;
}

void _move(String direction) {
  analogWrite(EN1, speed);
  analogWrite(EN2, speed);
  if (direction == "F") {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN4, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
  }
  if (direction == "B") {
    digitalWrite(IN1, LOW);
    digitalWrite(IN4, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH);
  }
  return;
}

void _stop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN4, LOW);
  return;
}

// Chambers Operation
void stopChamber(){
  digitalWrite(pin1A, LOW);
  digitalWrite(pin1B, LOW);
  digitalWrite(pin2A, LOW);
  digitalWrite(pin2B, LOW);
}

void openChamber(int chamber){
  stopChamber();
  if(chamber == 1){
    // Forward
    digitalWrite(pin1A, HIGH);
  }
  if(chamber == 2){
    //Forward
    digitalWrite(pin2A, HIGH);
  }
}

void closeChamber(int chamber){
  stopChamber();
  if(chamber == 1){
    // Backward
    digitalWrite(pin1B, HIGH);
  }
  if(chamber == 2){
    // Backward
    digitalWrite(pin2B, HIGH);
  }
}

// RFID
boolean verify_card() {

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent()) {
    // Serial.println("Place Card...");
    // delay(200);
    return false;
  }

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
    return false;




  // Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI && piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return false;
  }

  // Try to verify the card

  if (rfid.uid.uidByte[0] == 0xB3 && rfid.uid.uidByte[1] == 0xCC && rfid.uid.uidByte[2] == 0x0C && rfid.uid.uidByte[3] == 0xAE) {
    Serial.println("Access granted!");
    return true;
  } else {
    Serial.println("Unauthorized!");
    return false;
  }
  
  // return;
}

void displayMenu(String type){
  Serial.println(type);
  for (int i = 0; i < menu[type].length(); i++) {
    int index = i + 1;
    String s = (String) index;
    String n = menu[type][i];
    Serial.println(s + " - " + n);
  }
  
}

void awaitKeypad(){
    i=0;                      //All variables set to 0
    j=0;
    k=0; 
 char code[]= {'1','2','3','4'};  //The default code, you can change it or make                                    
    while(keyPressed != ENTER_KEY){ 
          keyPressed = myKeypad.getKey();                         
            if(keyPressed != NO_KEY && keyPressed != ENTER_KEY ){       //If the char typed isn't A and neither "nothing"
            // lcd.setCursor(j,1);                                  //This to 
            // lcd.print("*");
            k++;
          if(keyPressed == code[i] && i < sizeof(code)){            //if the char typed is correct a and i increments to verify the next caracter
                j++;                                              
                i++;
                }
          else
              j--;                                               //if the character typed is wrong a decrements and cannot equal the size of code []
          }
          }
  keyPressed = NO_KEY;
}

void home(){

}


void listenForKey(String message = "Press a key"){
  while (keyPressed == NO_KEY) {
    keyPressed = myKeypad.getKey();
  }
  if(keyPressed == EXIT_KEY){
      home();
  }
}


void deliveryMode(){
  listenForKey();
   while(keyPressed != NO_KEY){
    switch (keyPressed) {
      case '1':
      keyPressed = NO_KEY;
      //addDelivery();
      break;
        
      case '2':
      keyPressed = NO_KEY;
        //viewDelivery();
      break;
      
      case '3':
      keyPressed = NO_KEY;
        //clearDelivery()
      break;
    }
  }
}

void ordersMode(){
   listenForKey();
 while(keyPressed != NO_KEY){
    switch (keyPressed) {
    case '1':
    keyPressed = NO_KEY;
     viewOrders();
    break;
      
    case '2':
    keyPressed = NO_KEY;
      clearOrders();
    break;
    }
    
   // Print "Wrong input"
  }
}


void adminMode(){
  listenForKey();
   while(keyPressed != NO_KEY){
    switch (keyPressed) {
    case '1':
    keyPressed = NO_KEY;
     deliveryMode();
    break;
      
    case '2':
    keyPressed = NO_KEY;
      ordersMode();
    break;
    
    case '3':
    keyPressed = NO_KEY;
      // resetMode();
    break;
    }
  // Print "Wrong input"
  }
}

// DELIVERIES
/*
void addDelivery(){
  char d[2];
  int q = 0;
  SD.remove("deliveries");
  Deliveries = SD.open("deliveries.txt", FILE_WRITE);

  while(q < 2){
    // Print "Enter Table no for chamber q"
    listenForKey();
    if(keyPressed == BACK_KEY) q--;
    if(q < 0) adminMode();
    d[q] = keyPressed;
    q++;
  }

  JSONVar deliveries = JSON.parse(d);
  String s = JSON.stringify(deliveries);
  Deliveries.println(s);
  Deliveries.close();
}
*/


// DELIVERIES
void addDelivery(){
 // Print "Please write the tablesto deliver"
 int finished = 0;
 char key[3];
 int i = 0;
 const char z[] = "[]";
  JSONVar data = JSON.parse(z);
 while (finished < 2){
   listenForKey();
   if(keyPressed == ENTER_KEY){
     if(i == 0){
       //Print "Please input a value"
     }
     else {
       // Write to the deliveries
       String k = String(i);
       data[finished] = k;
       finished++;
     }
   }
   else {
     if(i < 3){
        key[i] = keyPressed;
        // Shift the display by i value
        i++;
     }
     
   }

   if(keyPressed == BACK_KEY){
     if(finished > 0){
       finished--;
     }
     else {
       finished = 0;
     }
   }
 }
  SD.remove("deliveries.txt");
  Deliveries = SD.open("deliveries.txt", FILE_WRITE);
  if(Deliveries){
    Deliveries.println(JSON.stringify(data));
    Deliveries.close();
    return;
  }
  // Print "Unsble to add Deliveries"
}


void viewDelivery(){
  Deliveries = SD.open("deliveries.txt");
  if(Deliveries){
    JSONVar deliveries = JSON.parse(Deliveries.readString());
     for (int i = 0; i < deliveries.length(); i++) {
      int index = i + 1;
      String s = (String) index;
      String n = deliveries[i];
      Serial.println(s + " - " + n);
    }
    Deliveries.close();
    return;
  }
  // Print "No ODeliveries available"
}

void clearDelivery(){
  // Display "Do you really want to Clear Deliveries?"
  listenForKey();
  if(keyPressed != ENTER_KEY){
    keyPressed = NO_KEY;
    deliveryMode();
  }
  // Print "Clearing Deliveries"
  if(SD.remove("deliveries.txt")){
    // Print "Deliveries Cleared!"
    deliveryMode();
    return;
  }
  // Print "Coundn't clear orders, pls try again!"
  deliveryMode();
  return;
}

// ORDERS
void addOders(){
  char d[waitingQueue];
  int q = 0;
  SD.remove("deliveries.txt");
  Deliveries = SD.open("deliveries.txt", FILE_WRITE);
  waitingQueue++;
  while(q < waitingQueue){
    // Print "Enter Table no for chamber q"
    listenForKey();
    if(keyPressed == BACK_KEY) q--;
    // if(q < 0) adminMode();
    d[q] = keyPressed;
    q++;
  }

  JSONVar deliveries = JSON.parse(d);
  String s = JSON.stringify(deliveries);
  Deliveries.println(s);
  Deliveries.close();
}

void viewOrders(){
  Orders = SD.open("orders.txt");
  if(Orders){
    JSONVar orders = JSON.parse(Orders.readString());
     for (int i = 0; i < orders.length(); i++) {
      int index = i + 1;
      String s = (String) index;
      String n = orders[i];
      Serial.println(s + " - " + n);
    }
    Orders.close();
    return;
  }
  // Print "No Orders available"
}

void clearOrders(){
    // Display "Do you really want to Clear Orders?"
  listenForKey();
  if(keyPressed != ENTER_KEY){
    keyPressed = NO_KEY;
    deliveryMode();
    return;
  }
  // Print "Clearing Deliveries"
  if(SD.remove("deliveries.txt")){
    // Print "Deliveries Cleared!"
    deliveryMode();
    return;
  }
  // Print "Coundn't clear orders, pls try again!"
  deliveryMode();
  return;
}

void OrdersInterupt (){
  // Get current table (Approximately)
  // compareCoordinates();
  char s[32];
  Orders = SD.open("orders.txt");
  String v = Orders.readString();
  v.toCharArray(s, v.length()+1);
  const char a[] = "[]";
  JSONVar data = v ? JSON.parse(s) : JSON.parse(a);
  data[sizeof(data)] = currentTable;
  Orders.close();

  Orders = SD.open("orders.txt", FILE_WRITE);
  Orders.println(JSON.stringify(data));
  Orders.close();
}


void serveCustomer(){
  // 
  switch (nextDirection) {
    case 'U':
      /// Do function
      //Trurn to the right face
    break;

    case 'D':
      /// Do function
    break;

    case 'B':
      /// Do function
    break;

    case 'F':
      /// Do function
    break;
  }
  openChamber(currentChamber);
  while (keyPressed != ENTER_KEY) {
    // Print "Please take your Orders \n Press the ENTER Button if you're done"
    listenForKey();

  }
  closeChamber(currentChamber);
  //Repeat the Function and continue
}


void gotoKitchen(){
  if (nextDirection == 'U') {
    _turn(90, 'L');
    // Go Until it reaches the Kitchen
  }
}


void doPathAlgorithm(String nextTable = ""){
  // Calculate dx and dy 
  dx = tableX - currentX;
  dy = tableY - currentY;

  // dx == 0 and dy == 0
  // his happens at the delivery spot (after delivery);
  if(dy == 0 && dx == 0){
    if(currentTable == "") gotoKitchen();
    if(currentTable != "") serveCustomer();
    // Re-calculate dx and dy
    currentTable = nextTable;
    if(nextTable != "") getTableCoordinates(nextTable);
    if(nextTable == ""){
      tableX = 0;
      tableY = 0;
      currentTable = "";
    }
    dx = tableX - currentX;
    dy = tableY - currentY;
     // For dx
    if(dx < dy){
      // For dx
      if(dx < 0){
        // Negative dy
        switch (nextDirection) {
          case 'U':
            _turn(90, 'L');
          break;
          case 'D':
            _turn(90, 'R');
          break;
          case 'B':
            // Just leave it
          break;
          case 'F':
            _turn(180);
          break;
        }
        nextDirection = 'B';
      }
      if(dx > 0){
        // Positive dx
        switch (nextDirection) {
          case 'U':
            _turn(90, 'R');
          break;
          case 'D':
            _turn(90, 'L');
          break;
          case 'B':
          _turn(180);
          break;
          case 'F':
          // Just move
          break;
        }
        nextDirection = 'F';
      }
    }

    // For dy
    if(dy < dx){
    
      if(dy > 0){
      // Positive dy
        switch (nextDirection) {
          case 'U':
            _turn(180);
          break;
          case 'D':
            // just move
          break;
          case 'B':
            _turn(90, 'L');
          break;
          case 'F':
            _turn(90, 'R');
          break;
        }
        nextDirection = 'D';
      }
      if(dy < 0){
        // Negative dy
        switch (nextDirection) {
          case 'U':
            _turn(90, 'R');
          break;
          case 'D':
            _turn(90, 'L');
          break;
          case 'B':
          _turn(180);
          break;
          case 'F':
          // Just move
          break;
        }
        nextDirection = 'U';
      }   
    }
  }
  else {
    

  // dy == 0 or dx == 0
  // This happens at the Juction to turn
  if (dy == 0 && dx != 0) {
    // For postive dx 
    if (dx > 0) {
      switch (nextDirection) {
        case 'U':
          _turn(90, 'R');
        break;
         case 'D':
          _turn(90, 'L');
        break;
      }
      nextDirection = 'F';
    }
    // Negative dx
    if (dx < 0) {
      switch (nextDirection) {
        case 'U':
          _turn(90, 'R');
        break;
         case 'D':
          _turn(90, 'L');
        break;
      }
      nextDirection = 'B';
    }
  }

  // For dy
 if (dx == 0 && dy != 0) {
    // For postive dy 
    if (dy > 0) {
      switch (nextDirection) {
        case 'B':
          _turn(90, 'R');
        break;
        case 'F':
          _turn(90, 'L');
        break;
      }
      nextDirection = 'D';
    }
    // Negative dx
    if (dx < 0) {
      switch (nextDirection) {
        case 'B':
          _turn(90, 'R');
        break;
        case 'F':
          _turn(90, 'L');
        break;
      }
      nextDirection = 'U';
    }
  }
 
  }

}

void getTableCoordinates(String table){
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++){
      if(Table[i][j] == table){
        tableY = i;
        tableX = j;
        return;
      }
    }
  }
}

void gotoTable(String _table, String nextTable){
  getTableCoordinates(_table);

  // Move to Path.
  doPathAlgorithm(nextTable);
}


void loopDeliveries(){
  Deliveries = SD.open("deliveries.txt");
  if(Deliveries){
    String s = Deliveries.readString();
    char f[32];
    s.toCharArray(f, s.length()+1);
    JSONVar deliveries = JSON.parse(f);
    for (int i = 0; i < sizeof(deliveries); i++) {
      String n = deliveries[i];
      currentChamber = i;
      currentTable = n;
      String nextTable = i < sizeof(deliveries) ? deliveries[i+1] : "";
      // Move until the destination is achieved
      gotoTable(currentTable, nextTable);
    }

    doPathAlgorithm();
    
  }
  // Print "No deliveries Available";
  return;
}




