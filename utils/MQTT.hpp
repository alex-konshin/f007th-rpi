/*
 * MQTT.hpp
 *
 *  Created on: February 15, 2020
 *      Author: Alex Konshin <akonshin@gmail.com>
 */

#ifndef MQTT_HPP_
#define MQTT_HPP_

#include <string.h>
#include <mosquittopp.h>
#include <errno.h>

#include "Logger.hpp"
#include "../common/Config.hpp"

class Config;

class MqttPublisher: public mosqpp::mosquittopp {

private:
  const char *host;
  const char *id;
  int port;
  int keepalive;
  int options;
  volatile bool connected = false;

public:
  MqttPublisher(const char* id, const char* host, int port, int options = 0, int keepalive = 60) : mosquittopp(id) {
    mosqpp::lib_init();      // Initialization of mosquitto library
    this->keepalive = keepalive;
    this->options = options;
    this->id = id;
    this->port = port;
    this->host = host;
    instance = this;
  }

  ~MqttPublisher() {
    if (connected) stop(true);
    mosqpp::lib_cleanup();   // Mosquitto library cleanup
  }

  static MqttPublisher* instance;

  static bool create(Config& cfg);

  static void destroy() {
    if (instance != NULL) {
      MqttPublisher* publisher = instance;
      instance = NULL;
      if (publisher->is_connected()) {
        Log->log("Stopping MQTT publisher...");
        publisher->stop(true);
        Log->log("MQTT publisher has been stopped.");
      }
      Log->log("Destroying MQTT publisher...");
      delete publisher;
    }
  }

  bool start() {
    Log->log("Connecting to MQTT broker %s:%d...", host, port);
    int rc = connect(host, port, keepalive);
    switch (rc) {
    case MOSQ_ERR_SUCCESS:
      Log->log("Successfully connected to MQTT broker %s:%d.", host, port);
      connected = true;
      break;
    case MOSQ_ERR_INVAL:
      Log->error("ERROR MOSQ_ERR_INVAL on connecting to MQTT broker: %s", mosqpp::strerror(rc));
      return false;
    case MOSQ_ERR_ERRNO:
      char buffer[1024];
      const char* error_message;
      error_message = strerror_r(errno, buffer, 1024);
      Log->error("ERROR on connecting to MQTT broker: %s", rc, error_message);
      return false;
    default:
      Log->error("ERROR %d on connecting to MQTT broker: %s", rc, mosqpp::strerror(rc));
      return false;
    }

    loop_start();  // Start the thread that processes connection/publish/subscribe requests
    //Log->log("Connecting to MQTT broker %s:%d...", host, port);
    //connect_async(host, port, keepalive);  // non blocking connection to MQTT broker
    return true;
  }

  void stop(bool force) {
    if (connected) disconnect();
    loop_stop();             // Stop the thread
  }

  bool is_connected() {
    return connected;
  }

  bool publish_message(const char* topic, const char* message) {
    int error_code =
      publish(            // Publish the message.
        NULL,             // (output) Message Id (int *) this allow to latter get status of each message
        topic,            // topic of the message
        strlen(message),  // length of the payload (message)
        message,          // payload (the message)
        1,                // QOS (quality of service:
                          //  0: The broker/client will deliver the message once, with no confirmation.
                          //  1: The broker/client will deliver the message at least once, with confirmation required.
                          //  2: The broker/client will deliver the message exactly once by using a four step handshake.
        false             // retain (boolean) - indicates if message is retained on broker or not.
    );                    // Returns:
                          //   MOSQ_ERR_SUCCESS  on success.
                          //   MOSQ_ERR_INVAL  if the input parameters were invalid.
                          //   MOSQ_ERR_NOMEM  if an out of memory condition occurred.
                          //   MOSQ_ERR_NO_CONN  if the client is not connected to a broker.
                          //   MOSQ_ERR_PROTOCOL if there is a protocol error communicating with the broker.
                          //   MOSQ_ERR_PAYLOAD_SIZE if payload length is too large.
                          //   MOSQ_ERR_MALFORMED_UTF8 if the topic is not valid UTF-8
                          //   MOSQ_ERR_QOS_NOT_SUPPORTED  if the QoS is greater than that supported by the broker.
                          //   MOSQ_ERR_OVERSIZE_PACKET  if the resulting packet would be larger than supported by the broker.
    if (error_code == MOSQ_ERR_SUCCESS) return true;
    Log->error("ERROR %d on publishing MQTT message: %s", error_code, mosqpp::strerror(error_code));
    return false;
  }

private:
  void on_connect(int rc) {
    if (rc != MOSQ_ERR_SUCCESS ) {
      Log->error("ERROR %d on connecting to MQTT broker: %s", rc, mosqpp::strerror(rc));
    } else if ( (options&(VERBOSITY_INFO|VERBOSITY_DEBUG))!=0 ) {
      Log->log("Successfully connected to MQTT broker %s:%d.", host, port);
      connected = true;
    }
  }

  void on_disconnect(int rc) {
    connected = false;
    if (rc != MOSQ_ERR_SUCCESS ) {
      Log->error("Connection to MQTT broker %s:%d has been lost (error code %d).", host, port, rc);
    } else if ( (options&(VERBOSITY_INFO|VERBOSITY_DEBUG))!=0 ) {
      Log->log("Connection to MQTT broker %s:%d has been closed.", host, port);
    }
  }

  void on_publish(int mid) {
    if ( (options&VERBOSITY_DEBUG)!=0 ) {
      Log->error("Message has been sent to MQTT broker %s:%d.", host, port);
    }
  }

};


//-------------------------------------------------------------
// MQTT rule definition

class MqttRule : public AbstractRuleWithSchedule {
public:
  const char* mqttTopic;

  MqttRule(SensorDef* sensor_def, Metric metric, const char* mqttTopic) : AbstractRuleWithSchedule(sensor_def, metric) {
    this->mqttTopic = mqttTopic;
  }

  const char* getTypeName() {
    return "MQTT rule";
  }

  void execute(const char* message, class Config& cfg);

};

#endif /* MQTT_HPP_ */
