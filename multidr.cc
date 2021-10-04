#include <act/graph.h>

//This part works on the multi drivers
//either nested or neighboring
namespace fpga {

unsigned int extra_num = 1;

//Function to update connection map in 
//the process node after adding md mux
//instance
void update_cp (node *n, port *p) {
  std::vector<port *> new_ports;
  for (auto pp : n->cp[p->c]) {
    if (pp->dir == 1 || pp->owner == 0) {
      new_ports.push_back(pp);
    }
  }
  new_ports.push_back(p);
  new_ports.clear();
}

//Function to append md multiplexing node
//to the main circuit graph
void append_graph (graph *g, node *n) {
  if (g->hd) {
    g->tl->next = n;
    g->tl = n;
  } else {
    g->hd = n;
    g->hd->next = g->tl;
    g->tl = n;
  }
}

void prepend_graph (graph *g, node *n) {
  if (g->hd) {
    n->next = g->hd;
    g->hd = n;
  } else {
    g->hd = n;
    g->hd->next = g->tl;
    g->tl = n;
  }
}

//Function to append md multiplexing node 
//to the list of instances in the node
void append_inst (node *n, inst_node *in) {
  if (n->cgh) {
    n->cgt->next = in;
    n->cgt = in;
  } else {
    n->cgh = in;
    n->cgh->next = n->cgt;
    n->cgt = in;
  }
  n->i_num++;
}

void append_gate (node *n, gate *g) {
  if (n->gh) {
    n->gt->next = g;
    n->gt = g;
  } else {
    n->gh = g;
    n->gh->next = n->gt;
    n->gt = g;
  }
}

port *copy_port (port *p) {
  port *cp = new port;
  cp->c = p->c;
  cp->visited = p->visited;
  cp->disable = 0;
  cp->dir = p->dir; 
  cp->drive_type = p->drive_type;
  cp->delay = p->delay;
  cp->forced = p->forced;
	cp->primary = p->primary;
  return cp;
}

gate *copy_gate (gate *g) {
  gate *cg = new gate;
  cg->id = g->id;
  cg->type = g->type;
  cg->drive_type = g->drive_type;
  cg->visited = g->visited;
  cg->extra_gate = g->extra_gate;
  cg->is_weak = g->is_weak;
  for (auto pp : g->p) {
    port *cp;
    cp = copy_port(pp);
    cp->owner = 2;
    cp->u.g.g = cg;
    cg->p.push_back(cp);
  }
  for (auto netw : g->p_up) {
    cg->p_up.push_back(netw);
  }
  for (auto netw : g->p_dn) {
    cg->p_dn.push_back(netw);
  }
  for (auto netw : g->w_p_up) {
    cg->w_p_up.push_back(netw);
  }
  for (auto netw : g->w_p_dn) {
    cg->w_p_dn.push_back(netw);
  }
  cg->next = NULL;
  return cg;  
}

inst_node *copy_inst(inst_node *in) {
	inst_node *cin = new inst_node;
	cin->proc = in->proc;
	cin->n = in->n;
	cin->par = NULL;
	for (auto pp : in->p) {
		port *cp;
		cp = copy_port(pp);
		if (pp->owner == 0) {
			cp->owner = 0;
			cp->u.p.n = pp->u.p.n;
		} else if (pp->owner == 1) {
			cp->owner = 1;
			cp->u.i.in = cin;	
		}
		cin->p.push_back(cp);
	}
	cin->inst_name = in->inst_name;
	cin->array = in->array;
	cin->visited = in->visited;
	cin->extra_inst = in->extra_inst;
	cin->next = NULL;
	return cin;
}

//Function to copy process node
//and modify its ports because 
//not every instance of the process
//is a part of the multi driving
//Copy only printable part because
//the rest of the structure is for 
//computation
node *copy_node (node *n){
  node *cn = new node;
  if (n->proc) {
    cn->proc = n->proc;
  } else {
    fatal_error("You should not copy this bro");
  }
  cn->visited = 1;
  cn->extra_node = extra_num;
  ++extra_num;
  cn->copy = 1;
  cn->weight = 0;
  for (auto pp : n->p) {
    port *cpp;
    cpp = copy_port(pp);
    cpp->owner = 0;
    cpp->u.p.n = cn;
    cn->p.push_back(cpp);
    cn->cp[cpp->c].push_back(cpp);
  }
  for (auto gp : n->gp) {
    port *cgpp;
    cgpp = copy_port(gp);
    cgpp->owner = 0;
    cgpp->u.p.n = cn;
    cn->gp.push_back(cgpp);
    cn->cp[cgpp->c].push_back(cgpp);
  }
  
  cn->g_num = n->g_num;
  cn->gh = NULL;
  cn->gt = NULL;
  for (auto gn = n->gh; gn; gn = gn->next) {
    gate *cg;
    cg = copy_gate(gn);
    append_gate(cn, cg);
    for (auto pp : cg->p) {
      cn->cp[pp->c].push_back(pp);
    }
  }
  cn->i_num = 0;
  cn->cgh = NULL;
  cn->cgt = NULL;
  for (auto in = n->cgh; in; in = in->next) {
    inst_node *cin = new inst_node;
		if (in->extra_inst != 0) {
			cin = in;
		} else {
	    cin = copy_inst(in);
		}
    cin->par = cn;
    append_inst (cn, cin);
    for (auto pp : cin->p) {
      cn->cp[pp->c].push_back(pp);
    }
  }
  cn->spec = n->spec;
  cn->next = NULL;
  return cn;
}

//Function to create one gate to
//manage multi drivers inside the
//extra node
gate *create_extra_gate (std::vector<port *> &p) {
  gate *eg = new gate;
  eg->type = 0; 
  eg->drive_type = 2;
  eg->visited = 1;
  eg->extra_gate = extra_num;
  eg->is_weak = 0;
  ActId *id;
  for (auto pp : p) {
    port *gp = new port;
    gp->c = pp->c;
    gp->visited = 1;
    gp->disable = 0;
    if (pp->dir == 0) {
      id = pp->c->toid();
      eg->id = id;
    }
    gp->dir = pp->dir;
    gp->drive_type = 0;
    gp->delay = pp->delay;
    gp->forced = pp->forced;
    gp->owner = 2;
		gp->primary = 1;
    gp->u.g.g = eg;
    if (gp->dir == 1) {
      eg->extra_drivers.push_back(gp);
    }
  }
  eg->next = NULL;
  return eg;
}

//Function to create node to 
//manage multi drivers
//proc = NULL
//extra = 1
//only one gate
node *create_extra_node (std::vector<port *> &p) {
  std::vector<port *> new_ports;
	int o_num = 0;
  node *pn = new node;
  pn->proc = NULL;
  pn->visited = 1;
  pn->weight = 0;
  pn->extra_node = extra_num;
	pn->copy = 0;
  for (auto pp : p) {
    port *ep = new port;
    ep->c = pp->c;
    ep->visited = 1;
    ep->disable = 0;
    if (pp->dir == 0 && pp->owner != 0) {
      ep->dir = 1;
      ep->drive_type = 1;
    } else {
			o_num++;
      ep->dir = 0;
      ep->drive_type = 0;
    }
    ep->delay = pp->delay;
    ep->forced = pp->forced;
    ep->owner = 0;
		ep->primary = pp->primary;
    ep->u.p.n = pn;
    pn->p.push_back(ep);
    new_ports.push_back(ep);
  }
	if (o_num == 0) {
		port *ep = new port;
	  ep->c = p[0]->c;
    ep->visited = 1;
    ep->disable = 0;
    ep->dir = 0;
    ep->drive_type = 0;
    ep->delay = p[0]->delay;
    ep->forced = p[0]->forced;
    ep->owner = 0;
		ep->primary = 0;
    ep->u.p.n = pn;
    pn->p.push_back(ep);
    new_ports.push_back(ep);
	}
  pn->spec = NULL;
  pn->i_num = 0;
  pn->cgh = NULL;
  pn->cgt = NULL;
  pn->g_num = 1;
  pn->gh = NULL;
  pn->gt = NULL;
  pn->next = NULL;
  gate *g = create_extra_gate(new_ports);
  append_gate(pn, g);
  return pn;
}

//Function to create instances of 
//the extra nodes
inst_node *create_extra_inst (node *pn, node *n, std::vector<port *> &p) {
	int o_num = 0;
  inst_node *ein = new inst_node;
  ein->proc = NULL;
  ein->par = pn;
  ein->n = n;
  for (auto pp : p) {
    port *eip = new port;
    eip->c = pp->c;
    eip->visited = 1;
    eip->disable = 0;
    if (pp->dir == 0 && pp->owner != 0) {
      eip->dir = 1;
      eip->drive_type = 1;
    } else {
			o_num++;
      eip->dir = 0;
      eip->drive_type = 0;
      update_cp(pn, eip);
    }
    eip->delay = 0;
    eip->forced = 0;
    eip->owner = pp->owner;
		eip->primary = 0;
		if (pp->owner == 1) {
      eip->u.i.in = pp->u.i.in;
    } else if (pp->owner == 2) {
      eip->u.g.g = pp->u.g.g;
    } else {
      eip->u.p.n = pp->u.p.n;
    }
    ein->p.push_back(eip);
  }
	if (o_num == 0) {
    port *eip = new port;
    eip->c = p[0]->c;
    eip->visited = 1;
    eip->disable = 0;
    eip->dir = 0;
    eip->drive_type = 0;
    update_cp(pn, eip);
    eip->delay = 0;
    eip->forced = 0;
    eip->owner = p[0]->owner;
		eip->primary = 0;
    if (p[0]->owner == 1) {
      eip->u.i.in = p[0]->u.i.in;
    } else if (p[0]->owner == 2) {
      eip->u.g.g = p[0]->u.g.g;
    } else {
      eip->u.p.n = p[0]->u.p.n;
    }
    ein->p.push_back(eip);
	}
  ein->inst_name = NULL;
  ein->array = NULL;
  ein->visited = 0;
  ein->extra_inst = extra_num;
  ein->next = NULL;
  return ein;
}

//Function to add mutiplexer includes
//creation and copying of all necessary
//elements
void create_mux (graph *g, node *n, std::vector<port *> &p) {
  node *en;
  inst_node *ein;
  en = create_extra_node(p);
  ein = create_extra_inst(n, en, p);
  append_inst(n, ein);
  prepend_graph(g, en);
  ++extra_num;
}

//Function to find gate while traversing
//down in the hierarchy
void find_gate (graph *g, inst_node *in, port *p){
  if (in->n->copy == 0) {
    node *cn;
    cn = copy_node(in->n);
    prepend_graph(g, cn);
    in->n = cn;
  }
  int iport = 0;
  for (auto pp : in->p) {
    if (pp == p) {
      break;
    }
    iport++;
  }
  port *ip;
  ip = in->n->p[iport];
  int ic = 0;
  port *op;
  std::vector<port *> npc;
	int primary_flag = 0;
  for (auto pp : in->n->cp[ip->c]){
    if (pp->owner != 0) {
      npc.push_back(pp);
    }
    if (pp->dir == 0 && pp->owner != 0) {
      if (pp->owner == 1 && pp->u.i.in->extra_inst != 0) {
        continue;
      } else if (pp->owner == 2) {
        op = pp;
      } else {
        op = pp;
      }
    } else {
			if (pp->dir == 0 && pp->owner == 0) {
				primary_flag = 1;
			}
		}
    if (pp->dir == 1) {
      ic++;
    }
  }
  if (op->owner == 0) {
    fatal_error("this shouldn't have happened");
  } else if (op->owner == 1) {
    unsigned int old_cnt = 0;
    unsigned int new_cnt = 0;
		if (primary_flag == 1) {
			op->primary = 1;
		}
    ip->drive_type = 1;
    op->drive_type = 1;
    old_cnt = op->u.i.in->p.size();
    find_gate(g, op->u.i.in, op);
    new_cnt = op->u.i.in->p.size();
    if (old_cnt < new_cnt) {
      port *cpp;
      port *cip;
      cpp = copy_port(op);
      cip = copy_port(in->p[iport]);
      cpp->drive_type = 0;
      cip->drive_type = 0;
      cpp->dir = 1;
      cip->dir = 1;
      in->n->p.push_back(cpp);
      in->p.push_back(cip);
    } else if (ic >= 1) {
      create_mux(g, in->n, npc);
    } 
  } else if (op->owner == 2) {
		if (primary_flag == 1){
			op->primary = 1;
		}
    ip->drive_type = 1;
    op->drive_type = 1;
    op->u.g.g->drive_type = 1;
    if (ic >= 1) {
      port *cpp;
      port *cip;
      cpp = copy_port(op);
      cip = copy_port(in->p[iport]); 
      cpp->drive_type = 0;
      cip->drive_type = 0;
      cpp->dir = 1;
      cip->dir = 1;
      in->n->p.push_back(cpp);
      in->p.push_back(cip);
    }
  }
  ic = 0;
}

//Function to find multi drivers
void find_drivers (graph *g, node *n){
  for (auto pair : n->cp) {
		//multi driver case
    if (pair.second.size() >= 2) {
      int o_num = 0;
      for (auto pp : pair.second) {
        if (pp->dir == 0 && pp->owner != 0) {
          o_num++;
        }
      }
      if (o_num > 1) {
				int i_flag = 0;
        node *en;
        inst_node *ein;
        std::vector<port *> port_collection;
        for (auto pp : pair.second) {
					if (pp->dir == 1) {
						if (i_flag == 0) {
							i_flag = 1;
						} else {
							continue;
						}
					}
          port_collection.push_back(pp);
        }
        create_mux(g, n, port_collection);
        for (auto pp : pair.second) {
          if (pp->dir == 0) {
            if (pp->owner == 0) {
              pp->drive_type = 1;
            } else if (pp->owner == 1) {
              pp->drive_type = 1;
              find_gate(g, pp->u.i.in, pp);
            } else if (pp->owner == 2) {
              pp->drive_type = 1;
              pp->u.g.g->drive_type = 1;
            }
          }
        }
      }
    }
  }
};

void add_md (project *p) {

  graph *g = p->g;

  std::vector <node *> graph_copy;

  for (auto n = g->hd; n; n = n->next) {
    graph_copy.push_back(n);
  }

  for (auto nn : graph_copy){
    find_drivers(g, nn);
  }

}

}
