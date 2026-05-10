#include "config.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_Fingerprint.h>
#include <WidgetTerminal.h>
#include <Firebase_ESP_Client.h>
#include <HTTPClient.h>
#include "base64.h"
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"


HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

WidgetTerminal terminal(V4);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
const char* MASTER_SECRET =
"BiometricAttendance2026";

const char* PROJECT_SALT =
"R307SecureCloud";

unsigned char aesKey[16];
int fingerprintID = 1;

String userNames[30];

String userEmails[50];

String currentName = "";

String currentEmail = "";
unsigned long lastScanTime = 0;

const int scanCooldown = 5000;

int failedAttempts = 0;

bool sensorLocked = false;

unsigned long lockStartTime = 0;

const int maxFailedAttempts = 5;

const int lockDuration = 30000;

uint8_t templateBuffer[1024];

void logMessage(String msg)
{
  Serial.println(msg);

  terminal.println(msg);

  terminal.flush();
}

BLYNK_WRITE(V0)
{
  int value = param.asInt();

  if (value == 1)
  {
    logMessage("Starting Enrollment");

    Blynk.virtualWrite(V1, 1);

    enrollFingerprint(fingerprintID);

    fingerprintID++;

    Blynk.virtualWrite(V1, 0);
  }
}
BLYNK_WRITE(V2)
{
  currentName = param.asString();

  logMessage("Name Received: " + currentName);
}
BLYNK_WRITE(V3)
{
  currentEmail = param.asString();

  logMessage("Email Received: " + currentEmail);
}
String encryptTemplate(uint8_t* data, size_t length)
{
  mbedtls_aes_context aes;

  mbedtls_aes_init(&aes);

  mbedtls_aes_setkey_enc(
    &aes,
    aesKey,
    128
  );

  size_t paddedLength =
  ((length + 15) / 16) * 16;

  uint8_t paddedData[2048];

  memset(paddedData, 0, sizeof(paddedData));

  memcpy(paddedData, data, length);

  uint8_t encrypted[2048];

  memset(encrypted, 0, sizeof(encrypted));

  for (size_t i = 0; i < paddedLength; i += 16)
  {
    mbedtls_aes_crypt_ecb(
      &aes,
      MBEDTLS_AES_ENCRYPT,
      paddedData + i,
      encrypted + i
    );
  }

  mbedtls_aes_free(&aes);

  String encoded =
  base64::encode(encrypted, paddedLength);

  return encoded;
}
uint8_t enrollFingerprint(int id)
{
  int p = -1;

  logMessage("Place Finger");

  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
  }

  p = finger.image2Tz(1);

  if (p != FINGERPRINT_OK)
  {
    logMessage("Image Conversion Failed");
    return p;
  }

  logMessage("Remove Finger");

  delay(2000);

  p = 0;

  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }

  logMessage("Place Same Finger Again");

  p = -1;

  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
  }

  p = finger.image2Tz(2);

  if (p != FINGERPRINT_OK)
  {
    logMessage("Second Image Failed");
    return p;
  }

  p = finger.createModel();

  if (p != FINGERPRINT_OK)
  {
    logMessage("Fingerprints Did Not Match");
    return p;
  }

  p = finger.storeModel(id);

  if (p == FINGERPRINT_OK)
  {
    userNames[id] = currentName;

    userEmails[id] = currentEmail;

    logMessage("Fingerprint Enrolled Successfully");

    String storedMsg = "Stored at ID: " + String(id);

    logMessage(storedMsg);

    logMessage("Name: " + currentName);

    logMessage("Email: " + currentEmail);

    String path = "/Employees/" + String(id);

    bool success = true;

    success &= Firebase.RTDB.setString(&fbdo, path + "/name", currentName);

    success &= Firebase.RTDB.setString(&fbdo, path + "/email", currentEmail);

    success &= Firebase.RTDB.setInt(&fbdo, path + "/id", id);

    if(success)
    {
      logMessage("Uploaded to Firebase");
      String fingerprintTemplate =
      downloadFingerprintTemplate(id);

      if (fingerprintTemplate != "")
      {
        Firebase.RTDB.setString(
        &fbdo,
        path + "/template",
        fingerprintTemplate
        );

        logMessage("Fingerprint Template Backed Up");
      }
      else
      {
        logMessage("Template Backup Failed");
      }
    }
    else
    {
      logMessage("Firebase Upload Failed");
      logMessage(fbdo.errorReason());
    }
    return true;
  }
  else
  {
    logMessage("Storage Failed");
    return p;
  }
}

