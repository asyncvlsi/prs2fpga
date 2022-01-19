#include <string.h>
#include <map>
#include <vector>
#include <algorithm>
#include <utility>
#include <act/graph.h>
#include <act/passes/booleanize.h>
#include <act/passes/netlist.h>
#include <act/iter.h>

namespace fpga {

static ActBooleanizePass *BOOL = NULL;
static ActNetlistPass *NETL = NULL;

act_booleanized_var_t *get_bool_var(act_boolean_netlist_t *bnl, act_connection *pc)
{
  ihash_bucket *hb;
  act_booleanized_var_t *bv;

  hb = ihash_lookup(bnl->cH, (long)pc);
  bv = (act_booleanized_var_t *)hb->v;

  return bv;
}

bool is_input (act_boolean_netlist_t *bnl, act_connection *pc)
{
  act_booleanized_var_t *bv = get_bool_var(bnl, pc);

  return bv->input == 1 && bv->used == 1;
}

bool is_output (act_boolean_netlist_t *bnl, act_connection *pc)
{
  act_booleanized_var_t *bv = get_bool_var(bnl, pc);

  return bv->output == 1 && bv->used == 1;
}

bool is_bidir (act_boolean_netlist_t *bnl, act_connection *pc)
{
  act_booleanized_var_t *bv = get_bool_var(bnl, pc);

  return bv->input == 1 && bv->output == 1;
}

//Function to compare two ports' owners
//0 - Not equal
//1 - Equal
bool cmp_owner (port *p1, port *p2) {
  if (p1->owner == p2->owner) {
    if (p1->owner == 0)        { return p1->u.p.n == p2->u.p.n; } 
    else if (p1->owner == 1) { return p1->u.i.in == p2->u.i.in; }
    else if (p1->owner == 2) { return p1->u.g.g == p2->u.g.g; } 
    else {
     /* should not be here? */
     fatal_error ("Should not be here");
     return false;
    }
  } else {
    return false;
  }
}

//Function to travers thorugh the graph
//from input to the output and do mapping
void find_out (node *n, int ip, port *p) {
  int oport = 0;
  int iport = 0;
  if (p->u.p.n == n && p->dir == 0) {
    for (auto pp : n->p) {
      if (pp->c == p->c && pp->dir == p->dir) {
        if (std::find(n->io_map[ip].begin(),
                      n->io_map[ip].end(),
                      oport) == n->io_map[ip].end()) {
          n->io_map[ip].push_back(oport);
          return;
        }
      }
      oport++;
    }
  }
  oport = 0;
  if (p->visited == 1 || p->dir == 0) {
    return;
  }
  p->visited = 1;
  if (p->owner == 1) {
    for (auto pp : p->u.i.in->p) {
      if (pp->c == p->c && pp->dir == p->dir) {
        break;
      }
      iport++;
    }
    for (auto op : p->u.i.in->n->io_map[iport]) {
      act_connection *tmp_c;
      tmp_c = p->u.i.in->p[op]->c;
      for (auto cp : n->cp[tmp_c]) {
        if (cmp_owner(cp,p->u.i.in->p[op])) {
          continue;
        }
        find_out(n,ip,cp);
      }
    }
  } else if (p->owner == 2) {
    for (auto pp : p->u.g.g->p) {
      if (pp->c == p->c && pp->dir == p->dir) {
        break;
      }
      iport++;
    }
    oport = p->u.g.g->io_map[iport]; 
    for (auto cp : n->cp[p->u.g.g->p[oport]->c]) {
      if (cmp_owner(p,cp)) {
        continue;
      }
      find_out(n,ip,cp);
    }
  }
}

//Function to map inputs to all corresponding
//outputs
void map_io (graph *g) {
  for (auto n = g->hd; n; n = n->next) {
    int iport = 0;
    for (auto pp : n->p) {
      if (pp->dir == 0) {
        iport++;
        continue;
      }
      for (auto cp : n->cp[pp->c]) {
        if (cp == pp) {
          continue;
        }
        find_out(n,iport,cp);
        if (n->gh) {
          //Reset gates
          for (auto gn = n->gh; gn; gn = gn->next) {
            for (auto pp : gn->p) {
              pp->visited = 0;
            }
          }
        }
        //Reset instances
        if (n->cgh) {
          for (auto in = n->cgh; in; in = in->next) {
            for (auto pp : in->p) {
              pp->visited = 0;
            }
          }
        }
      }
      iport++;
    }
  }
}

//Function to map connections to ports.
//This data structure will be used to detect multi
//drivers and to traverse graph. No edges needed.
void map_cp (graph *g) {
  for (auto n = g->hd; n; n = n->next) {
    for (auto np : n->p) {
      n->cp[np->c].push_back(np);
    }
    for (auto gnp : n->gp) {
      n->cp[gnp->c].push_back(gnp);
    }
    for (auto in = n->cgh; in; in = in->next) {
      for (auto ip : in->p) {
        n->cp[ip->c].push_back(ip);
      }
    }
    for (auto gg = n->gh; gg; gg = gg->next) {
      for (auto gp : gg->p) {
        n->cp[gp->c].push_back(gp);
      }
    }
  }
}

//Function to add ports to gates.
//Extracting from production rules expression tree
//traversal.
void add_gate_ports(Scope *cs, act_boolean_netlist_t *bnl, act_prs_expr_t *e, gate *g) {

  act_connection *pc;

  if (!e) {
    return;
  }
  int added = 0;
  if (e->type == ACT_PRS_EXPR_VAR) {
    for (auto pp : g->p) {
      char buf1[10240];
      char buf2[10240];
      e->u.v.id->Canonical(cs)->toid()->sPrint(buf1, 10240);
      pp->c->toid()->sPrint(buf2, 10240);
      if (strcmp(buf1, buf2) == 0) {
        added = 1;
        break;
      }
    }
    if (added == 0) {
      port *p = new port;
      p->dir = 1;
      pc = e->u.v.id->Canonical(cs);
      p->c = pc;
      if (is_bidir(bnl, pc)) { p->bi = 1; } 
      else { p->bi = 0; }
      p->setOwner(g);
      g->p.push_back(p);
    } else {
      added = 0;
    }
  } else {
    add_gate_ports(cs, bnl, e->u.e.l, g);
    add_gate_ports(cs, bnl, e->u.e.r, g);
  }
}

//Function to add gates to the process node.
//Collects all production rules for each gate
//including those described in different prs
//bodies.
//If there is no production rules for some 
//networks, NULL pointer will be added to the 
//vector to simplify muli driver resolving
void add_gates (Scope *cs, act_boolean_netlist_t *bnl, netlist_t *nl, act_prs *prs, node *par) {

  act_prs *tmp_prs;
  tmp_prs = prs;

  char tmp1[10240];
  char tmp2[10240];
  std::vector<act_prs_lang_t *> added_prs;
  int added = 0;

  act_connection *pc;

  while (prs) {
    for (auto pl = prs->p; pl; pl = pl->next) {
     
      tmp_prs = prs;

      pl->u.one.id->sPrint(tmp1, 10240);
      for (auto apl : added_prs) {
        apl->u.one.id->sPrint(tmp2, 10240);
        if (strcmp(tmp1, tmp2) == 0) {
          added = 1;
          break;
        }
      }

      if (added == 1) {
        added = 0;
        continue;
      } else {
        added_prs.push_back(pl);
      }
  
      pc = pl->u.one.id->Canonical(cs);

      gate *g = new gate;
      g->id = pc->toid();
      g->type = 0;
      g->drive_type = 0;
      g->next = NULL;
      g->visited = 0;
      g->extra_gate = 0;
  
      port *go = new port;
      go->c = pc;
      go->dir = 0;
      go->bi = 0;
      go->setOwner(g);
      g->p.push_back(go);

      int is_weak = 0;
      if (pl->u.one.attr) {
        for (auto attr = pl->u.one.attr; attr; attr = attr->next) {
          char const *weak = "weak";
          if (strcmp (weak, attr->attr) == 0) {
            is_weak = 1;
            g->is_weak = 1;
          }
        }
      }

      if (pl->u.one.dir == 0) {
        if (is_weak == 1) {
          g->w_p_dn.push_back(pl);
          if (pl->u.one.arrow_type != 0) {
            g->w_p_up.push_back(pl);
          }
        } else {
          g->p_dn.push_back(pl);
          if (pl->u.one.arrow_type != 0) {
            g->p_up.push_back(pl);
          }
        }
      } else {
        if (is_weak == 1) {
          g->w_p_up.push_back(pl);
          if (pl->u.one.arrow_type != 0) {
            g->w_p_dn.push_back(pl);
          }
        } else {
          g->p_up.push_back(pl);
          if (pl->u.one.arrow_type != 0) {
            g->p_dn.push_back(pl);
          }
        }
      }

      while (tmp_prs) {
        for (auto tpl = tmp_prs->p; tpl; tpl = tpl->next) {

          if (tpl == pl) {
            continue;
          }

          tpl->u.one.id->sPrint(tmp2, 10240);

          if (strcmp(tmp1,tmp2) != 0) {
            continue;
          }

          is_weak = 0;
          if (tpl->u.one.attr) {
            for (auto attr = tpl->u.one.attr; attr; attr = attr->next) {
              char const *weak = "weak";
              if (strcmp (weak, attr->attr) == 0) {
                is_weak = 1;
              }
            }
          }

          if (tpl->u.one.dir == 0) {
            if (is_weak == 1) {
              g->w_p_dn.push_back(tpl);
              if (tpl->u.one.arrow_type != 0) {
                g->w_p_up.push_back(tpl);
              }
            } else {
              g->p_dn.push_back(tpl);
              if (tpl->u.one.arrow_type != 0) {
                g->p_up.push_back(tpl);
              }
            }
          } else {
            if (is_weak == 1) {
              g->w_p_up.push_back(tpl);
              if (tpl->u.one.arrow_type != 0) {
                g->w_p_dn.push_back(tpl);
              }
            } else {
              g->p_up.push_back(tpl);
              if (tpl->u.one.arrow_type != 0) {
                g->p_dn.push_back(tpl);
              }
            }
          }
        }
        tmp_prs = tmp_prs->next;
      }

      for (auto l : g->p_up)   { add_gate_ports (cs, bnl, l->u.one.e, g); }
      for (auto l : g->p_dn)   { add_gate_ports (cs, bnl, l->u.one.e, g); }
      for (auto l : g->w_p_up) { add_gate_ports (cs, bnl, l->u.one.e, g); }
      for (auto l : g->w_p_dn) { add_gate_ports (cs, bnl, l->u.one.e, g); }

      for (auto nn = nl->hd; nn; nn = nn->next) {
        if (nn->v) {
          if (nn->v->stateholding == 0) { continue; }

          ActId *vi;
          vi = nn->v->v->id->toid()->Canonical(cs)->toid();

          g->id->sPrint(tmp1, 10240);
          vi->sPrint(tmp2, 10240);
          
          if (strcmp(tmp1, tmp2) == 0) {
            g->type = 2;
          }
        }
      }

      if (g->p_up.size() == 0)  { g->p_up.push_back(NULL); }
      if (g->p_dn.size() == 0)  { g->p_dn.push_back(NULL); }
      if (g->w_p_up.size() == 0){ g->w_p_up.push_back(NULL); }
      if (g->w_p_dn.size() == 0){ g->w_p_dn.push_back(NULL); }

      int oport = 0;
      int iport = 1;
      for (auto ip : g->p) {
        if (ip->dir == 1) {
          g->io_map[iport] = oport;
          iport++;
        }
      }

      par->g_num++;
      if (par->gh) {
        par->gt->next = g;
        par->gt = g;
      } else {
        par->gh = g;
        par->gh->next = par->gt;
        par->gt = g;
      }
    }
    prs = prs->next;
  }  
}

//Function to map instances to the unique nodes
//Need it for hierarchical approach
void map_instances (graph *g) {
  for (auto n = g->hd; n; n = n->next) {
    for (auto in = n->cgh; in; in = in->next) {
      for (auto nn = g->hd; nn; nn = nn->next) {
        if (in->proc == nn->proc) {
          in->n = nn;
          nn->iv.push_back(in);
        }
      }
    }
  }
}

void map_instances_io (graph *g) {
  for (auto n = g->hd; n; n= n->next) {
    for (auto in = n->cgh; in; in = in->next) {
    }
  }
}

//Function to add instances to the process node
//Extra work is done to extract groups of ports 
//for each instance. In booleanize structure 
//there is a list of all ports with no separation
void add_instances (Scope *cs, act_boolean_netlist_t *bnl, node *par) {

  ActUniqProcInstiter i(cs);

  int iport = 0;
  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    Process *i_proc;
    i_proc = dynamic_cast<Process *>(vx->t->BaseType());

    if (BOOL->getBNL(i_proc)->isempty) {
      continue;
    }

    act_boolean_netlist_t *sub;
    sub = BOOL->getBNL (i_proc);

    int ports_exist = 0;
    for (int j = 0; j < A_LEN(sub->ports); j++) {
      if (sub->ports[j].omit == 0) {
      	ports_exist = 1;
      	break;
      }
    }

    act_connection *pc;
    act_connection *i_pc;

    if (ports_exist == 1) {
      if (vx->t->arrayInfo()) {
	      Arraystep *as = vx->t->arrayInfo()->stepper();
	      while (!as->isend()) {
	        if (vx->isPrimary (as->index())) {
            inst_node *in = new inst_node;
            in->n = NULL;
            in->par = par;
            in->inst_name = vx;
            in->array = as->string();
            in->next = NULL;
            in->proc = i_proc;
            in->extra_inst = 0;
            for (int j = 0; j < A_LEN(sub->ports); j++) {
              if (sub->ports[j].omit) continue;
              pc = bnl->instports[iport]->toid()->Canonical(cs);
              if (is_input(sub, sub->ports[j].c)) {
                port *pp = new port;
                pp->c = pc;
                pp->dir = 1;
                if (is_output(sub, sub->ports[j].c)) { pp->bi = 1; } 
                else { pp->bi = 0; }
                pp->setOwner(in);
                in->p.push_back(pp);
              }
              if (is_output(sub,sub->ports[j].c)) {
                port *pp = new port;
                pp->c = pc;
                pp->dir = 0;
                if (is_input(sub,sub->ports[j].c)) { pp->bi = 1; } 
                else { pp->bi = 0; }
                pp->setOwner(in);
                in->p.push_back(pp);
              }
              iport++;
            }
            par->i_num++;
            if (par->cgh) {
              par->cgt->next = in;
              par->cgt = in;
            } else {
              par->cgh = in;
              par->cgh->next = par->cgt;
              par->cgt = in;
            }
          }
          as->step();
        }
      } else {
        inst_node *in = new inst_node;
        in->n = NULL;
        in->par = par;
        in->inst_name = vx;
        in->array = NULL;
        in->next = NULL;
        in->proc = i_proc;
        in->extra_inst = 0;
        for (int j = 0; j < A_LEN(sub->ports); j++) {
          if (sub->ports[j].omit) continue;
          pc = bnl->instports[iport]->toid()->Canonical(cs);
          if (is_input(sub, sub->ports[j].c)) {
            port *pp = new port;
            pp->c = pc;
            pp->dir = 1;
            if (is_output(sub, sub->ports[j].c)) { pp->bi = 1; } 
            else { pp->bi = 0; }
            pp->setOwner(in);
            in->p.push_back(pp);
          }
          if (is_output(sub, sub->ports[j].c)) {
            port *pp = new port;
            pp->c = pc;
            pp->dir = 0;
            if (is_input(sub,sub->ports[j].c)) { pp->bi = 1; } 
            else { pp->bi = 0; }
            pp->setOwner(in);
            in->p.push_back(pp);
          }
          iport++;
        }
        par->i_num++;
        if (par->cgh) {
          par->cgt->next = in;
          par->cgt = in;
        } else {
          par->cgh = in;
          par->cgh->next = par->cgt;
          par->cgt = in;
        }
      }
    }
  }
}

