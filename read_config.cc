#include <string.h>
#include <act/fpga_config.h>
#include <act/act.h>

namespace fpga {

fpga_config *read_config (FILE *conf_file, int func){
  fpga_config *fc = new fpga_config;

  if (func == 0) {

    fc->vendor = 0;
    fc->chip = "XC7VX690T-3FFG1761C";
    fc->num = 1;
    fc->lut = 433200;
    fc->ff = 866400;
    fc->dsp = 3600;
    fc->freq = 500;
    fc->pins = 850;
    fc->intf = 0;
    fc->inct.push_back("");
    fc->incx.push_back("");
    fc->print = 1;
    fc->opt = 0;

  } else if (func == 1) {

    char buf[1024];
    char *val;
    unsigned int int_val = 0;

    while (1){
      fgets(buf, 1024, conf_file);
      if (feof(conf_file)) {
        break;
      }
      val = strtok(buf, "=");
      while (val) {
        if (strcmp(val, "VENDOR") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->vendor = int_val;
        } else if (strcmp(val, "CHIP") == 0) {
          val = strtok(NULL, "=");
          val[strcspn(val, "\n")] = 0;
          fc->chip = val;
        } else if (strcmp(val, "LUT") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->lut = int_val;
        } else if (strcmp(val, "FF") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->ff = int_val;
        } else if (strcmp(val, "DSP") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->dsp = int_val;
        } else if (strcmp(val, "NUM") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->num = int_val;
        } else if (strcmp(val, "FREQ") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->freq = int_val;
        } else if (strcmp(val, "PINS") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->pins = int_val;
        } else if (strcmp(val, "INTERFACE") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->intf = int_val;
        } else if (strcmp(val, "INCLUDE_TCL") == 0) {
          val = strtok(NULL, "=");
          val[strcspn(val, "\n")] = 0;
          fc->inct.push_back(val);
        } else if (strcmp(val, "INCLUDE_XDC") == 0) {
          val = strtok(NULL, "=");
          val[strcspn(val, "\n")] = 0;
          fc->incx.push_back(val);
        } else if (strcmp(val, "OPTIMIZATION") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->opt = int_val;
        } else if (strcmp(val, "VERILOG") == 0) {
          val = strtok(NULL, "=");
          int_val = atoi(val);
          fc->print = int_val;
        } else {
          fatal_error("Unknown config parameter");
        }
        val = strtok(NULL, "=");
      }
    }
  }

  return fc;

}

}
