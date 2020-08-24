#include <string>
#include <vector>

namespace fpga {

struct fpga_config {
  unsigned int vendor:2;          //0 - Xilinx; 1 - Intel; 2 - ?; 3 - ?

  std::string chip;               //chip id

  unsigned int num;               //number of available chips

  unsigned int lut;               //number of luts in the chip

  unsigned int ff;                //number of flip-flops in the chip

  unsigned int dsp;               //number of DSP blocks

  unsigned int freq;              //targeting clock frequency MHz

  unsigned int pins;              //number of io pins on the chip

  unsigned int intf:2;            //unterface:
                                  //0 - non
                                  //1 - TileLink
                                  //2 - AXI
                                  //3 - UART

  std::vector<std::string> inct;  //vector of strings with
                                  //paths to the included tcl files

  std::vector<std::string> incx;  //vector of strings with
                                  //paths to the included xdc files

  unsigned int print:1;           //print verilog
                                  //0 - NO
                                  //1 - YES

  unsigned int opt;               //0 - no ff/lut optimization
                                  //1 - basic ff/lut optimization
                                  //>1 - every Nth gate is ff
};

fpga_config *read_config (FILE *, int);

}