//Function to add internal ports to the process
//as well as global ports
void add_proc_ports (Scope *cs, act_boolean_netlist_t *bnl, node *pn) {

  act_connection *pc;

  //adding internal ports to process node 
  for (int i = 0; i < A_LEN(bnl->ports); i++) {
    if (bnl->ports[i].omit) continue;
    pc = bnl->ports[i].c->toid()->Canonical(cs);
    if (is_input(bnl, pc)) {
      port *fpi = new port;
      fpi->c = pc;
      fpi->dir = 1;
      if (is_output(bnl, pc)) { fpi->bi = 1; } 
      else { fpi->bi = 0; }
      fpi->setOwner(pn);
      pn->p.push_back(fpi);
    }
    if (is_output(bnl,pc)) {
      port *fpo = new port;
      fpo->c = pc;
      fpo->dir = 0;
      if (is_input(bnl, pc)) { fpo->bi = 1; } 
      else { fpo->bi = 0; }
      fpo->setOwner(pn);
      pn->p.push_back(fpo);
    }
  }

  //adding global ports to process node
  for (int i = 0; i < A_LEN(bnl->used_globals); i++) {
    port *fgp = new port;
    fgp->c = bnl->used_globals[i]->toid()->Canonical(cs); 
    fgp->dir = 1;
    fgp->bi = 0;
    fgp->setOwner(pn);
    pn->gp.push_back(fgp);
  }
}

