//************************************************************
// this is a simple example that uses the painlessMesh library
//
// 1. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 2. prints anything it receives to Serial.print
//
//
//************************************************************
#include "painlessMesh.h"

#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>
#include <NfcAdapter.h>

PN532_SPI pn532spi(SPI, 5);
NfcAdapter nfc = NfcAdapter(pn532spi);

#define   MESH_PREFIX     "TEAM2"
#define   MESH_PASSWORD   "PASSWORD2"
#define   MESH_PORT       5555

typedef struct product {
  String categorie;
  String nume;
  String pret;
  String cantitate;
} product;

product products[10];

int nr_products;

Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;

// User stub
void sendMessage() ; // Prototype so PlatformIO doesn't complain

Task taskSendMessage( TASK_SECOND * 5 , TASK_FOREVER, &sendMessage );

bool is_already_in_cart(product current_product) {
  int i;
  
  for (i = 0; i < nr_products; i++) {
    if (products[i].categorie.equals(current_product.categorie) &&
    products[i].nume.equals(current_product.nume) &&
    products[i].pret.equals(current_product.pret) &&
    products[i].cantitate.equals(current_product.cantitate)) {
      return true;
    }
  }

  return false;
}

boolean isAsciiRange(String stringul){
  int i;
  
  for(i = 0; i < stringul.length(); i++) {
    if(stringul.charAt(i) != ' ' && (!(stringul.charAt(i) >= '0' && stringul.charAt(i) <= '9')) &&
        (!(stringul.charAt(i) >= 'A' && stringul.charAt(i) <= 'Z')) &&
         (!(stringul.charAt(i) >= 'a' && stringul.charAt(i) <= 'z')))
         return false;
  }
  return true;
}

bool replace_if_cheaper(product current_product) {
  int i;
  
  for (i = 0; i < nr_products; i++) {
    if (products[i].categorie.equals(current_product.categorie)) {
      if (products[i].pret.compareTo(current_product.pret) > 0) {
        products[i] = current_product;
      }
      return true;
    }
  }

  return false;
}

void handle_received_product(product received_product) {
  bool stat;

  stat = replace_if_cheaper(received_product);

  if (!stat) {
    products[nr_products++] = received_product;
  }
}

void sendMessage() {
  DynamicJsonDocument doc(1024), doc_monitor(1024);
  String msg, msg_monitor;
  int i;
  
  doc["nr_elem"] = nr_products;

  for (i = 0; i < nr_products; i++) {
    doc["products"][i]["categorie"] = products[i].categorie;
    doc["products"][i]["nume"]   = products[i].nume;
    doc["products"][i]["pret"] = products[i].pret;
    doc["products"][i]["cantitate"] = products[i].cantitate;
  }

  serializeJson(doc, msg);

  Serial.print("Sending: ");
  Serial.println(msg);

  doc_monitor["monitor"]["nr_elem"] = nr_products;

  for (i = 0; i < nr_products; i++) {
    doc_monitor["monitor"]["products"][i]["categorie"] = products[i].categorie;
    doc_monitor["monitor"]["products"][i]["nume"]   = products[i].nume;
    doc_monitor["monitor"]["products"][i]["pret"] = products[i].pret;
    doc_monitor["monitor"]["products"][i]["cantitate"] = products[i].cantitate;
  }

  serializeJson(doc_monitor, msg_monitor);

  Serial.print("Sending monitor: ");
  Serial.println(msg_monitor);
  
  mesh.sendBroadcast( msg );
  mesh.sendBroadcast( msg_monitor );
  taskSendMessage.setInterval( TASK_SECOND * 5);
}

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) {
  StaticJsonDocument<1200> doc;
  int nr_prod_received, i;

  DeserializationError error = deserializeJson(doc, msg.c_str());

  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());

    Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
    
    return;
  }
  else {
    String is_monitor = doc["monitor"];

    if (is_monitor.compareTo("")) {
      nr_prod_received = doc["nr_elem"];

      Serial.printf("Received nr_elem %d ", nr_prod_received);
  
      for (i = 0; i < nr_prod_received; i++) {
        product current_product;
  
        current_product.categorie = doc["products"][i]["categorie"].as<String>();
        current_product.nume = doc["products"][i]["nume"].as<String>();
        current_product.pret = doc["products"][i]["pret"].as<String>();
        current_product.cantitate = doc["products"][i]["cantitate"].as<String>();
  
        Serial.printf("%s %s %s %s\n", current_product.categorie, current_product.nume,
        current_product.pret, current_product.cantitate);
  
        handle_received_product(current_product);
      } 
    }
  }
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void setup() {
  Serial.begin(115200);

  Serial.println("Set up mesh");

  nr_products = 0;

//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();

  Serial.println("Started setting NFC up");

  nfc.begin();
}

