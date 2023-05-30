
#include "Arduino.h"

#include "sensesp.h"
#include "sensesp/sensors/analog_input.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/transforms/linear.h"
#include "sensesp_app.h"
#include "sensesp_app_builder.h"
#include "ui_configurables.h"

// #include <StringSplitter.h>

#include <NimBLEDevice.h>


using namespace sensesp;

// StringConfig *conf_whitelist;
// StringSplitter *split_whitelist;

NimBLEScan* pBLEScan;

void printWhitelist();
void decodeV3(const char* macaddr, int signal, const char* data);
void decodeV5(const char* macaddr, int signal, const char* data);


class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
      if (advertisedDevice->getManufacturerData()[0] == 0x99 && advertisedDevice->getManufacturerData()[1] == 0x04) {
/*
        debugI("FOUND RUUVI TAG: %s - %s\n",
                std::string(advertisedDevice->getAddress()).c_str(),
                std::string(advertisedDevice->getManufacturerData()).c_str());
*/ 
        debugI("FOUND RUUVI TAG: %s\n", advertisedDevice->toString().c_str());
      }

//      if (advertisedDevice->getManufacturerData()[2] == 3) {
//        decodeV3();
//      }
      if (advertisedDevice->getManufacturerData()[2] == 5) {
        char data[25];
        for (uint8_t i = 0; i < sizeof(advertisedDevice->getManufacturerData()); i++){
          data[i] = advertisedDevice->getManufacturerData()[i];
        }

        decodeV5( advertisedDevice->getAddress().toString().c_str(),
                  advertisedDevice->getRSSI(),
                  data);
      }
    }
};

// SensESP builds upon the ReactESP framework. Every ReactESP application
// must instantiate the "app" object.
reactesp::ReactESP app;

// The setup function performs one-time application initialization.
void setup() {
// Some initialization boilerplate when in debug mode...
#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(115200);
#endif

  // Create the global SensESPApp() object.
  SensESPAppBuilder builder;
  sensesp_app = builder.set_hostname("SensESP-ruuvi-bridge")
                    ->enable_ota("thisisfine")
                    ->get_app();

//  app.onRepeat(2000, []() {printWhitelist();});

/*
 *  CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE (2)
 *  Filter by address and data, advertisements from the same address will be reported only once,
 *  except if the data in the advertisement has changed, then it will be reported again.
*/
//  NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);

/** *Optional* Sets the scan filter cache size in the BLE controller.
 *  When the number of duplicate advertisements seen by the controller
 *  reaches this value it will clear the cache and start reporting previously
 *  seen devices. The larger this number, the longer time between repeated
 *  device reports. Range 10 - 1000. (default 20)
 *
 *  Can only be used BEFORE calling NimBLEDevice::init.
 */
  // NimBLEDevice::setScanDuplicateCacheSize(200);

  NimBLEDevice::init("");

  pBLEScan = NimBLEDevice::getScan(); //create new scan
  // Set the callback for when devices are discovered, no duplicates.
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
  pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
  pBLEScan->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
  pBLEScan->setWindow(37);  // How long to scan during the interval; in milliseconds.
  pBLEScan->setMaxResults(0); // do not store the scan results, use callback only.
  pBLEScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);

/*
  conf_whitelist = new StringConfig("aa:bb:cc:dd:ee:ff,11:22:33:44:55:66", "/Settings/Whitelist", 
    "comma-separated list (NO SPACES!) of the MAC addresses of your ruuvi-Tags", 1000 );

  debugD("whitelist: %s\n", conf_whitelist->get_value().c_str());

  split_whitelist = new StringSplitter(conf_whitelist->get_value(), ',', 15);

  int itemCount = split_whitelist->getItemCount();
  for(int i = 0; i < itemCount; i++){
    String item = split_whitelist->getItemAtIndex(i);
    debugD("split item: --%s--\n", item.c_str());
    std::string macaddr(item.c_str());
    NimBLEDevice::whiteListAdd(macaddr);
  }
*/

  // Start the SensESP application running
  sensesp_app->start();
}

// The loop function is called in an endless loop during program execution.
// It simply calls `app.tick()` which will then execute all reactions as needed.
void loop() { 

  // If an error occurs that stops the scan, it will be restarted here.
  if(pBLEScan->isScanning() == false) {
      debugD("restarting scanner");
      // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
      pBLEScan->start(0, nullptr, false);
  }

  app.tick(); 
}

void printWhitelist() {
  Serial.println("Whitelist contains:");
  for (auto i=0; i<NimBLEDevice::getWhiteListCount(); ++i) {
    Serial.println(NimBLEDevice::getWhiteListAddress(i).toString().c_str());
  }
};

void decodeV3(const char* macaddr, int signal, const char* data) {

    double temperature = data[4] & 0b01111111;
    temperature = temperature + ((double)data[5] / 100);
    if (data[4] & 0b10000000 == 0) {temperature = temperature * -1;}
    double pressure = ((unsigned short)data[6]<<8) + (unsigned short)data[7] + 50000;
    short accelX  = ((unsigned short)data[8]<<8)  +  (unsigned short)data[9];
    short accelY  = ((unsigned short)data[10]<<8) +  (unsigned short)data[11];
    short accelZ  = ((unsigned short)data[12]<<8) +  (unsigned short)data[13];
    short voltage = ((unsigned short)data[14]<<8) +  (unsigned short)data[15];

//    debugD(fmt3, temperature,(double)data[3]/2, pressure,
//            (double)accelX/1000,(double)accelY/1000,(double)accelZ/1000,double(voltage)/1000,signal);
}

void decodeV5(const char* macaddr, int signal, const char* data) {
    short temperature = ((short)data[3]<<8) | (unsigned short)data[4];
    unsigned short humidity = ((unsigned short)data[5]<<8) | (unsigned short)data[6];
    float pressure    = ((unsigned short)data[7]<<8)  + (unsigned short)data[8] + 50000;
    
    short accelX  = ((short)data[9]<<8)  | (short)data[10];
    short accelY  = ((short)data[11]<<8) | (short)data[12];
    short accelZ  = ((short)data[13]<<8) | (short)data[14];
    
    unsigned short foo = ((unsigned short)data[15] << 8) + (unsigned short)data[16];
    float voltage = ((double)foo / 32  + 1600)/1000;
    short power = (((data[16] & 0x1f)*2)-40);
    unsigned short seqnumber = ((unsigned short)data[18] << 8) + (unsigned short)data[19];

    debugD("%s - t:%02.1f h:%02.1f p:%04.2f aX:%03.3f aY:%03.3f aZ:%03.3f v:%02.2f s:%d\n",
          macaddr,
          (float)temperature*0.005,
          (unsigned int)humidity*0.0025,
          pressure/100,
          (float)accelX/1000,
          (float)accelY/1000,
          (float)accelZ/1000,
          voltage,
          signal);
}
