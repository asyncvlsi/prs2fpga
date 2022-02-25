/*******************************************************************************
 *
 * Copyright (c) 2022 Ruslan Dashkin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/

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
