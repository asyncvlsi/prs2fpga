#include "graph.h"

namespace fpga {

port::port(){
  c = NULL;
  visited = 0;
  disable = 0;
  dir = 0;
  bi = 0;
  drive_type = 0;
  delay = 0;
  forced = 0;
  owner = 0;
  primary = 0;
  wire = 0;
  u.p.n = NULL;
}

port::~port(){}

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
}