void traverse_exp(Expr *e) {
  if (e->type == 0) {
    fprintf(stdout, "E TYPE 0\n");
    traverse_exp(e->u.e.l);
    traverse_exp(e->u.e.r);
  } else if (e->type == 1) {
    fprintf(stdout, "E TYPE 1\n");
  }
}

//Main function to create a linked list of 
//unique process node from ACT data structure
void build_graph (Process *p, graph *g) {

  act_boolean_netlist_t *bnl = BOOL->getBNL(p);
  netlist_t *nl = NETL->getNL(p);

  if (bnl->visited) {
    return;
  }
  bnl->visited = 1;

  if (bnl->isempty) {
    return;
  } 

  act_languages *lang;
  Scope *cs;
  act_prs *prs;
  act_spec *spec;

  if (p) {
    lang = p->getlang();
    cs = p->CurScope();
  }

  if (lang) {
    prs = lang->getprs();
    spec = lang->getspec();
  }

  ActInstiter i(cs);

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    if (TypeFactory::isProcessType(vx->t)) {
      build_graph (dynamic_cast<Process *>(vx->t->BaseType()), g);
    }
  }

  node *pn = new node;

  pn->visited = 0;
  pn->extra_node = 0;
  pn->proc = p;
  pn->gh = NULL;
  pn->gt = NULL;
  pn->cgh = NULL;
  pn->cgt = NULL;
  pn->spec = spec;
  pn->next = NULL;
  pn->i_num = 0;
  pn->g_num = 0;
  pn->copy = 0;
  pn->weight = 0;

  add_proc_ports(cs,bnl,pn);
  add_instances(cs,bnl,pn);

  if (prs) { add_gates(cs,bnl,nl,prs,pn); }

  //appending process node to the graph
  if (g->hd) {
    g->tl->next = pn;
    g->tl = pn;
  } else {
    g->hd = pn;
    g->hd->next = g->tl;
    g->tl = pn;
  }

}

