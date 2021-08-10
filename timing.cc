#include <vector>
#include <string.h>
#include <utility>
#include <act/graph.h>
#include <act/iter.h>

namespace fpga {

void find_min_io_delay(node *, int);
void find_max_io_delay(node *, int);
void find_min_delay(node *, port *, std::map<port *, int> &);
void find_max_delay(node *, port *, std::map<port *, int> &);

//Function to reset visited flags on 
//every node/gate/inst
void unmark_node_visited (node *n) {
  n->visited = 0;
  for (auto in = n->cgh; in; in = in->next) {
    in->visited = 0;
  }
  for (auto gn = n->gh; gn; gn = gn->next ) {
    gn->visited = 0;
  }
}

void unmark_port_visited (node *n) {
  for (auto pp : n->p) {
    pp->visited = 0;
  }

  for (auto in = n->cgh; in; in = in->next) {
    for (auto pp : in->p) {
      pp->visited = 0;
    }
  }

  for (auto gn = n->gh; gn; gn = gn->next) {
    for (auto pp : gn->p) {
      pp->visited = 0;
    }
  }
}

//Check if there is inst node or sh gate 
//in the path
//Func:
//0 - from P to P
//1 - check every owner in the path
//Return:
//0 - clear cycle
//1 - broken cycle
int check_path (port *p, std::vector<port *> path, int func) {
  int check = 0;
  int result = 0;
  for (auto pp : path) {
    if (func == 0) {
      if (pp == p) {
        check = 1;
      }
    } else {
      check = 1;
    }
    if (check == 1) {
      if (pp->owner == 2) {
        if (pp->u.g.g->type == 1 || pp->u.g.g->type == 3) {
          return 1;
        }
      } else {
        return 1;
      }
    }
  }
  return 0;
}

//Function goes thought all nodes and gates 
//inside the process node and collects them 
//in the path vector. Once visited node/gate 
//reached it calls check path to see if there 
//are egisters already on the path.
void break_cycle (node *n, port *p, std::vector<port *> &path, int func, int cur) {
  int iport = 0;
  int oport = 0;
  int cnt = cur;
  if (p->owner == 2) {
    if (p->u.g.g->visited == 1) {
      port *op = p->u.g.g->p[0];
      path.push_back(op);
      if (check_path(op, path, 0) == 0) {
        p->u.g.g->type = 1;
      }
      path.pop_back();
      return;
    } else if (p->u.g.g->visited == 2) {
      return;
    } else {
      p->u.g.g->visited = 1;
      for (auto ip : p->u.g.g->p) {
        if (ip->c == p->c) {
          break;
        }
        iport++;
      }
      oport = p->u.g.g->io_map[iport];
      port *op = p->u.g.g->p[oport];
      path.push_back(op);
      for (auto cp : n->cp[op->c]) {
        if (cmp_owner(op,cp) == 1) {
          continue;
        }
        if (func > 1) {
          ++cnt;
        }
        if (func > 1 && cnt == func) {
          if (op->u.g.g->type < 2) {
            op->u.g.g->type = 1;
          } else if (op->u.g.g->type > 1) {
            op->u.g.g->type = 3;
          }
          cnt = 0;
        }

        if (cp->owner == 0) {
          if (cp->u.p.n == n) {
            if (check_path(cp, path,1) == 0) {
              if (p->u.g.g->type == 0) {
                p->u.g.g->type = 1;
              } else if (p->u.g.g->type == 2) {
                p->u.g.g->type = 3;
              }
              cnt = 0;
              continue;
            }
          }
        }
        break_cycle(n, cp, path, func, cnt);
        if (func > 1) {
          --cnt;
        }
      }
      path.pop_back();
      p->u.g.g->visited = 2;
    }
  } else if (p->owner == 1) { 
    if (p->u.i.in->visited == 1 || p->u.i.in->visited == 2) {
      return;
    } else {
      if (func > 1) {
        cnt = 0;
      }
      p->u.i.in->visited = 1;
      for (auto ip : p->u.i.in->p) {
        if (ip->c == p->c) {
          break;
        }
        iport++;
      }
      for (auto oport : p->u.i.in->n->io_map[iport]) {
        port *op = p->u.i.in->p[oport];
        path.push_back(op);
        for (auto cp : n->cp[op->c]) {
          if (cmp_owner(op,cp) == 1) {
            continue;
          }
          if (cp->owner == 0) {
            if (cp->u.p.n == n) {
              continue;
            }
          }
          break_cycle(n, cp, path, func, cnt);
        }
        path.pop_back();
      }
      p->u.i.in->visited = 2;
    }
  }
}

//Function to execute topological sort
void topo_sort (node *n, port *p, std::vector<port *> &st) {
  
  if (p->visited == 1) {
    return;
  }

  for (auto pp : n->cp[p->c]) {
    if (pp->visited == 1) {
      continue;
    } else {
      pp->visited = 1;
    }
    if (pp == p) {
      continue;
    }
    if (pp->dir == 0 && pp->owner != 0) {
      continue;
    }
    if (pp->owner == 0) {
      st.push_back(pp);
      continue;
    } else if (pp->owner == 1) {
      unsigned int iport = 0;
      for (auto ip : pp->u.i.in->p) {
        if (ip == pp) {
          break;
        }
        iport++;
      }
      for (auto oport : pp->u.i.in->n->io_map[iport]) {
        port *op;
        op = pp->u.i.in->p[oport];
        topo_sort(n, op, st);
      }
    } else if (pp->owner == 2) {
      port *op;
      op = pp->u.g.g->p[0];
      topo_sort(n, op, st);
    }
  }
  p->visited = 1;
  st.push_back(p);

}

void find_max_io_delay (node *n, int ip) {

  std::map<port *, int> maxd;

  for (auto pair : n->cp) {
    for (auto pp : pair.second) {
      if (pp->dir == 0) {
        maxd[pp] = 0;
      }
    }
  }

  std::vector<port *> stack;
  topo_sort(n, n->p[ip], stack);
  unmark_port_visited(n);

  int len = 0;
  len = stack.size();

  for (auto i = len - 1; i >= 0; i--) {
    find_max_delay (n, stack[i], maxd);
  }

  for (auto op : n->io_map[ip]) {
    std::pair<int, int> io;
    io.first = ip;
    io.second = op;
    n->max_io_delay[io] = maxd[n->p[op]];
  }

  unmark_port_visited(n);
}

//Function to find max delays
void find_max_delay(node *n, port * p, std::map<port *, int> &pd){

  unsigned int delay = 0;

  int done = 0;

  for (auto pp : n->cp[p->c]) {
    if (pp == p) {
      continue;
    }
    if (pp->dir == 0 && pp->owner != 0) {
      continue;
    }
    if (pp->owner == 0) {
      pd[pp] = pd[p];
      continue;
    } else if (pp->owner == 1) {
      unsigned int iport = 0;
      for (auto ip : pp->u.i.in->p) {
        if (ip == pp) {
          break;
        }
        iport++;
      }

      for (auto mad : pp->u.i.in->n->max_io_delay) {
        if (mad.first.first == iport) {
          done = 1;
          break;
        }
      }

      if (done == 0) {
        find_max_io_delay(pp->u.i.in->n, iport);
      } else {
        done = 0;
      }

      for (auto oport : pp->u.i.in->n->io_map[iport]) {
        std::pair<int,int> io;
        io.first = iport;
        io.second = oport;
        port *op = pp->u.i.in->p[oport];
        delay = pp->u.i.in->n->max_io_delay[io];
        delay = delay + op->delay;
        if (pd[op] < pd[p] + delay) {
          pd[op] = pd[p] + delay;
        }
      }
    } else if (pp->owner == 2) {
      port *op;
      op = pp->u.g.g->p[0];

      if (pp->u.g.g->type == 1 ||
          pp->u.g.g->type == 3 ) {
        delay = 1 + op->delay;
      } else {
        delay = 0;
      }
      if (pd[op] < pd[p] + delay) {
        pd[op] = pd[p] + delay;
      }
    }
  }
}

void find_min_io_delay (node *n, int ip) {

  std::map<port *, int> mind;

  int oport = 0;
  int i = 0;

  for (auto pair : n->cp) {
    for (auto pp : pair.second) {
      if (pp->dir == 0) {
        mind[pp] = 100000;
      }
    }
  }

  n->p[ip]->visited = 1;
  mind[n->p[ip]] = 0;

  find_min_delay(n, n->p[ip], mind);

  for (auto op : n->io_map[ip]) {
    std::pair <int, int> io;
    io.first = ip;
    io.second = op;
    n->min_io_delay[io] = mind[n->p[op]];
  }

  unmark_port_visited(n);

}

//Dijkstra min path algorithm
//where output ports are "nodes"
//traversing from output port to another
//output port
void find_min_delay(node *n, port *p, std::map<port *, int> &pd) {

  unsigned int delay = 0;

  int done = 0;

  for (auto pp : n->cp[p->c]) {
    //If this is the same port then pass
    if (pp == p) {
      continue;
    }
    //If port is output then pass
    if (pp->dir == 0 && pp->owner != 0) {
      continue;
    }
    //If port is visited then pass
    if (pp->visited == 1) {
      continue;
    } else {
      pp->visited = 1;
    }
    delay = 0;
    //Primary io
    if (pp->owner == 0) {
      pd[pp] = pd[p];
      continue;
    //Instance node
    } else if (pp->owner == 1) {
      //port index
      unsigned int iport = 0;
      //Find input port index
      for (auto ip : pp->u.i.in->p) {
        if (ip == pp) {
          break;
        }
        iport++;
      }

      for (auto mid : pp->u.i.in->n->min_io_delay) {
        if (mid.first.first == iport) {
          done = 1;
          break;
        }
      }

      if (done == 0) {
        find_min_io_delay(pp->u.i.in->n, iport);
      } else {
        done = 0;
      }

      for (auto oport : pp->u.i.in->n->io_map[iport]) {
        std::pair<int,int> io;
        io.first = iport;
        io.second = oport;
        port *op = pp->u.i.in->p[oport];
        delay = pp->u.i.in->n->min_io_delay[io];
        delay = delay + op->delay;
        if (pd[p] + delay < pd[op]) {
          pd[op] = pd[p] + delay;
        } else {
          pd[op] = pd[p];
        }
      }
    //Gate node
    } else if (pp->owner == 2) {
      //For gate owner output port is always p[0]
      //select it for algorithm propagation
      port *op = pp->u.g.g->p[0];

      //If gate type is not combinational or long
      //state holding then delay is 1 + previously
      //added delay, i.e. for overlapping forks
      if (pp->u.g.g->type == 1 ||
          pp->u.g.g->type == 3 ) {
        delay = 1 + op->delay; 
      } else {
        delay = op->delay;
      }
      if (pd[p] + delay < pd[op]) {
        pd[op] = pd[p] + delay;
      } else {
        pd[op] = pd[p];
      }
    }
  }

  
  //fprintf(stdout, "NEW ITERATION: ");
  //p->c->toid()->Print(stdout);
  //fprintf(stdout, " %i \n", pd[p]);
  //for (auto test : pd) {
  //  test.first->c->toid()->Print(stdout);
  //  fprintf(stdout, " %i %i\n", test.second, test.first->owner);
  //}
  

  std::pair<port*, int> next;
  next.first = NULL;
  next.second = 100000;
  for (auto pair : pd) {
    if (pair.second < next.second && pair.first->visited == 0) {
      next.first = pair.first;
      next.second = pair.second;
    }
  }

  if (!next.first) {
    return;
  }

  next.first->visited = 1;

  find_min_delay(n, next.first, pd);
}

//Function to satisfy constraints in the given
//process scope
void sat_constr (node *n, act_connection *ac,
                          act_connection *bc,
                          act_connection *cc) {

  std::map<port *, int> mind;
  std::map<port *, int> maxd;

  port *ap = NULL; //a port
  port *bp = NULL; //b port
  port *cp = NULL; //c port

  
  Assert (ac, "What A?!\n");
  Assert (bc, "What B?!\n");
  Assert (cc, "What C?!\n");
  
  for (auto pp : n->cp[ac]) {
    if (pp->dir == 1 && pp->owner == 0) {
      ap = pp;
      break;
    } else if (pp->dir == 0 && pp->owner != 0) {
      ap = pp;
      break;
    }
  }

  for (auto pair : n->cp) {
    for (auto pp : pair.second) {
      if (pp->dir == 0) {
        mind[pp] = 100000;
        maxd[pp] = 0;
      }
      if (bc == pair.first && pp->dir == 0) {
        bp = pp;
      } else if (cc == pair.first && pp->dir == 0) {
        cp = pp;
      }
    }
  }

 // Assert (ap, "Didn't find A port ?!\n");
 // Assert (bp, "Didn't find B port ?!\n");
 // Assert (cp, "Didn't find C port ?!\n");

  if (!ap || !bp || !cp) { 
    return; 
  }

  /*    
  fprintf(stdout, "%s\n", n->proc->getName());
  ap->c->toid()->Print(stdout);
  fprintf(stdout,"(%i,%i)\t",ap->owner,ap->dir);
  bp->c->toid()->Print(stdout);
  fprintf(stdout,"(%i,%i)\t",bp->owner,bp->dir);
  cp->c->toid()->Print(stdout);
  fprintf(stdout,"(%i,%i)\n",cp->owner,cp->dir);
  */
  int min = 0;
  int max = 0;

  mind[ap] = 0;

  ap->visited = 1;

  find_min_delay (n, ap, mind);
  min = mind[cp];
  unmark_port_visited(n);

  /*
  fprintf(stdout, "\nMIN------------------------\n");
  for (auto dd : mind) {
    dd.first->c->toid()->Print(stdout);
    fprintf(stdout, "=%i(%i)\t",dd.second,dd.first->owner);
  }
  fprintf(stdout, "\n");
  cp->c->toid()->Print(stdout);
  fprintf(stdout, "\n------------------------\n");
  */
  std::vector<port *> stack;
  topo_sort(n, ap, stack);

  int len = 0;
  len = stack.size();

  for (auto i = len - 1; i >= 0; i--) {
    find_max_delay (n, stack[i], maxd);
  }
  /*
  fprintf(stdout, "\nMAX------------------------\n");
  for (auto dd : stack) {
    dd->c->toid()->Print(stdout);
    fprintf(stdout,"(%i)", dd->owner);
    fprintf(stdout, "\t");
  }
  fprintf(stdout, "\n");
  for (auto dd : maxd) {
    dd.first->c->toid()->Print(stdout);
    fprintf(stdout, "=%i\t",dd.second);
  }
  fprintf(stdout, "\n------------------------\n");
  */
  unmark_port_visited(n);

  max = maxd[bp];

  //fprintf(stdout, "===>%i %i\n", min, max);
  if (max >= min) {
    cp->delay = max - min + 1;
  }

}

//Function to process timing contraint
//in the process type
//timing a+/- : b+/- < c+/-
void process_time_proc (node *n, act_spec *sp) {

  act_connection *ac;
  act_connection *bc;
  act_connection *cc;

  Scope *cs;
  cs = n->proc->CurScope();

  ac = sp->ids[0]->Canonical(cs);
  bc = sp->ids[1]->Canonical(cs);
  cc = sp->ids[2]->Canonical(cs);

  sat_constr(n, ac, bc, cc);

}

//Function to walk through all all specs and 
//pick timing ones. Next it calls function
//to process and satisfy timing constraints
void process_time_spec (node *n) {
  act_spec *sp = n->spec;
  while (sp) {
    if (ACT_SPEC_ISTIMING (sp)) {
      process_time_proc (n, sp);
    }
    sp = sp->next;
  }
}

//function to trace timing constraint and find
//scope where all variables are visible
void find_spec_scope (node *pn, ValueIdx *i_name, 
                                act_connection *c_ac, 
                                act_connection *c_bc, 
                                act_connection *c_cc) {
  ActId *tmp;

  ActId *id[3];
  ActId *c_id[3];
  act_connection *con[3];

  c_id[0] = c_ac->toid();
  c_id[1] = c_bc->toid();
  c_id[2] = c_cc->toid();

  Scope *par_scope = pn->proc->CurScope();

  if (i_name) {
    tmp = new ActId(i_name->getName());
  }
  for (auto i = 0; i < 3; i++) {
    if (i_name) {
      id[i] = tmp->Clone();
      id[i]->Append(c_id[i]);
      con[i] = id[i]->Canonical(par_scope);
    } else {
      id[i] = c_id[i];
      con[i] = id[i]->Canonical(par_scope);
    }
  }

  ActId *tl[3];

  Array *aref[3];
  InstType *it[3];
  Arraystep *as[3];

  for (auto i = 0; i < 3; i++) {
    it[i] = par_scope->FullLookup(id[i], &aref[i]);
    if (it[i]->arrayInfo()) {
      if (i == 0) {
        if (!aref[i] || !aref[i]->isDeref()) {
          as[i] = NULL;
          fatal_error("fork LHS must be one bit wide");
          return;
        }
      } else {
        if (!aref[i] || !aref[i]->isDeref()) {
          if (aref[i]) {
            as[i] = it[i]->arrayInfo()->stepper(aref[i]);
          } else {
            as[i] = it[i]->arrayInfo()->stepper();
          }
        } else {
          as[i] = NULL;
        }
      }
    } else {
      as[i] = NULL;
    }
    tl[i] = id[i]->Tail();
  }

  if (!as[1] && !as[2]) {
    if (con[0] && con[1] && con[2]) {
      if (pn->cp.find(con[0]) != pn->cp.end() &&
          pn->cp.find(con[1]) != pn->cp.end() &&
          pn->cp.find(con[2]) != pn->cp.end()) {
   
        sat_constr(pn, con[0], con[1], con[2]);
    
      } else {
        if (pn->iv.size() > 0) {
          for (auto in : pn->iv) {
            find_spec_scope (in->par, in->inst_name, con[0], con[1], con[2]); 
          }
        }
      }
    }
  } else {
    if (as[1] && !as[1]->isend()) {
      while (!as[1]->isend()) {
        if (as[2] && !as[2]->isend()) {
          while (!as[2]->isend()) {
            tl[1]->setArray(as[1]->toArray());
            con[1] = id[1]->Canonical(par_scope);
            tl[2]->setArray(as[2]->toArray());
            con[2] = id[2]->Canonical(par_scope);
            if (con[0] && con[1] && con[2]) {
              if (pn->cp.find(con[0]) != pn->cp.end() &&
                  pn->cp.find(con[1]) != pn->cp.end() &&
                  pn->cp.find(con[2]) != pn->cp.end()) {
                sat_constr(pn, con[0], con[1], con[2]);
                tl[1]->setArray(NULL);
                tl[2]->setArray(NULL);
              } else {
                tl[1]->setArray(NULL);
                tl[2]->setArray(NULL);
                if (pn->iv.size() > 0) {
                  for (auto in : pn->iv) {
                    find_spec_scope(in->par, in->inst_name, con[0], con[1], con[2]);
                  }
                } else {
                  find_spec_scope(pn, NULL, con[0], con[1], con[2]);
                }
              }             
            }
            as[2]->step();
          }
        } else {
          tl[1]->setArray(as[1]->toArray());
          con[1] = id[1]->Canonical(par_scope);
          if (con[0] && con[1] && con[2]) {
            if (pn->cp.find(con[0]) != pn->cp.end() &&
                pn->cp.find(con[1]) != pn->cp.end() &&
                pn->cp.find(con[2]) != pn->cp.end()) {
              sat_constr(pn, con[0], con[1], con[2]);
              tl[1]->setArray(NULL);
            } else {
              tl[1]->setArray(NULL);
              con[1] = id[1]->Canonical(par_scope);
              if (pn->iv.size() > 0) {
                for (auto in : pn->iv) {
                  find_spec_scope(in->par, in->inst_name, con[0], con[1], con[2]);
                }
              } else {
                find_spec_scope(pn, NULL, con[0], con[1], con[2]);
              }
            }
          }
        }
        as[1]->step();
      }
    } else if (as[2] && !as[2]->isend()) {
      while (!as[2]->isend()) {
        tl[2]->setArray(as[2]->toArray());
        con[2] = id[2]->Canonical(par_scope);
        if (con[0] && con[1] && con[2]) {
          if (pn->cp.find(con[0]) != pn->cp.end() &&
              pn->cp.find(con[1]) != pn->cp.end() &&
              pn->cp.find(con[2]) != pn->cp.end()) {
            sat_constr(pn, con[0], con[1], con[2]);
            tl[2]->setArray(NULL);
          } else {
            tl[2]->setArray(NULL);
            if (pn->iv.size() > 0) {
              for (auto in : pn->iv) {
                find_spec_scope(in->par, in->inst_name, con[0], con[1], con[2]);
              }
            } else {
              find_spec_scope(pn, NULL, con[0], con[1], con[2]);
            }
          }             
        }
        as[2]->step();
      }
    }
  }
}

//Function to find timing constraints in the
//nested user defined types
void find_userdef_spec (node *n, UserDef *ud, ActId *pref){

  Scope *cs;
  if (ud) {
    cs = ud->CurScope();
  } else {
    cs = n->proc->CurScope();
  }
  ActInstiter i(cs);
 
  act_spec *sp;
  ActId *tmp = NULL;
  ActId *tl = NULL;

  for (i = i.begin(); i != i.end(); i++) {
    ValueIdx *vx = *i;
    sp = NULL;
    if (TypeFactory::isUserType(vx->t)) {
      if (TypeFactory::isChanType(vx->t) ||
          TypeFactory::isDataType(vx->t) ||
          TypeFactory::isStructure(vx->t)) {
        UserDef *iud;
        iud = dynamic_cast<UserDef *>(vx->t->BaseType());
        if (pref) {
          tl = pref->Tail();
          tmp = new ActId(vx->getName());
          tl->Append(tmp);
        } else {
          pref = new ActId(vx->getName());
        }
        if (vx->t->arrayInfo()) {
          Arraystep *pas = vx->t->arrayInfo()->stepper();
          while (!pas->isend()) {
            pref->Tail()->setArray(pas->toArray());
            find_userdef_spec(n, iud, pref);
            pref->Tail()->setArray(NULL);
            pas->step();
          }
        } else {
          find_userdef_spec(n, iud, pref);
        }
        if (iud->getlang()) {
          if (iud->getlang()->getspec()) {
            if (ACT_SPEC_ISTIMING(iud->getlang()->getspec())) {
              sp = iud->getlang()->getspec();
            }
          }
        }
      }
    }

    act_connection *ac, *bc, *cc;

    Arraystep *as = NULL;
    Array *ar_info = vx->t->arrayInfo();
    Array *ar = NULL;

    if (ar_info) {
      as = ar_info->stepper();
    }

    while (sp) {
      if (ar_info && !as->isend()) {
        ar = as->toArray();
        pref->Tail()->setArray(ar);
      }

      for (int i = 0; i < 3; i++) {
        tmp = pref->Tail();
        tmp->Append(sp->ids[i]);
        if (i == 0) {
          ac = pref->Canonical(n->proc->CurScope());
        } else if (i == 1) {
          bc = pref->Canonical(n->proc->CurScope());
        } else if (i == 2) {
          cc = pref->Canonical(n->proc->CurScope());
        }
        tmp->prune();
      }

      if (ac && bc && cc) {
        if (n->iv.size() > 0) {
          for (auto in : n->iv) {
            find_spec_scope(in->par, in->inst_name, ac, bc, cc);
          }
        } else {
          find_spec_scope(n, NULL, ac, bc, cc);
        }
      }
    
      if (ar_info) {
        as->step();
        pref->Tail()->setArray(NULL);
        if (!as->isend()) {
          continue;
        } else {
          as = ar_info->stepper();
        }
      }
      sp = sp->next;
    }
    if (tl) {
      tl->prune();
    }
  }
}

//Function to process timing constraint
//in the user defined type
void process_userdef_spec (graph *g) {

  for (auto n = g->hd; n; n = n->next) {
    find_userdef_spec(n, NULL, NULL);
  }

}

//Function to mark every gate as sequential
void all_regs (node *n) {
  for (auto gn = n->gh; gn; gn = gn->next) {
    if (gn->type == 0) {
      gn->type = 1;
    } else if (gn->type == 2) {
      gn->type = 3;
    }
  }
}

void add_timing (graph *g, int func) {
  //For every node in the graph place flip-flops
  //either for every gate, or following the rules
  for (auto n = g->hd; n; n = n->next) {
    n->visited = 1;
    if (func == 0) {
      if (n->g_num > 0) {
        all_regs (n);
      }
    } else {
      for (auto p : n->p) {
        if (p->dir == 0) { continue; }
        for (auto cp : n->cp[p->c]) {
          if (p == cp) {
            continue;
          }
          if (cmp_owner(p,cp) == 1 && cp->dir == 0) {
            p->delay = 1;
          } else {
            int cur = 0;
            std::vector<port *> path;
            break_cycle(n, cp, path, func, cur);
          }
        }
      }
      for (auto p : n->gp) {
        if (p->dir == 0) { continue; }
        for (auto cp : n->cp[p->c]) {
          if (p == cp) {
            continue;
          }
          if (cmp_owner(p,cp) == 1 && cp->dir == 0) {
            p->delay = 1;
          } else {
            int cur = 0;
            std::vector<port *> path;
            break_cycle(n, cp, path, func, cur);
          }
        }
      }
    }
    unmark_node_visited(n);
    n->visited = 0;
  }
  //For every node satisfy timing forks
  for (auto n = g->hd; n; n = n->next) {
    if (n->spec) {
      process_time_spec(n);
    }
  }

  process_userdef_spec(g);
  
}

}
