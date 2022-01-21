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

  return;
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

  return;
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

  return;
}

void update_graph (graph *g, node *n)
{
  for (auto nn = g->hd; nn; nn = nn->next) {
    if (nn->extra_node != 0) { continue; }
    if (nn->proc == n->proc) {
      n->next = nn->next;
      nn = n;
    }
  }

  return;
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

  return;
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

  return;
}

port *copy_port (port *p) {
  port *cp = new port;
  cp->c = p->c;
  cp->visited = p->visited;
  cp->disable = 0;
  cp->dir = p->dir; 
  cp->bi = p->bi;
  cp->drive_type = p->drive_type;
  cp->delay = p->delay;
  cp->forced = p->forced;
  cp->primary = p->primary;
  cp->wire = p->wire;
  cp->extra_owner = p->extra_owner;
  cp->e = p->e;
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
  for (auto pp : g->extra_drivers) {
    port *cp;
    cp = copy_port(pp);
    cp->owner = 2;
    cp->u.g.g = cg;
    cg->extra_drivers.push_back(cp);   
  }
  for (auto netw : g->p_up) { cg->p_up.push_back(netw); }
  for (auto netw : g->p_dn) { cg->p_dn.push_back(netw); }
  for (auto netw : g->w_p_up) { cg->w_p_up.push_back(netw); }
  for (auto netw : g->w_p_dn) { cg->w_p_dn.push_back(netw); }
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
    if (pp->owner == 0) { cp->setOwner(pp->u.p.n); } 
    else if (pp->owner == 1) { cp->setOwner(pp->u.i.in); } 
    else { cp->setOwner(pp->u.g.g); }
    cin->p.push_back(cp);
  }
  cin->inst_name = in->inst_name;
  cin->array = in->array;
  cin->visited = in->visited;
  cin->extra_inst = in->extra_inst;
  cin->next = NULL;
  return cin;
}

//Function to copy process node and modify its ports because 
//not every instance of the process is a part of the multi driver
node *copy_node (node *n) {
  node *cn = new node;
  if (n->proc) { cn->proc = n->proc; } 
  else { cn->proc = NULL; }
  cn->visited = 1;
  cn->extra_node = extra_num;
  extra_num++;
  cn->copy = 1;
  cn->weight = 0;
  for (auto pp : n->p) {
    port *cpp;
    cpp = copy_port(pp);
    if (pp->owner == 0) { cpp->setOwner(cn); } 
    else if (pp->owner == 1) { cpp->setOwner(pp->u.i.in); } 
    else { cpp->setOwner(pp->u.g.g); }
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
    for (auto pp : cg->p) { cn->cp[pp->c].push_back(pp); }
  }
  cn->i_num = n->i_num;
  cn->cgh = NULL;
  cn->cgt = NULL;
  for (auto in = n->cgh; in; in = in->next) {
    inst_node *cin = new inst_node;
    cin = copy_inst(in);
    cin->par = cn;
    append_inst (cn, cin);
    for (auto pp : cin->p) { 
      if (pp->e) { pp->e = cin; }
      cn->cp[pp->c].push_back(pp); 
    }
  }
  cn->spec = n->spec;
  cn->decl.insert(n->decl.begin(), n->decl.end());
  cn->next = NULL;
  return cn;
}

