/*====================================================================================
  This sketch supports for the ESP6266 and ESP32 SPIFFS filing system

  Created by Bodmer 15th Jan 2017
  ==================================================================================*/

  //====================================================================================
  //                 Print a SPIFFS directory list (root directory)
  //====================================================================================

void listFiles(void) {
    Serial.println();
    Serial.println("SPIFFS files found:");

#ifdef ESP32
    listDir(SPIFFS , "/" , true);
#else
    fs::Dir dir = SPIFFS.openDir("/"); // Root directory
    String  line = "=====================================";

    Serial.println(line);
    Serial.println("  fs::File name               Size");
    Serial.println(line);

    while (dir.next()) {
        String fileName = dir.fileName();
        Serial.print(fileName);
        int spaces = 25 - fileName.length(); // Tabulate nicely
        if (spaces < 0) spaces = 1;
        while (spaces--) Serial.print(" ");
        fs::File f = dir.openFile("r");
        Serial.print(f.size()); Serial.println(" bytes");
        yield();
    }

    Serial.println(line);
#endif
    Serial.println();
    delay(1000);
}
//====================================================================================

#ifdef ESP32
void listDir(fs::FS& fs , const char* dirname , uint8_t levels) {
    Serial.printf("Listing directory: %s\n" , dirname);

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
            int spaces = 32 - fileName.length(); // Tabulate nicely
            if (spaces < 1) spaces = 1;
            while (spaces--) Serial.print(" ");
            String fileSize = (String) fs::File.size();
            spaces = 8 - fileSize.length(); // Tabulate nicely
            if (spaces < 1) spaces = 1;
            while (spaces--) Serial.print(" ");
            Serial.println(fileSize + " bytes");
        }

        fs::File = root.openNextFile();
    }
}
#endif
