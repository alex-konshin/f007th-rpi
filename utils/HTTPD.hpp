/*
 * HTTPD.hpp
 *
 *  Created on: December 2, 2020
 *      Author: Alex Konshin <akonshin@gmail.com>
 */

#ifndef HTTPD_HPP_
#define HTTPD_HPP_

#include "../common/SensorsData.hpp"
#include "../common/Config.hpp"

struct MHD_Daemon* start_httpd(int port, SensorsData* sensorsData, Config* cfg);
void stop_httpd(struct MHD_Daemon* httpd);

class HTTPD {
public:
  SensorsData* sensorsData;
  Config* cfg;
  int port;
  struct MHD_Daemon* daemon;
  bool no_home_page = false;

  HTTPD(SensorsData* sensorsData, Config* cfg);
  ~HTTPD();

  void start();
  void stop();

  static HTTPD* start(SensorsData* sensorsData, Config* cfg);
  static void destroy(HTTPD*& httpd);
};

#endif /* HTTPD_HPP_ */
