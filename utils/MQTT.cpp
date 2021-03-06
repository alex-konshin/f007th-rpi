/*
 * MQTT.cpp
 *
 *  Created on: November 27, 2020
 *      Author: Alex Konshin <akonshin@gmail.com>
 */

#include <stdlib.h>
#include <stdint.h>

#include "Logger.hpp"
#include "../common/SensorsData.hpp"
#include "MQTT.hpp"
#include "../common/Config.hpp"

MqttPublisher* MqttPublisher::instance = NULL;

bool MqttPublisher::create(Config& cfg) {
  if (cfg.mqtt_enable && instance == NULL) {
    Log->log("Starting MQTT publisher to broker %s:%d...", cfg.mqtt_broker_host, cfg.mqtt_broker_port);
    MqttPublisher* mqtt_publisher = new MqttPublisher(cfg.mqtt_client_id, cfg.mqtt_broker_host, cfg.mqtt_broker_port, cfg.mqtt_username, cfg.mqtt_password, cfg.options);
    if (!mqtt_publisher->start() ) {
      Log->error("Could not connect to MQTT broker.");
      return false;
    }
  }
  return true;
}
void MqttRule::execute(const char* message, class Config& cfg) {
  MqttPublisher* publisher = MqttPublisher::instance;
  if (publisher != NULL && publisher->is_connected()) {
    bool debug = (cfg.options&VERBOSITY_DEBUG) != 0;
    if (debug) Log->info("%s \"%s\" => topic=\"%s\" message=\"%s\".", getTypeName(), id, mqttTopic, message);
    bool info = (cfg.options&VERBOSITY_INFO) != 0;
    if (info) Log->info("MQTT publishing: %s \"%s\"", mqttTopic, message);
    publisher->publish_message(mqttTopic, message);
  }
}