void declare_vars (Process *p, graph *g)
{
  act_boolean_netlist_t *bnl;
  Scope *cs;

  int tmp = 0;

  for (auto n = g->hd; n; n = n->next) {
    bnl = BOOL->getBNL(n->proc);
    cs = n->proc->CurScope();
    
    phash_iter_t hiter;
    phash_bucket_t *b;
    phash_iter_init (bnl->cH, &hiter);
    while (b = phash_iter_next(bnl->cH, &hiter)) {
      act_booleanized_var_t *bv;
      bv = (act_booleanized_var_t *)b->v;
      var *fv = new var;
      if (bv->isport == 1 || bv->isglobal == 1) {
        fv->type = 0;
        if (bv->input == 1 && bv->output == 1) {
          fv->port = 2;
        } else {
          fv->port = 1;
        }
      } else {
        fv->type = 2;
        fv->port = 0;
      }
      fv->drive_type = 0;
      fv->forced = 0;
      fv->delay = 0;

      act_connection *vc = bv->id->toid()->Canonical(cs);
      for (auto pp : n->cp[vc]) {
        if (pp->owner == 1) { tmp = 1; }
        else { tmp = 0; break; }
      }
      if (tmp == 1) { fv->type = 1; }

      n->decl[vc] = fv;
    }
  }
}

//Function to determine type of an output port
//either wire or reg
void declare_ports (graph *g)
{
  for (auto n = g->hd; n; n = n->next) {
    for (auto pp : n->p) {
      if (pp->dir == 1) { continue; }
      for (auto in = n->cgh; in; in = in->next) {
        for (auto ip : in->p) {
          if (ip->dir == 1) { continue; }
          if (ip->c == pp->c) { pp->wire = 1; }
        }
      }
    }
  }
  return;
}

void build_project_graph (project *proj, Act *a, Process *p) {

  ActPass *apb = a->pass_find("booleanize");
  ActPass *apn = a->pass_find("prs2net");

  BOOL = dynamic_cast<ActBooleanizePass *>(apb);
  NETL = dynamic_cast<ActNetlistPass *>(apn);

  graph *g = new graph;
  g->hd = NULL;
  g->tl = NULL;

  build_graph(p, g);
  map_instances(g);
  map_cp(g);
  map_io(g);
  declare_vars(p, g);
  declare_ports(g);

  proj->g = g;

}

}
