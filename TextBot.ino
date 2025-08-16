#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#define MODEM_RX 16
#define MODEM_TX 17
#define PWRKEY 4

const char* ssid = ""; //PUT WIFI NAME HERE
const char* password = ""; // PUT PASSWORD HERE
const char* openai_api_key = "";  // Replace with your real key

HardwareSerial modem(1);

struct ChatHistory {
  String phone;
  String history;
  unsigned long lastSeen; // time of last activity in millis
};


const int MAX_HISTORY = 5;  // Number of users to track
ChatHistory chats[MAX_HISTORY];


void setup() {
  Serial.begin(115200);
  pinMode(PWRKEY, OUTPUT);
  digitalWrite(PWRKEY, HIGH);
  delay(100);
  digitalWrite(PWRKEY, LOW);
  delay(1000);
  digitalWrite(PWRKEY, HIGH);
  connectToWiFi();
  delay(1000);
  ArduinoOTA.begin();
  delay(7000);
  modem.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  sendAT("AT");
  sendAT("ATE0");
  sendAT("AT+CPIN?");
  sendAT("AT+CREG?");
  sendAT("AT+CSQ");
  sendAT("AT+COPS?");
  sendAT("AT+CMGF=1");          // Set SMS to text mode
  sendAT("AT+CNMI=2,1,0,0,0");  // Auto notify of new SMS
  processAllStoredSMS();
}

ChatHistory* getChatHistory(String phone) {
  unsigned long now = millis();

  for (int i = 0; i < MAX_HISTORY; i++) {
    if (chats[i].phone == phone) {
      // Check if this user's history is stale
      if (now - chats[i].lastSeen > 300000) { // 5 minutes = 300000 ms
        chats[i].history = "";
        Serial.println("Clearing old history for " + phone);
      }
      chats[i].lastSeen = now;
      return &chats[i];
    }
  }

  // Find empty slot
  for (int i = 0; i < MAX_HISTORY; i++) {
    if (chats[i].phone == "") {
      chats[i].phone = phone;
      chats[i].history = "";
      chats[i].lastSeen = now;
      return &chats[i];
    }
  }

  // No space left: overwrite first
  chats[0].phone = phone;
  chats[0].history = "";
  chats[0].lastSeen = now;
  return &chats[0];
}


String askChatGPT(String prompt, String phone) {
  ChatHistory* chat = getChatHistory(phone);
  chat->history += "User: " + prompt + "\n";

  // Compose full message history
  String messagesJson = R"([)";
  messagesJson += R"({ "role": "system", "content": "You are a witty and helpful assistant called Tether. Always reply in under 160 characters, suitable for SMS. Keep answers short and snappy." })";

  // Parse history into assistant/user turns
  int userIndex = 0;
  while (true) {
    int userStart = chat->history.indexOf("User: ", userIndex);
    if (userStart == -1) break;
    int botStart = chat->history.indexOf("Bot: ", userStart);

    if (userStart != -1 && botStart != -1) {
      String userMsg = chat->history.substring(userStart + 6, botStart);
      userMsg.trim();
      String botMsg = "";

      int nextUser = chat->history.indexOf("User: ", botStart + 5);
      if (nextUser != -1) {
        botMsg = chat->history.substring(botStart + 5, nextUser);
        botMsg.trim();
        userIndex = nextUser;
      } else {
        botMsg = chat->history.substring(botStart + 5);
        botMsg.trim();
        userIndex = chat->history.length();
      }


      messagesJson += ",";
      messagesJson += "{\"role\": \"user\", \"content\": \"" + userMsg + "\"}";
      messagesJson += ",";
      messagesJson += "{\"role\": \"assistant\", \"content\": \"" + botMsg + "\"}";
    } else {
      break;
    }
  }

  // Add current prompt
  messagesJson += ",";
  messagesJson += "{\"role\": \"user\", \"content\": \"" + prompt + "\"}";
  messagesJson += "]";

  // Send request
  HTTPClient http;
  http.begin("https://api.openai.com/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + openai_api_key);

  String body = "{\"model\": \"gpt-3.5-turbo-0125\", \"messages\": " + messagesJson + "}";
  int httpCode = http.POST(body);
  String response = "";

  if (httpCode > 0) {
    response = http.getString();
    Serial.println("Request:");
    Serial.println(body);
    Serial.println("Raw Response:");
    Serial.println(response);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      String content = doc["choices"][0]["message"]["content"].as<String>();
      content.trim();
      chat->history += "Bot: " + content + "\n";
      return content;
    } else {
      Serial.println("JSON parse error: " + String(error.c_str()));
      return "Error: GPT parse failed.";
    }
  } else {
    Serial.println("HTTP request failed");
    return "Error contacting GPT.";
  }

  http.end();
}


void sendSMS(String number, String message) {
  modem.print("AT+CMGS=\"");
  modem.print(number);
  modem.println("\"");
  delay(500);
  modem.print(message);
  delay(100);
  modem.write(26);  // CTRL+Z to send
  delay(3000);
  Serial.println("SMS sent back to " + number);
}

