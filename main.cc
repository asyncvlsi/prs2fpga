#include <stdio.h>
#include <unistd.h>
#include <act/act.h>
#include <act/fpga_proto.h>
#include <act/passes/booleanize.h>
#include <act/passes/netlist.h>

void logo () {

fprintf(stdout, "    +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +    \n");
fprintf(stdout, "  +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |                        |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |       PRS -> FPGA      |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+     Yale University    +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |        AVLSI Lab       |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+     Ruslan Dashkin     +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |      Rajit Manohar     |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  |                        |    |                        |    |                        |  \n");
fprintf(stdout, "+-+                        +-++-+                        +-++-+                        +-+\n");
fprintf(stdout, "  +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+    +-+--+--+--+--+--+--+--+-+  \n");
fprintf(stdout, "    +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +        +  +  +  +  +  +  +  +    \n");

}

void usage () {
  fprintf(stdout, "=============================================================================================\n");
  logo();
  fprintf(stdout, "=============================================================================================\n\n");
  fprintf(stdout, "Usage: fpga_proto [-h] [-p <process_name>] [-o <*.file>] [-c <*.conf>] <*.act>\n");
  fprintf(stdout, "-p - Specify process name;\n");
  fprintf(stdout, "-o - Save to the file(default stdout);\n");
  fprintf(stdout, "-c - Specify config file;\n");
  fprintf(stdout, "-h - Usage guide;\n\n");
  fprintf(stdout, "=============================================================================================\n");
  fprintf(stdout, "Configuration file description:\n");
  fprintf(stdout, "VENDOR - 0 - Xilinx; 1 - Intel(Altera)\n");
  fprintf(stdout, "CHIP - full chip name: Family, Package, Temp.grade, Speed grade etc.\n");
  fprintf(stdout, "LUT - number of luts\n");
  fprintf(stdout, "FF - number of flip-flops\n");
  fprintf(stdout, "DSP - number of dsp blocks\n");
  fprintf(stdout, "NUM - number of available chips\n");
  fprintf(stdout, "FREQ - targeting frequency\n");
  fprintf(stdout, "PINS - number of available pins on the chip\n");
  fprintf(stdout, "INTERFACE - 0 - NON; 1 - TileLink; 2 - AXI; 3 - UART\n");
  fprintf(stdout, "INCLUDE_TCL - paths to additional tcl scripts\n");
  fprintf(stdout, "INCLUDE_XDC - paths to additional xdc files\n");
  fprintf(stdout, "OPTIMIZATION - 0 - ff per gate; 1 - reduced number of ffs\n");
  fprintf(stdout, "VERILOG - 0 - no print; 1 - print verilog\n");
  fprintf(stdout, "DUT - 0 no dut template; 1 - create dut template\n");
  fprintf(stdout, "=============================================================================================\n");
}

int main (int argc, char **argv) { 

  Act *a = NULL;
  char *proc = NULL;

  Act::Init(&argc, &argv);

  int print_or_not_to_print = 0; //that is the question
  int how_to_print = 0;
  int where_to_print = 0;
  FILE *fout  = stdout;
  char *conf = NULL;

  int key = 0;

  extern int opterr;
  opterr = 0;

  while ((key = getopt (argc, argv, "p:hmc:o:t:")) != -1) {
    switch (key) {
      case 'c':
        conf = optarg;
        break;
      case 'o':
        fout  = fopen(optarg, "w");
        break;
      case 'h':
        usage();
        return 0;
        break;
      case 'p':
        if (proc) {
          FREE(proc);
        }
        if (optarg == NULL) {
          fatal_error ("Missing process name");
        }
        proc = Strdup(optarg);
        break;
      case ':':
        fprintf(stderr, "Need a file here\n");
        break;
      case '?':
        usage();
        fatal_error("Something went wrong\n");
        break;
      default: 
        fatal_error("Hmm...\n");
        break;
    }
  }

  if (optind != argc - 1) {
    fatal_error("Missing act file name\n");
  }

  if (proc == NULL) {
    fatal_error("Missing process name\n");
  }
 
  a = new Act(argv[optind]);
  a->Expand ();

	Process *p = a->findProcess (proc);

	if (!p) {
		fatal_error ("Wrong process name, %s", proc);
	}

	if (!p->isExpanded()){
		fatal_error ("Process '%s' is not expanded.", proc);
	}

	fpga::graph *fg;

	ActBooleanizePass *BOOL = new ActBooleanizePass (a);
	ActNetlistPass *NETL = new ActNetlistPass (a);

	Assert (BOOL->run (p), "Booleanize pass failed");
  Assert (NETL->run(p) , "Netlist pass failed");

  fpga::fpga_config *fc;
  FILE *conf_file;
  conf_file = fopen(conf, "r");
  if (conf_file == 0) {
    fc = fpga::read_config(conf_file, 0);
  } else {
    fc = fpga::read_config(conf_file, 1);
    fclose(conf_file);
  }

  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tBUILDING VERILOG PROJECT...\n");
	fg = fpga::create_fpga_project(a,p);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tPLACING EXCLUSION MODULES...\n");
  fpga::add_arb(fg);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tSATISFYING TIMING CONSTRAINTS...\n");
  if (fc->opt == 0) {
    fpga::add_timing(fg, fc->opt);
  } else {
    fpga::add_timing(fg, fc->opt);
  }
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tHANDLING MULTI DRIVERS...\n");
  fpga::add_md(fg);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  if (fc->print == 1) {
    fprintf(stdout, "==========================================\n");
    fprintf(stdout, "\tPRINTING VERILOG...\n");
    fpga::print_verilog(fg, fout);
    fprintf(stdout, "------------------------------------------\n");
    fprintf(stdout, "\tDONE\n");
  }
  fprintf(stdout, "==========================================\n");
  return 0;
}