//Function to create one gate to
//manage multi drivers inside the
//extra node
gate *create_extra_gate (port *ip, std::vector<port *> &p, int type) {
  gate *eg = new gate;
  eg->type = type; 
  eg->visited = 1;
  eg->extra_gate = extra_num;
  eg->is_weak = 0;
  eg->drive_type = 2;

  ActId *id;

  port *gop = new port; 
  gop->c = ip->c;
  gop->visited = 1;
  gop->disable = 0;
  id = ip->c->toid();

  eg->id = id;
  gop->dir = 0;
  gop->bi = ip->bi;
  gop->drive_type = ip->drive_type;
  gop->delay = ip->delay;
  gop->forced = ip->forced;
  gop->extra_owner = 0;
  gop->owner = 2;
  gop->primary = 1;
  gop->u.g.g = eg;
  eg->extra_drivers.push_back(gop);

  for (auto pp : p) {
    if (pp->dir == 0) { continue; }
    port *gp = new port;
    gp->c = pp->c;
    gp->visited = 1;
    gp->disable = 0;
    gp->dir = 1;
    gp->bi = pp->bi;
    gp->drive_type = 0;
    gp->delay = pp->delay;
    gp->forced = pp->forced;
    gp->extra_owner = 0;
    gp->owner = 2;
    gp->primary = 1;
    gp->u.g.g = eg;
    eg->extra_drivers.push_back(gp);
  }
  eg->next = NULL;
  return eg;
}

//Function to create a node to manage multi drivers
//i.e. multiplexor
//proc = NULL, extra = 1, only one gate
node *create_extra_node (port *ip, std::vector<port *> &p, int type) {
  std::vector<port *> new_ports;
  int o_num = 0;
  node *pn = new node;
  pn->proc = NULL;
  pn->visited = 1;
  pn->weight = 0;
  pn->extra_node = extra_num;
  pn->copy = 0;

  port *ep;
  ep = new port;

  ep->c = ip->c;
  ep->visited = 1;
  ep->disable = 0;
  ep->dir = 0;
  ep->bi = ip->bi;
  ep->drive_type = ip->drive_type;
  ep->delay = ip->delay;
  ep->forced = ip->forced;
  ep->extra_owner = 0;
  ep->owner = 0;
  ep->primary = 0;
  ep->u.p.n = pn;
  pn->p.push_back(ep);
  new_ports.push_back(ep);
  pn->cp[ip->c].push_back(ep);

  for (auto pp : p) {
    ep = new port;
    ep->c = pp->c;
    ep->visited = 1;
    ep->disable = 0;
    ep->bi = pp->bi;
    ep->dir = 1;
    ep->drive_type = 1;
    ep->delay = pp->delay;
    ep->forced = pp->forced;
    ep->extra_owner = 0;
    ep->owner = 0;
    ep->primary = pp->primary;
    ep->u.p.n = pn;
    pn->p.push_back(ep);
    new_ports.push_back(ep);
    pn->cp[pp->c].push_back(ep);
  }

  pn->spec = NULL;
  pn->i_num = 0;
  pn->cgh = NULL;
  pn->cgt = NULL;
  pn->g_num = 1;
  pn->gh = NULL;
  pn->gt = NULL;
  pn->next = NULL;
  gate *g = create_extra_gate(ip, new_ports, type);
  append_gate(pn, g);
  return pn;
}

