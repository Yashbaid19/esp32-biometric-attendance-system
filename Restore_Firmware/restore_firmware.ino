#include <WiFi.h>
#include <Adafruit_Fingerprint.h>
#include <Firebase_ESP_Client.h>
#include "config.h"
#include "mbedtls/base64.h"
#include "mbedtls/aes.h"
#include "mbedtls/sha256.h"




HardwareSerial mySerial(2);

Adafruit_Fingerprint finger =
Adafruit_Fingerprint(&mySerial);

FirebaseData fbdo;

FirebaseAuth auth;

FirebaseConfig config;

uint8_t templateBuffer[2048];

size_t decodedTemplateSize = 0;

const char* MASTER_SECRET =
"BiometricAttendance2026";

const char* PROJECT_SALT =
"R307SecureCloud";

unsigned char aesKey[16];

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

bool decryptTemplate(
  uint8_t* encryptedData,
  size_t length
)
{
  mbedtls_aes_context aes;

  mbedtls_aes_init(&aes);

  mbedtls_aes_setkey_dec(
    &aes,
    aesKey,
    128
  );

  uint8_t decrypted[2048];

  memset(decrypted, 0, sizeof(decrypted));

  for (size_t i = 0; i < length; i += 16)
  {
    mbedtls_aes_crypt_ecb(
      &aes,
      MBEDTLS_AES_DECRYPT,
      encryptedData + i,
      decrypted + i
    );
  }

  memcpy(templateBuffer, decrypted, length);

  mbedtls_aes_free(&aes);

  return true;
}

bool restoreFingerprintTemplate(int id)
{
  String path =
  "/Employees/" +
  String(id) +
  "/template";

  if (!Firebase.RTDB.getString(&fbdo, path))
  {
    Serial.println("Failed to fetch template");

    Serial.println(fbdo.errorReason());

    return false;
  }

  String encodedTemplate =
  fbdo.stringData();

  if (encodedTemplate == "")
  {
    Serial.println("Template Empty");

    return false;
  }

  decodedTemplateSize = 0;

  int result = mbedtls_base64_decode(
    templateBuffer,
    sizeof(templateBuffer),
    &decodedTemplateSize,
    (const unsigned char*)
    encodedTemplate.c_str(),
    encodedTemplate.length()
  );

  if (result != 0)
  {
    Serial.println("Base64 Decode Failed");

    return false;
  }

  decryptTemplate(
    templateBuffer,
    decodedTemplateSize
  );

  Serial.println("Template Decrypted");

  Serial.println("Template Downloaded");

  Serial.print("Decoded Bytes: ");

  Serial.println(decodedTemplateSize);

  return true;
}

bool uploadTemplateToSensor()
{
  Serial.println(
    "Uploading Template To Sensor..."
  );

  uint8_t uploadCommand[] = {
    0xEF, 0x01,
    0xFF, 0xFF, 0xFF, 0xFF,
    0x01,
    0x00, 0x03,
    0x09,
    0x00,
    0x0D
  };

  mySerial.write(
    uploadCommand,
    sizeof(uploadCommand)
  );

  delay(500);

  int totalBytes = decodedTemplateSize;

  int chunkSize = 128;

  int offset = 0;

  while (offset < totalBytes)
  {
    int remaining =
    totalBytes - offset;

    int currentChunk =
    remaining > chunkSize
    ? chunkSize
    : remaining;

    uint8_t packetType =
    (offset + currentChunk >= totalBytes)
    ? 0x08
    : 0x02;

    uint16_t packetLength =
    currentChunk + 2;

    uint16_t checksum =
    packetType +
    (packetLength >> 8) +
    (packetLength & 0xFF);

    mySerial.write(0xEF);
    mySerial.write(0x01);

    mySerial.write(0xFF);
    mySerial.write(0xFF);
    mySerial.write(0xFF);
    mySerial.write(0xFF);

    mySerial.write(packetType);

    mySerial.write(packetLength >> 8);

    mySerial.write(packetLength & 0xFF);

    for (int i = 0; i < currentChunk; i++)
    {
      uint8_t b =
      templateBuffer[offset + i];

      mySerial.write(b);

      checksum += b;
    }

    mySerial.write(checksum >> 8);

    mySerial.write(checksum & 0xFF);

    offset += currentChunk;

    delay(50);
  }

  Serial.println(
    "Template Upload Complete"
  );

  return true;
}

bool saveRestoredTemplate(int id)
{
  Serial.println(
    "Attempting StoreModel..."
  );

  uint8_t p =
  finger.storeModel(id);

  Serial.print(
    "StoreModel Response: "
  );

  Serial.println(p);

  if (p == FINGERPRINT_OK)
  {
    Serial.println(
      "Template Stored Successfully"
    );

    return true;
  }

  Serial.println(
    "Failed To Store Template"
  );

  return false;
}

void restoreAllFingerprints()
{
  bool restoredAny = false;

  for (int id = 1; id <= 100; id++)
  {
    String path =
    "/Employees/" +
    String(id) +
    "/template";

    if (!Firebase.RTDB.getString(&fbdo, path))
    {
      continue;
    }

    String templateData =
    fbdo.stringData();

    if (templateData == "")
    {
      continue;
    }

    Serial.println("");

    Serial.println(
      "================================"
    );

    Serial.println(
      "Restoring Fingerprint ID: " +
      String(id)
    );

    bool fetched =
    restoreFingerprintTemplate(id);

    if (!fetched)
    {
      Serial.println(
        "Template Fetch Failed"
      );

      continue;
    }

    bool uploaded =
    uploadTemplateToSensor();

    if (!uploaded)
    {
      Serial.println(
        "Template Upload Failed"
      );

      continue;
    }

    delay(2000);

    bool saved =
    saveRestoredTemplate(id);

    if (!saved)
    {
      Serial.println(
        "StoreModel Failed"
      );

      continue;
    }

    restoredAny = true;

    Serial.println(
      "Fingerprint Restored Successfully"
    );
  }

  Serial.println("");

  if (restoredAny)
  {
    Serial.println(
      "ALL FINGERPRINTS RESTORED SUCCESSFULLY"
    );
  }
  else
  {
    Serial.println(
      "NO VALID FINGERPRINTS FOUND IN FIREBASE"
    );
  }
}

void setup()
{
  Serial.begin(115200);

  generateAESKey();

  mySerial.begin(
    57600,
    SERIAL_8N1,
    16,
    17
  );

  finger.begin(57600);

  if (finger.verifyPassword())
  {
    Serial.println(
      "Fingerprint Sensor Found"
    );
  }
  else
  {
    Serial.println(
      "Fingerprint Sensor NOT Found"
    );

    while (1);
  }

  WiFi.begin(ssid, pass);

  Serial.println(
    "Connecting WiFi..."
  );

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);

    Serial.print(".");
  }

  Serial.println("");

  Serial.println("WiFi Connected");

  config.api_key = API_KEY;

  config.database_url =
  DATABASE_URL;

  if (Firebase.signUp(
      &config,
      &auth,
      "",
      ""
    ))
  {
    Serial.println(
      "Firebase Signup Success"
    );
  }
  else
  {
    Serial.println(
      "Firebase Signup Failed"
    );

    Serial.println(
      config.signer.signupError
      .message.c_str()
    );
  }

  Firebase.begin(
    &config,
    &auth
  );

  Firebase.reconnectWiFi(true);

  Serial.println(
    "Firebase Connected"
  );

  restoreAllFingerprints();
}

void loop()
{
}