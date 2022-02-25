#include <stdio.h>
#include <unistd.h>
#include <act/act.h>
#include <act/graph.h>
#include <act/passes/booleanize.h>
#include <act/passes/netlist.h>
#include <act/passes/cells.h>

#include "debug.h"

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
  fprintf(stdout, "Usage: prs2fpga [-he] [-On] [-p <process_name>] [-o <*.file>] <*.act>\n");
  fprintf(stdout, "-p - Specify process name;\n");
  fprintf(stdout, "-o - Save to the file(default stdout);\n");
  fprintf(stdout, "-O<0,1...> - C style optimization flag(0 default);\n");
  fprintf(stdout, "-c - Specify config file;\n");
  fprintf(stdout, "-e - Print an explicit delay(non-synthesizable/overwrites O option);\n");
  fprintf(stdout, "-h - Usage guide;\n\n");
  fprintf(stdout, "=============================================================================================\n");
}

int main (int argc, char **argv) { 

  Act *a = NULL;
  char *proc = NULL;

  Act::Init(&argc, &argv);
  config_set_int ("net.black_box_mode", 0);

  FILE *fout  = stdout;

  extern int opterr;
  opterr = 0;

  int print_option = 1;
  int opt_option = 0;
  int unit_option = 0;

  char *cell_file = NULL;

  int key = 0;
  while ((key = getopt (argc, argv, "c:p:ehm:o:t:O:P")) != -1) {
    switch (key) {
      case 'O':
        if (optarg == NULL) { fatal_error("Missing optimization level"); }
        opt_option = atoi(optarg);
        break;
      case 'P':
        print_option = 0;
        break;
      case 'o':
        fout  = fopen(optarg, "w");
        break;
      case 'e':
        unit_option = -1;
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
      case 'c':
        if (cell_file) {
          FREE(cell_file);
        }
        if (optarg == NULL) {
          fatal_error ("Missing cell file");
        }
        cell_file = Strdup(optarg);
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
 
  if (cell_file) {
    a->Merge (cell_file);
  }

  a = new Act(argv[optind]);
  a->Expand ();

  if (cell_file) {
    ActCellPass *cp = new ActCellPass (a);
    cp->run();

    FILE *oc = fopen (cell_file, "w");
    if (!oc) {
      fatal_error ("Could not write cell file");
    }
    cp->Print (oc);
    fclose (oc);
  }

	Process *p = a->findProcess (proc);

	if (!p) {
		fatal_error ("Wrong process name, %s", proc);
	}

	if (!p->isExpanded()){
		fatal_error ("Process '%s' is not expanded.", proc);
	}

  fpga::project *fp = new fpga::project;
	fpga::graph *fg;
  fpga::config *fc = new fpga::config;
  
  fp->g = fg;

	ActBooleanizePass *BOOL = new ActBooleanizePass (a);
	ActNetlistPass *NETL = new ActNetlistPass (a);

	Assert (BOOL->run (p), "Booleanize pass failed");
  Assert (NETL->run(p) , "Netlist pass failed");

  if (opt_option != 0) {
    fc->opt = opt_option;
  } else {
    fc->opt = 0;
  }
  if (unit_option == -1) {
    fc->opt = -1;
  }
  fc->print = print_option;
 
  fp->c = fc;

  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tBUILDING VERILOG PROJECT...\n");
	fpga::build_project_graph(fp,a,p);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tPLACING EXCLUSION MODULES...\n");
  fpga::add_arb(fp);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tSATISFYING TIMING CONSTRAINTS...\n");
  fpga::add_timing(fp);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  fprintf(stdout, "==========================================\n");
  fprintf(stdout, "\tHANDLING MULTI DRIVERS...\n");
  fpga::add_md(fp);
  fprintf(stdout, "------------------------------------------\n");
  fprintf(stdout, "\tDONE\n");
  if (fc->print == 1) {
    fprintf(stdout, "==========================================\n");
    fprintf(stdout, "\tPRINTING VERILOG...\n");
    fpga::print_verilog(fp, fout);
    fprintf(stdout, "------------------------------------------\n");
    fprintf(stdout, "\tDONE\n");
  }
  fprintf(stdout, "==========================================\n");
  return 0;
}
