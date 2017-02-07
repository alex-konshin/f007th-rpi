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

  const bool format_json = false;
  const bool write_undecoded_to_file = false;

  int verbosity = VERBOSITY_INFO | VERBOSITY_PRINT_UNDECODED;

  FILE* fp = NULL;

  RFReceiver receiver(gpio);
  ReceivedMessage message;

  if (write_undecoded_to_file) fp = fopen("f007th_data.txt", "w+");

  receiver.enableReceive();

  if ((verbosity&VERBOSITY_PRINT_STATISTICS) != 0)
    receiver.printStatisticsPeriodically(1000); // print statistics every second

  if ((verbosity&VERBOSITY_INFO) != 0) fputs("Receiving data...\n", stderr);
  while(!receiver.isStopped()) {

    if (receiver.waitForMessage(message)) {
      if (receiver.isStopped()) break;

      if (format_json) {
        message.json(stdout, true);
      } else {
        message.print(stdout, verbosity);
      }

      if (write_undecoded_to_file && !message.isEmpty() && message.isUndecoded()) {
        if (message.print(fp, VERBOSITY_PRINT_UNDECODED | VERBOSITY_PRINT_DETAILS)) fflush(fp);
      }
    }

    if (receiver.checkAndResetTimerEvent()) {
      receiver.printStatistics();
    }
  }
  if ((verbosity&VERBOSITY_INFO) != 0) fputs("\nExiting...\n", stderr);

  if (write_undecoded_to_file) fclose(fp);

  exit(0);
}




