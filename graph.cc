#include "graph.h"

namespace fpga {

/*
  Port class
*/
port::port(){
  c = NULL;
  visited = 0;
  disable = 0;
  dir = 0;
  bi = 0;
  drive_type = 0;
  delay = 0;
  forced = 0;
  extra_owner = 0;
  extra_dir = 0;
  owner = 0;
  primary = 0;
  wire = 0;
  e = NULL;
  u.p.n = NULL;
}

port::~port(){}

void port::setExtraOwner(inst_node *in)
{
  extra_owner = 1;
  e = in;
}

void port::setExtraDir(int i)
{
  extra_dir = i;
}

void port::setOwner(node *n)
{
  owner = 0;
  u.p.n = n;
}

void port::setOwner(inst_node *in)
{
  owner = 1;
  u.i.in = in;
}

void port::setOwner(gate *g)
{
  owner = 2;
  u.g.g = g;
}

/*
  Instance node class
*/

inst_node::inst_node()
{
  proc = NULL;
  n = NULL;
  par = NULL;
  inst_name = NULL;
  array = NULL;
  visited = 0;
  extra_inst = 0;
  next = NULL;  
}

inst_node::~inst_node(){}

}
