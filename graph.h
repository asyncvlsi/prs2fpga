#include <vector>
#include <map>
#include <utility>
#include <act/act.h>

namespace fpga {

struct node;
struct inst_node;
struct gate;
struct graph;

struct port {
  act_connection *c;          //connection pointer of the port

  unsigned int visited:1;
  unsigned int disable:1;     //not used yet...

  unsigned int dir:1;         //0 - output; 1 - input
  unsigned int drive_type:2;  //0 - orig; 1 - master; 2 - slave
  unsigned int delay;         //0 - orig; N - number of FFs to delay

  unsigned int forced:2;      //0 - no; 1 - mk_hi; 2 - mk_lo;

  unsigned int owner:2;       //0 - proc; 1 - inst node; 2 - gate
  
  unsigned int primary:1;     //0 - interconnection
                              //1 - directly connected to the parent port
  union {
    struct {
      node *n;                //process node owner
    } p;
    struct {
      inst_node *in;          //instance node owner
    } i;
    struct {
      gate *g;                //gate owner
    } g;
  } u;
};

struct inst_node {
  Process *proc;          //need this to map to the orig node
  node *n;                //pointer to the original node
  node *par;              //pointer to the parent original node
  std::vector<port *> p;  //instance ports

  ValueIdx *inst_name;    //name of the instance
  char *array;            //if it is an array of instances this will hold an index
  
  //std::map<int, std::vector<int> > io_map;  //map input ports to all outputs
                                            //(copy from the original node)

  unsigned int visited:2;

  unsigned int extra_inst;    //0 - not used;
                              //!0 - inst didn't exist in the original circuit
 
  inst_node *next;        //next instance of the same process
};

struct gate {
  ActId *id;                            //id

  unsigned int type:2;                  //0 - comb;
                                        //1 - seq; 
                                        //2 - state-holding short;
                                        //3 - state-holding long;

  unsigned int drive_type:3;            //0 - orig; 1 - md master;
                                        //          2 - multiplexer;

  unsigned int is_weak:1;               //0 - regular; 1 - weak;

  unsigned int visited:2;

  unsigned int extra_gate;

  std::vector<port *> p;                //ports
  
  std::map<int, int> io_map;            //map input to outputs

  std::vector<act_prs_lang_t *> p_up;   //pull up networks (all of them)
  std::vector<act_prs_lang_t *> w_p_up; //weak pull up networks (all of them)
  std::vector<act_prs_lang_t *> p_dn;   //pull dn networks (all of them) 
  std::vector<act_prs_lang_t *> w_p_dn; //weak pull dn networks (all of them)

  std::vector<port *> extra_drivers;    //used only in the multiplexer

  gate *next;
};

struct node {
  Process *proc;            //original process

  unsigned int visited:2;     //0 - no; 1 - yes
  unsigned int extra_node;    //0 - not used; 
                              //1 - node didn't exist in the original circuit
  unsigned int copy:1;        //0 - original node; 1 - modified copy

  unsigned int weight;        //node weight for area estimation

  std::vector<port *> p;    //local ports
  std::vector<port *> gp;   //global ports
  
  act_spec *spec;           //pointer to spec if any

  int i_num;                //number of process instances
  inst_node *cgh, *cgt;     //child graph of unique nodes and gates
  int g_num;                //number of gates in the process
  gate *gh, *gt;            //list of gates if any

  std::map<int, std::vector<port *> > excl; //map of groups of signals
                                            //to be exclusive. 
                                            //0 - mk_excllo; 1 - mk_exclhi

  //maps to simplify graph traversal
  std::map<act_connection *, std::vector<port *> > cp; //map connections2ports

  std::map<int, std::vector<int> > io_map;          //map input ports to all outputs
  std::map<std::pair<int, int>, int> max_io_delay;  //max #input to #output delay
  std::map<std::pair<int, int>, int> min_io_delay;  //min #input to #output delay

  std::vector<inst_node *> iv;  //vector to keep pointer to all instances of this node

  node *next;
};


struct graph {
  node *hd, *tl;
};

//fpga_config *read_config(FILE *, int);
int cmp_owner(port *, port *);
graph *create_fpga_project (Act *, Process *);
void add_arb (graph *);
void add_timing (graph *, int);
void add_md (graph *);
void print_verilog (graph *, FILE *);
//void print_inst (node *, fpga_config *);
}

#include "fpga_config.h"
#include "debug.h"


