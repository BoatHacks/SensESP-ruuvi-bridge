
#include "Arduino.h"

#include "sensesp.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/signalk/signalk_metadata.h"
#include "sensesp_app.h"
#include "sensesp_app_builder.h"
#include "ui_configurables.h"

#include <StringSplitter.h>

#include <NimBLEDevice.h>

using namespace sensesp;

StringConfig *conf_allowlist;
StringSplitter *split_allowlist;

#define NUM_TAGS 5
SKOutputFloat* temperatureOut[NUM_TAGS];
SKOutputFloat* humidityOut[NUM_TAGS];
SKOutputFloat* pressureOut[NUM_TAGS];
SKOutputFloat* voltageOut[NUM_TAGS];
SKOutputInt* signalOut[NUM_TAGS];
std::string maclist[NUM_TAGS];

NimBLEScan* pBLEScan;

void printAllowlist();
void decodeV3(const char* macaddr, const int signal, const char* data);
void decodeV5(const char* macaddr, const int signal, const char* data);
void constructSKOutput(const char* macaddr, const int idx);

class MyAdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
      if (advertisedDevice->getManufacturerData()[0] == 0x99 && advertisedDevice->getManufacturerData()[1] == 0x04) {
        debugI("found ruuvi-tag: %s\n", advertisedDevice->toString().c_str());
      }

      if (advertisedDevice->getManufacturerData()[2] == 3) {
        char data[25];
        for (uint8_t i = 0; i < sizeof(advertisedDevice->getManufacturerData()); i++){
          data[i] = advertisedDevice->getManufacturerData()[i];
        }
        decodeV3( advertisedDevice->getAddress().toString().c_str(),
                  advertisedDevice->getRSSI(),
                  data);
      }
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
                    ->enable_system_info_sensors()
                    ->enable_ota("thisisfine")
                    ->get_app();

  NimBLEDevice::init("");

  pBLEScan = NimBLEDevice::getScan(); //create new scan
  // Set the callback for when devices are discovered, no duplicates.
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
  pBLEScan->setActiveScan(true); // Set active scanning, this will get more data from the advertiser.
  pBLEScan->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
  pBLEScan->setWindow(37);  // How long to scan during the interval; in milliseconds.
  pBLEScan->setMaxResults(0); // do not store the scan results, use callback only.
  pBLEScan->setFilterPolicy(BLE_HCI_SCAN_FILT_NO_WL);

  conf_allowlist = new StringConfig("", "/Settings/Allowlist", 
    "comma-separated list (NO SPACES!) of the MAC addresses (in lowercase) of your ruuvi-tags", 100 );

  debugD("allowlist: %s\n", conf_allowlist->get_value().c_str());

  split_allowlist = new StringSplitter(conf_allowlist->get_value(), ',', 15);

  int itemCount = split_allowlist->getItemCount();
  for(int i = 0; i < itemCount; i++){
    String item = split_allowlist->getItemAtIndex(i);
    debugD("split item: --%s--\n", item.c_str());
    constructSKOutput(item.c_str(), i);
    maclist[i] = item.c_str();
  }

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

void decodeV3(const char* macaddr, const int signal, const char* data) {

    double temperature = data[4] & 0b01111111;
    temperature = temperature + ((double)data[5] / 100);
    if (data[4] & 0b10000000 == 0) {temperature = temperature * -1;}
    double pressure = ((unsigned short)data[6]<<8) + (unsigned short)data[7] + 50000;
    //  measurement intervals are too long to be of any use, save some memory and not even parse acceleration
    short voltage = ((unsigned short)data[14]<<8) + (unsigned short)data[15];

    debugD("v3: %s - t:%02.1f h:%02.1f p:%06.f v:%02.2f s:%d\n",
            temperature,
            (double)data[3]/2,
            pressure,
            double(voltage)/1000,
            signal);

    for (int i=0; i < NUM_TAGS; i++) {
      if ((String)maclist[i].c_str() == (String)macaddr) {
        debugI("found %s in allowlist at index %i", maclist[i].c_str(), i);
        temperatureOut[i]->set_input(temperature + 272.15);
        humidityOut[i]->set_input(((double)data[3]/2)/100);
        pressureOut[i]->set_input(pressure);
        voltageOut[i]->set_input(double(voltage)/1000);
        signalOut[i]->set_input(signal);
      }
    }

}

void decodeV5(const char* macaddr, const int signal, const char* data) {
    short temperature = ((short)data[3]<<8) | (unsigned short)data[4];
    unsigned short humidity = ((unsigned short)data[5]<<8) | (unsigned short)data[6];
    float pressure = ((unsigned short)data[7]<<8)  + (unsigned short)data[8] + 50000;
    //  measurement intervals are too long to be of any use, save some memory and not even parse acceleration
    unsigned short extra = ((unsigned short)data[15] << 8) + (unsigned short)data[16];
    float voltage = ((double)extra / 32 + 1600)/1000;

    debugD("%s - t:%02.1f h:%02.1f p:%06.f v:%02.2f s:%d\n",
          macaddr,
          (float)temperature*0.005,
          (unsigned int)humidity*0.0025,
          pressure,
          voltage,
          signal);

    for (int i=0; i < NUM_TAGS; i++) {
      if ((String)maclist[i].c_str() == (String)macaddr) {
        debugI("found %s in allowlist at index %i", maclist[i].c_str(), i);
        temperatureOut[i]->set_input((float)temperature*0.005 + 272.15);
        humidityOut[i]->set_input(((unsigned int)humidity*0.0025)/100);
        pressureOut[i]->set_input(pressure);
        voltageOut[i]->set_input(voltage);
        signalOut[i]->set_input(signal);
      }
    }
}

void constructSKOutput(const char* macaddr, const int idx) {
  temperatureOut[idx] = new SKOutputFloat("environment." + (String)macaddr + ".temperature",
                                            "/Paths/" + (String)macaddr + "/Temperature",
                                            "K");
  humidityOut[idx] = new SKOutputFloat("environment." + (String)macaddr + ".relativeHumidity",
                                            "/Paths/" + (String)macaddr + "/Humidity",
                                            "ratio");
  pressureOut[idx] = new SKOutputFloat("environment." + (String)macaddr + ".pressure",
                                            "/Paths/" + (String)macaddr + "/Pressure",
                                            "Pa");
  voltageOut[idx] = new SKOutputFloat("sensorDevice.ruuvi-" + (String)macaddr + ".voltage",
                                            "/Paths/" + (String)macaddr + "/Voltage",
                                            "V");
  signalOut[idx] = new SKOutputInt("sensorDevice.ruuvi-" + (String)macaddr + ".signal",
                                            "/Paths/" + (String)macaddr + "/Signal",
                                            "dBm");
  debugI("created SK connections for %s, index %d", macaddr, idx);
}
