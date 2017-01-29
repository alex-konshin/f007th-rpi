/*
 * Ambient Weather F007TH sensor listener
 *
 * Usage: sudo F007TH
 *
 * Created on January 25, 2017
 * @author Alex Konshin <akonshin@gmail.com>
 */

#include "RFReceiver.hpp"


int main(int argc, char *argv[]) {
  const int gpio = 27;

  const bool format_json = true;
  const bool print_details = false;
  const bool print_undecoded = true;
  const bool write_undecoded_to_file = false;
  const bool print_statistics = false;

  FILE* fp = NULL;

  RFReceiver receiver(gpio);
  ReceivedMessage message;

  if (write_undecoded_to_file) fp = fopen("f007th_data.txt", "w+");

  receiver.enableReceive();

  if (print_statistics) receiver.printStatisticsPeriodically(1000); // print statistics every second

  if (print_details) puts("receiving data...");
  while(1) {

    if (receiver.waitForMessage(message)) {

      if (format_json) {
        message.json(stdout, false);
      } else {
        message.print(stdout, print_details, print_undecoded);
      }

      if (write_undecoded_to_file && !message.isEmpty() && message.isUndecoded()) {
        if (message.print(fp, true, true)) fflush(fp);
      }
    }

    if (receiver.checkAndResetTimerEvent()) {
      receiver.printStatistics();
    }
  }

  if (write_undecoded_to_file) fclose(fp);

  exit(0);
}




