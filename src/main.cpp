
#include <Arduino.h>

#include "sensesp/sensors/analog_input.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/transforms/linear.h"
#include "sensesp_app.h"
#include "sensesp_app_builder.h"

#include <NimBLEDevice.h>


using namespace sensesp;

NimBLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
      debugI("Advertised Device: %s \n", advertisedDevice->toString().c_str());
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


  debugI("scanner starting...\n");

  // Create the global SensESPApp() object.
  SensESPAppBuilder builder;
  sensesp_app = builder.set_hostname("SensESP-ruuvi-bridge")
                    ->enable_ota("thisisfine")
                    ->get_app();

/*
 *  CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE (2)
 *  Filter by address and data, advertisements from the same address will be reported only once,
 *  except if the data in the advertisement has changed, then it will be reported again.
*/
  NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE);

/** *Optional* Sets the scan filter cache size in the BLE controller.
 *  When the number of duplicate advertisements seen by the controller
 *  reaches this value it will clear the cache and start reporting previously
 *  seen devices. The larger this number, the longer time between repeated
 *  device reports. Range 10 - 1000. (default 20)
 *
 *  Can only be used BEFORE calling NimBLEDevice::init.
 */
  NimBLEDevice::setScanDuplicateCacheSize(200);

  NimBLEDevice::init("");

  pBLEScan = NimBLEDevice::getScan(); //create new scan
  // Set the callback for when devices are discovered, no duplicates.
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
  pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
  pBLEScan->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
  pBLEScan->setWindow(37);  // How long to scan during the interval; in milliseconds.
  pBLEScan->setMaxResults(0); // do not store the scan results, use callback only.


  // Start the SensESP application running
  sensesp_app->start();
  pBLEScan->start(0, nullptr, false);
  debugI("scanner started\n");
}

// The loop function is called in an endless loop during program execution.
// It simply calls `app.tick()` which will then execute all reactions as needed.
void loop() { 

/*
  // If an error occurs that stops the scan, it will be restarted here.
  if(pBLEScan->isScanning() == false) {
      // Start scan with: duration = 0 seconds(forever), no scan end callback, not a continuation of a previous scan.
      pBLEScan->start(0, nullptr, false);
  }
*/

  app.tick(); 
}