void loop(void) {
  // it will run the user scheduler as well
  mesh.update();

  Serial.println("\nScan a NFC tag\n");

  if (nfc.tagPresent())
  {
    NfcTag tag = nfc.read();
    Serial.println(tag.getTagType());
    Serial.print("UID: ");Serial.println(tag.getUidString());

    if (tag.hasNdefMessage()) // every tag won't have a message
    {

      NdefMessage message = tag.getNdefMessage();
      Serial.print("\nThis NFC Tag contains an NDEF Message with ");
      Serial.print(message.getRecordCount());
      Serial.print(" NDEF Record");
      if (message.getRecordCount() != 1) {
        Serial.print("s");
      }
      Serial.println(".");

      // cycle through the records, printing some info from each
      int recordCount = message.getRecordCount();

      if (recordCount == 4) {
        product current_product;
      
        for (int i = 0; i < recordCount; i++)
        {
          Serial.print("\nNDEF Record ");Serial.println(i+1);
          NdefRecord record = message.getRecord(i);
          // NdefRecord record = message[i]; // alternate syntax
  
          Serial.print("  TNF: ");Serial.println(record.getTnf());
          Serial.print("  Type: ");Serial.println(record.getType()); // will be "" for TNF_EMPTY
  
          // The TNF and Type should be used to determine how your application processes the payload
          // There's no generic processing for the payload, it's returned as a byte[]
          int payloadLength = record.getPayloadLength();
          byte payload[payloadLength];
          record.getPayload(payload);
  
          // Print the Hex and Printable Characters
          Serial.print("  Payload (HEX): ");
          PrintHexChar(payload, payloadLength);
  
          // Force the data into a String (might work depending on the content)
          // Real code should use smarter processing
          String payloadAsString = "";
          for (int c = 0; c < payloadLength; c++) {
            payloadAsString += (char)payload[c];
          }
          Serial.print("  Payload (as String): ");
          Serial.println(payloadAsString);
  
          if (i == 0) {
            current_product.categorie = payloadAsString.substring(payloadAsString.indexOf(" ") + 1);
          }
          else if (i == 1) {
            current_product.nume = payloadAsString.substring(payloadAsString.indexOf(" ") + 1);
          }
          else if (i == 2) {
            current_product.pret = payloadAsString.substring(payloadAsString.indexOf(" ") + 1);
          }
          else if (i == 3) {
            current_product.cantitate = payloadAsString.substring(payloadAsString.indexOf(" ") + 1);
          }
          else {
            
          }
  
          // id is probably blank and will return ""
          String uid = record.getId();
          if (uid != "") {
            Serial.print("  ID: ");Serial.println(uid);
          }
        }
  
        if (isAsciiRange(current_product.categorie) &&
        isAsciiRange(current_product.nume) &&
        isAsciiRange(current_product.pret) &&
        isAsciiRange(current_product.cantitate) &&
        !is_already_in_cart(current_product)) {
          bool stat;
  
          stat = replace_if_cheaper(current_product);
  
          if (!stat) {
            products[nr_products++] = current_product;
          }

          taskSendMessage.setInterval( TASK_SECOND * 1);
        } 
      }
    }
  }
  
  delay(1000);
}