//Function to create instances of the extra nodes
inst_node *create_extra_inst (node *pn, node *n, port *ip, std::vector<port *> &p) {
  int o_num = 0;
  inst_node *ein = new inst_node;
  ein->proc = NULL;
  ein->par = pn;
  ein->n = n;

  port *eip;

  eip = new port;
  eip->c = ip->c;
  eip->visited = 0;
  eip->disable = 0;
  eip->dir = 0;
  eip->bi = ip->bi;
  eip->drive_type = ip->drive_type;
  eip->delay = 0;
  eip->forced = 0;
  eip->primary = 0;
  eip->setExtraOwner(ein);
  if (ip->owner == 0) { eip->setOwner(ip->u.p.n); } 
  else if (ip->owner == 1) { eip->setOwner(ip->u.i.in); }
  else { eip->setOwner(ip->u.g.g); }
  ein->p.push_back(eip);
  update_cp(pn, eip);

  for (auto pp : p) {
    eip = new port;
    eip->c = pp->c;
    eip->visited = 1;
    eip->disable = 0;
    eip->bi = pp->bi;
    eip->dir = 1;
    eip->drive_type = 1;
    eip->delay = 0;
    eip->forced = 0;
    eip->primary = 0;
    eip->setExtraOwner(ein);
    if (pp->owner == 0) { eip->setOwner(pp->u.p.n); } 
    else if (pp->owner == 1) { eip->setOwner(pp->u.i.in); } 
    else { eip->setOwner(pp->u.g.g); }
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
void create_mux (graph *g, node *n, port *ip, std::vector<port *> &p, int type) {
//fprintf(stdout, "%s %i\n",n->proc->getName(), extra_num);
  node *en;
  inst_node *ein;
  en = create_extra_node(ip, p, type);
  ein = create_extra_inst(n, en, ip, p);
  append_inst(n, ein);
  prepend_graph(g, en);
  extra_num++;
}


//Function to find driving gate while traversing
//down in the hierarchy
//Arguments are the circuit graph, instance inside which
//the gate is sitting and the output port we are looking for
bool find_driver (graph *g, inst_node *in, port *p){
 
  if (in->n->copy == 0) {
    node *cn;
    cn = copy_node(in->n);
    prepend_graph(g, cn);
    in->n = cn;
  }

  //Find port index inside the instance scope
  port *ip;
  if (in->extra_inst == 0) {
    int iport = 0;
    for (auto pp : in->p) {
      if (pp == p) { break; }
      iport++;
    }
    ip = in->n->p[iport];
  } else { ip = in->n->p[0]; }

  //Traverse port a list of ports corresponding to the
  //case act_connection as the port in the arg list
  int ic = 0;
  port *op = NULL;
  std::vector<port *> npc;
  int primary_flag = 0;
  int fp = 0;
  int oi = 0;

  if (in->extra_inst == 0) {
    for (auto pp : in->n->cp[ip->c]) {
      if (pp->owner == 0 || pp->dir == 1 || pp->visited == 1) { continue; }
      if (pp->extra_owner == 1) {
        op = pp;
        break;
      } else { op = pp; }
    }
  } else { op = in->n->gh->extra_drivers[0]; }

  bool root = true;
  if (op) {
    op->visited = 1;
    if (op->drive_type == 0) {
      if (op->owner == 1 || op->extra_owner == 1) {
        if (op->extra_owner == 0) { root = find_driver(g, op->u.i.in, op); }
        else { root = find_driver(g, op->e, op); }
        if (root) {
          if (primary_flag == 1) { op->primary = 1; }
          if (op->u.i.in->extra_inst != 0) {
            std::vector<port*> mini_mux = {op};
            create_mux(g, in->n, op, mini_mux, 0);
          }
          ip->drive_type = 1;
          op->drive_type = 1;
        } else { return false; }
      } else if (op->owner == 2) {
        ip->drive_type = 1;
        op->drive_type = 1;
        if (primary_flag == 1) { op->primary = 1; }
        if (op->u.g.g->extra_gate == 0) { op->u.g.g->drive_type = 1; }
      }
    } else { ip->drive_type = 1; }
    return true;
  } else { return false; }
  ic = 0;

}

//Function to find multi drivers
//First it checks connection map to find cases with more
//than two outputs driving inputs
//Second it changes the type of the ports:
//  - To primary io change the type
//  - For the gate it changes the gate type and port type
//  - For the instance it changes port type and traverses
//    down the hierarchy to find the source driving gate
//    and update its type as well as all ports connecting
//    to it
void find_md (graph *g, node *n){

  bool root = true;

  //Walk through instances and check if their primary ios were
  //updated at the previous parent level
  for (auto in=n->cgh; in; in=in->next) {
    if (in->extra_inst == 0) {
      for (auto i = 0; i < in->n->p.size(); i++) {
        if (in->n->p[i]->drive_type != in->p[i]->drive_type) { 
          in->p[i]->drive_type = in->n->p[i]->drive_type;
        }
      }
    }
  }

  std::vector<port *> o_port_collection;
  std::vector<port *> i_port_collection;

  int o_num = 0;
  int i_flag_1 = 0;
  int i_flag_2 = 0;
  int i_flag_3 = 0;
  int i_flag_4 = 0;
  int gate_dr = 0;

  for (auto pair : n->cp) {
    //multi driver case
    if (pair.second.size() > 2) {
      o_port_collection.clear();
      i_port_collection.clear();
      o_num = 0;
      i_flag_1 = 0;
      i_flag_2 = 0;
      i_flag_3 = 0;
      i_flag_4 = 0;
      gate_dr = 0;
      for (auto pp : pair.second) {
        if (pp->dir == 1 && pp->owner != 0) {
          if (pp->bi == 0 && i_flag_1 == 0 && pp->drive_type == 0) {
            i_flag_1 = 1;
            i_port_collection.push_back(pp);
            continue;
          }
          if (pp->bi == 0 && i_flag_2 == 0 && pp->drive_type == 1) {
            i_flag_2 = 1;
            i_port_collection.push_back(pp);
            continue;
          }
          if (pp->bi == 1 && i_flag_3 == 0 && pp->drive_type == 0) {
            i_flag_3 = 1;
            i_port_collection.push_back(pp);
            continue;
          }
          if (pp->bi == 1 && i_flag_4 == 0 && pp->drive_type == 1) {
            i_flag_4 = 1;
            i_port_collection.push_back(pp);
            continue;
          }
        }
      }
      for (auto pp : pair.second) {
        if ((pp->dir == 0 && pp->owner != 0)
            | (pp->dir == 1 && pp->owner == 0)) { 
          o_num++; 
          o_port_collection.push_back(pp);
          if (pp->owner == 2 && (pp->u.g.g->type == 1 || pp->u.g.g->type == 3)) { 
            gate_dr = 1; 
          }
        }
      }
      if (o_num > 1) {
        for (auto ip : i_port_collection) {
          if (gate_dr == 1) { create_mux(g, n, ip, o_port_collection, 1); }
          else { create_mux(g, n, ip, o_port_collection, 0); }
          for (auto op : o_port_collection) {
            op->visited = 1;
            if (op->dir == 0) {
              n->decl[op->c]->type = 1;
              if (op->owner == 1) {
                root = find_driver(g, op->u.i.in, op);
                if (root) { op->drive_type = 1; }
              } else if (op->owner == 2) {
                op->drive_type = 1;
                op->u.g.g->drive_type = 1;
                if (op->delay == 0) { op->u.g.g->type = 0; }
              }
            } else {
              if (op->owner == 0) { op->drive_type = 1; }
            }
            var *mv = n->decl[op->c];
            mv->drive_type = 1;
            mv->type = 2;
          }
        }
        for (auto pp : pair.second) {
          pp->wire = 1;
        }
      }
    }
  }

  //Walk through instances and check if their primary ios were
  //updated at the child level. We need this because we only seek
  //for drivers at lower levels and at parent level they are not 
  //resolved. Even if there is no multiple drivers the source driver
  //should be modified to drive-type 1
  for (auto in = n->cgh; in; in = in->next) {
    if (in->extra_inst == 0) {
      for (auto i = 0; i < in->n->p.size(); i++) {
        if (in->n->p[i]->drive_type != in->p[i]->drive_type) { 
          in->p[i]->drive_type = in->n->p[i]->drive_type;
          for (auto pp : n->cp[in->p[i]->c]) {
            if (pp->dir == 1 | pp->drive_type != 0) { continue; }
            if (pp->owner == 0) { pp->drive_type = 1; } 
            else if (pp->owner == 1) {
              if (pp->u.i.in->extra_inst != 0) { continue; }
              root = find_driver(g, pp->u.i.in, pp);
              if (root) { pp->drive_type = 1; }
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

  int remap = 0;

  for (auto nn : graph_copy){
fprintf(stdout, "=============================\n%s\n", nn->proc->getName());
    find_md(g, nn);
  }

}

}
