/*====================================================================================
  This sketch contains support functions for the ESP6266 SPIFFS filing system

  Created by Bodmer 15th Jan 2017
  Updated by Bodmer to support ESP32 with SPIFFS Jan 2018
  ==================================================================================*/

  //====================================================================================
  //                 Print a SPIFFS directory list (root directory)
  //====================================================================================
#ifdef ESP8266
void listFiles(void) {
    Serial.println();
    Serial.println("SPIFFS files found:");

    fs::Dir dir = SPIFFS.openDir("/"); // Root directory
    String  line = "=====================================";

    Serial.println(line);
    Serial.println("  fs::File name               Size");
    Serial.println(line);

    while (dir.next()) {
        String fileName = dir.fileName();
        Serial.print(fileName);
        int spaces = 21 - fileName.length(); // Tabulate nicely
        while (spaces--) Serial.print(" ");

        fs::File f = dir.openFile("r");
        String fileSize = (String) f.size();
        spaces = 10 - fileSize.length(); // Tabulate nicely
        while (spaces--) Serial.print(" ");
        Serial.println(fileSize + " bytes");
    }

    Serial.println(line);
    Serial.println();
    delay(1000);
}
#endif

//====================================================================================

#ifdef ESP32

void listFiles(void) {
    listDir(SPIFFS , "/" , 0);
}

void listDir(fs::FS& fs , const char* dirname , uint8_t levels) {

    Serial.println();
    Serial.println("SPIFFS files found:");

    Serial.printf("Listing directory: %s\n" , "/");
    String  line = "=====================================";

    Serial.println(line);
    Serial.println("  fs::File name               Size");
    Serial.println(line);

    fs::File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    fs::File fs::File = root.openNextFile();
    while (fs::File) {

        if (fs::File.isDirectory()) {
            Serial.print("DIR : ");
            String fileName = fs::File.name();
            Serial.print(fileName);
            if (levels) {
                listDir(fs , fs::File.name() , levels - 1);
            }
        } else {
            String fileName = fs::File.name();
            Serial.print("  " + fileName);
            int spaces = 20 - fileName.length(); // Tabulate nicely
            while (spaces--) Serial.print(" ");
            String fileSize = (String) fs::File.size();
            spaces = 10 - fileSize.length(); // Tabulate nicely
            while (spaces--) Serial.print(" ");
            Serial.println(fileSize + " bytes");
        }

        fs::File = root.openNextFile();
    }

    Serial.println(line);
    Serial.println();
    delay(1000);
}
#endif