int getFingerprintID()
{
  if (sensorLocked)
  {
    if (
      millis() - lockStartTime
      < lockDuration
    )
    {
      logMessage(
        "SYSTEM LOCKED"
      );

      return -1;
    }
    else
    {
      sensorLocked = false;

      failedAttempts = 0;

      logMessage(
        "System Unlocked"
      );
    }
  }
  uint8_t p = finger.getImage();

  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.image2Tz();

  if (p != FINGERPRINT_OK)
    return -1;

  p = finger.fingerSearch();

  if (p != FINGERPRINT_OK)
  {
    failedAttempts++;

    logMessage(
      "Unknown Finger Attempt: " +
      String(failedAttempts)
    );

    logSecurityEvent(
      "Unknown Finger"
    );

    if (
      failedAttempts >=
      maxFailedAttempts
    )
    {
      sensorLocked = true;

      lockStartTime = millis();

      logMessage(
        "SYSTEM TEMPORARILY LOCKED"
      );

      logSecurityEvent(
        "System Locked"
      );
    }

    return -1;
  }

  int id = finger.fingerID;

  if (
    millis() - lastScanTime
    < scanCooldown
  )
  {
    logMessage(
      "Please Wait..."
    );

    return -1;
  }

  lastScanTime = millis();

  failedAttempts = 0;

  String path = "/Employees/" + String(id);

  String name = "";

  String email = "";

  if (Firebase.RTDB.getString(&fbdo, path + "/name"))
  {
    name = fbdo.stringData();
  }

  if (Firebase.RTDB.getString(&fbdo, path + "/email"))
  {
    email = fbdo.stringData();
  }

  String encodedName = name;

  String encodedEmail = email;

  encodedName.replace(" ", "%20");

  encodedEmail.replace("@", "%40");

  encodedEmail.replace(".", "%2E");

  String url = scriptURL +
               "?id=" + String(id) +
               "&name=" + encodedName +
               "&email=" + encodedEmail;

  HTTPClient http;

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  http.begin(url);

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0)
  {
    String response = http.getString();

    String message = name + " " + response;

    logMessage(message);

    Blynk.logEvent("attendance_alert", message);
  }
  else
  {
    logMessage("HTTP Request Failed");
  }

  http.end();

  return finger.fingerID;
}

String downloadFingerprintTemplate(uint16_t id)
{
  uint8_t p = finger.loadModel(id);

  if (p != FINGERPRINT_OK)
  {
    logMessage("Failed to load template");

    return "";
  }

  p = finger.getModel();

  if (p != FINGERPRINT_OK)
  {
    logMessage("Failed to get template");

    return "";
  }

  delay(500);

  int index = 0;

  while (mySerial.available() && index < 1024)
  {
    templateBuffer[index++] = mySerial.read();
  }

  if (index == 0)
  {
    logMessage("No template data received");

    return "";
  }

  String encryptedTemplate =
  encryptTemplate(templateBuffer, index);

  return encryptedTemplate;
}
void generateAESKey()
{
  String combined =
  String(MASTER_SECRET) +
  String(PROJECT_SALT);

  unsigned char shaResult[32];

  mbedtls_sha256(
    (const unsigned char*)combined.c_str(),
    combined.length(),
    shaResult,
    0
  );

  memcpy(aesKey, shaResult, 16);

  Serial.println("AES Key Generated");
}
void logSecurityEvent(String event)
{
  String path =
  "/SecurityLogs/" +
  String(millis());

  Firebase.RTDB.setString(
    &fbdo,
    path + "/event",
    event
  );

  Firebase.RTDB.setInt(
    &fbdo,
    path + "/attempts",
    failedAttempts
  );

  Serial.println(
    "Security Event Logged"
  );
}
void setup()
{
  Serial.begin(115200);
  generateAESKey();

  mySerial.begin(57600, SERIAL_8N1, 16, 17);

  finger.begin(57600);
  finger.getTemplateCount();

  fingerprintID = finger.templateCount + 1;

  Serial.print("Next Fingerprint ID: ");
  Serial.println(fingerprintID);

  logMessage("Next ID: " + String(fingerprintID));

  if (finger.verifyPassword())
  {
    logMessage("Fingerprint sensor found");
  }
  else
  {
    logMessage("Fingerprint sensor NOT found");

    while (1);
  }

  Serial.println("Connecting WiFi...");

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");
  

  config.api_key = API_KEY;

  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", ""))
  {
    logMessage("Firebase Signup Success");
  }
  else
  {
    logMessage("Firebase Signup Failed");
    logMessage(config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);

  logMessage("Firebase Connected");

  Blynk.config(BLYNK_AUTH_TOKEN);

  Blynk.connect();

  logMessage("Connected to Blynk");
}

void loop()
{
  Blynk.run();

  getFingerprintID();

  delay(1000);
}