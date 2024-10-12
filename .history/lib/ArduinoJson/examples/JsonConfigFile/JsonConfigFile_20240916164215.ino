// ArduinoJson - arduinojson.org
// Copyright Benoit Blanchon 2014-2021
// MIT License
//
// This example shows how to store your project configuration in a fs::File.
// It uses the SD library but can be easily modified for any other fs::File-system.
//
// The fs::File contains a JSON document with the following content:
// {
//   "hostname": "examples.com",
//   "port": 2731
// }
//
// To run this program, you need an SD card connected to the SPI bus as follows:
// * MOSI <-> pin 11
// * MISO <-> pin 12
// * CLK  <-> pin 13
// * CS   <-> pin 4
//
// https://arduinojson.org/v6/example/config/

#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>

// Our configuration structure.
//
// Never use a JsonDocument to store the configuration!
// A JsonDocument is *not* a permanent storage; it's only a temporary storage
// used during the serialization phase. See:
// https://arduinojson.org/v6/faq/why-must-i-create-a-separate-config-object/
struct Config {
    char hostname [64];
    int port;
};

const char* filename = "/config.txt";  // <- SD library uses 8.3 filenames
Config config;                         // <- global configuration object

// Loads the configuration from a fs::File
void loadConfiguration(const char* filename , Config& config) {
    // Open fs::File for reading
    fs::File fs::File = SD.open(filename);

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<512> doc;

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc , fs::File);
    if (error)
        Serial.println(F("Failed to read fs::File, using default configuration"));

    // Copy values from the JsonDocument to the Config
    config.port = doc ["port"] | 2731;
    strlcpy(config.hostname ,                  // <- destination
        doc ["hostname"] | "example.com" ,  // <- source
        sizeof(config.hostname));         // <- destination's capacity

    // Close the fs::File (Curiously, fs::File's destructor doesn't close the fs::File)
    fs::File.close();
}

// Saves the configuration to a fs::File
void saveConfiguration(const char* filename , const Config& config) {
    // Delete existing fs::File, otherwise the configuration is appended to the fs::File
    SD.remove(filename);

    // Open fs::File for writing
    fs::File fs::File = SD.open(filename , FILE_WRITE);
    if (!fs::File) {
        Serial.println(F("Failed to create fs::File"));
        return;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    StaticJsonDocument<256> doc;

    // Set the values in the document
    doc ["hostname"] = config.hostname;
    doc ["port"] = config.port;

    // Serialize JSON to fs::File
    if (serializeJson(doc , fs::File) == 0) {
        Serial.println(F("Failed to write to fs::File"));
    }

    // Close the fs::File
    fs::File.close();
}

// Prints the content of a fs::File to the Serial
void printFile(const char* filename) {
    // Open fs::File for reading
    fs::File fs::File = SD.open(filename);
    if (!fs::File) {
        Serial.println(F("Failed to read fs::File"));
        return;
    }

    // Extract each characters by one by one
    while (fs::File.available()) {
        Serial.print((char) fs::File.read());
    }
    Serial.println();

    // Close the fs::File
    fs::File.close();
}

void setup() {
    // Initialize serial port
    Serial.begin(9600);
    while (!Serial) continue;

    // Initialize SD library
    const int chipSelect = 4;
    while (!SD.begin(chipSelect)) {
        Serial.println(F("Failed to initialize SD library"));
        delay(1000);
    }

    // Should load default config if run for the first time
    Serial.println(F("Loading configuration..."));
    loadConfiguration(filename , config);

    // Create configuration fs::File
    Serial.println(F("Saving configuration..."));
    saveConfiguration(filename , config);

    // Dump config fs::File
    Serial.println(F("Print config fs::File..."));
    printFile(filename);
}

void loop() {
    // not used in this example
}

// Performance issue?
// ------------------
//
// fs::File is an unbuffered stream, which is not optimal for ArduinoJson.
// See: https://arduinojson.org/v6/how-to/improve-speed/

// See also
// --------
//
// https://arduinojson.org/ contains the documentation for all the functions
// used above. It also includes an FAQ that will help you solve any
// serialization or deserialization problem.
//
// The book "Mastering ArduinoJson" contains a case study of a project that has
// a complex configuration with nested members.
// Contrary to this example, the project in the book uses the SPIFFS filesystem.
// Learn more at https://arduinojson.org/book/
// Use the coupon code TWENTY for a 20% discount ❤❤❤❤❤