void processSMS(int index) {
  modem.print("AT+CMGR=");
  modem.println(index);
  delay(500);

  String number = "";
  String text = "";

  bool readingText = false;
  while (modem.available()) {
    String line = modem.readStringUntil('\n');
    line.trim();

    if (line.startsWith("+CMGR:")) {
      // Parse phone number - second quoted string
      int firstQuote = line.indexOf("\"");
      int secondQuote = line.indexOf("\"", firstQuote + 1);
      int thirdQuote = line.indexOf("\"", secondQuote + 1);
      int fourthQuote = line.indexOf("\"", thirdQuote + 1);

      if (thirdQuote != -1 && fourthQuote != -1) {
        number = line.substring(thirdQuote + 1, fourthQuote);
      }

      readingText = true;  // Next lines are message body
    } else if (readingText) {
      if (line == "OK" || line.startsWith("+") || line == "") {
        break;  // Stop reading text on known modem signals
      }

      if (text.length() > 0) {
        text += "\n";
      }
      text += line;
    }
  }


  if (number.length() && text.length()) {
    Serial.println("Asking ChatGPT: " + text);
    String reply = askChatGPT(text, number);
    sendSMS(number, reply);
  }

  // Delete message
  modem.print("AT+CMGD=");
  modem.println(index);
  delay(200);
}


void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n Wi-Fi Connected!");
}


void processAllStoredSMS() {
  Serial.println("📦 Checking for stored SMS...");
  modem.println("AT+CMGL=\"ALL\"");
  delay(500);

  while (modem.available()) {
    String line = modem.readStringUntil('\n');
    line.trim();

    if (line.startsWith("+CMGL:")) {
      int indexStart = line.indexOf(":") + 1;
      int indexEnd = line.indexOf(",", indexStart);
      int msgIndex = line.substring(indexStart, indexEnd).toInt();

      Serial.print("Found message at index: ");
      Serial.println(msgIndex);

      // Read message
      modem.print("AT+CMGR=");
      modem.println(msgIndex);
      delay(500);

      String senderInfo;
      String messageText;

      while (modem.available()) {
        String msgLine = modem.readStringUntil('\n');
        msgLine.trim();

        if (msgLine.startsWith("+CMGR:")) {
          senderInfo = msgLine;
        } else if (msgLine.length() > 0) {
          messageText = msgLine;
        }
      }

      Serial.println("From: " + senderInfo);
      Serial.println("Message: " + messageText);

      int numberStart = senderInfo.indexOf("\"") + 1;
      int numberEnd = senderInfo.indexOf("\"", numberStart);
      String senderNumber = senderInfo.substring(numberStart, numberEnd);

      String reply = askChatGPT(messageText, senderNumber);

      if (reply.length() > 160) reply = reply.substring(0, 160);

      sendSMS(senderNumber, reply);


      // Delete message
      modem.print("AT+CMGD=");
      modem.println(msgIndex);
      delay(200);

      while (modem.available()) {
        String delLine = modem.readStringUntil('\n');
        delLine.trim();
        if (delLine.length() > 0) {
          Serial.println("🗑️ " + delLine);
        }
      }
    }
  }
}


void loop() {
  ArduinoOTA.handle();
  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup > 60000) {  // every 60s
    lastCleanup = millis();
    unsigned long now = millis();
    for (int i = 0; i < MAX_HISTORY; i++) {
      if (chats[i].phone != "" && (now - chats[i].lastSeen > 300000)) {
        Serial.println("🧹 Removing expired chat for " + chats[i].phone);
        chats[i].phone = "";
        chats[i].history = "";
        chats[i].lastSeen = 0;
      }
    }
  }
  if (modem.available()) {
    String line = modem.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      Serial.println(">> " + line);

      // Check for new SMS notification
      if (line.startsWith("+CMTI:")) {
        int index = line.substring(line.lastIndexOf(',') + 1).toInt();
        Serial.print("SMS received in slot: ");
        Serial.println(index);
        processSMS(index);  // Send to ChatGPT and reply
        // Read the SMS from that slot


        delay(500);  // Wait for response

        while (modem.available()) {
          String smsLine = modem.readStringUntil('\n');
          smsLine.trim();
          if (smsLine.length() > 0) {
            Serial.println("📨 " + smsLine);
          }
        }

        // Optional: Delete SMS to free memory
        modem.print("AT+CMGD=");
        modem.println(index);
        delay(100);
        while (modem.available()) {
          String delLine = modem.readStringUntil('\n');
          delLine.trim();
          if (delLine.length() > 0) {
            Serial.println("🗑️ " + delLine);
          }
        }
      }
    }
  }
}


void sendAT(const char* cmd) {
  Serial.print("Sending: ");
  Serial.println(cmd);
  modem.println(cmd);

  unsigned long timeout = millis();
  while (millis() - timeout < 3000) {
    if (modem.available()) {
      String line = modem.readStringUntil('\n');
      if (line.length() > 1) {
        Serial.println("→ " + line);
      }
    }
  }
  Serial.println();
}
