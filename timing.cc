#include <vector>
#include <string.h>
#include <utility>
#include <act/graph.h>

namespace fpga {

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

//Function to traverse graph and compute delays
//from inputs to outputs
void find_delay (node *n, port *sp, port *p, int &min, int &max) {
  int iport = 0;
  std::pair<int,int> io;
  if (p->owner == 0) {
    int oport = 0;
    for (auto pp : n->p) {
      if (sp == pp) {
        break;
      }
      iport++;
    }
    for (auto pp : n->p) {
      if (p == pp) {
        break;
      }
      oport++;
    }
    io.first = iport;
    io.second = oport;
    if (n->max_io_delay[io] < max) {
      n->max_io_delay[io] = max;
    }
    if (n->min_io_delay[io] > min) {
      n->min_io_delay[io] = min;
    }
    return;
  } else if (p->owner == 1) {
    if (p->u.i.in->visited == 1) {
      return;
    }
    p->u.i.in->visited = 1;
    for (auto pp : p->u.i.in->p) {
      if (pp == p) {
        break;
      }
      iport++;
    }
    node *in;
    in = p->u.i.in->n;
    for (auto oport : in->io_map[iport]) {
      io.first = iport;
      io.second = oport;
      max = max + in->max_io_delay[io];
      min = min + in->min_io_delay[io];
      for (auto cp : n->cp[p->u.i.in->p[oport]->c]) {
        if (cp->dir == 0 && cp->owner != 0) {
          continue;
        }
        if (cmp_owner(cp,p) == 1) {
          continue;
        }
        find_delay (n, sp, cp, min, max);
      }
      max = max - in->max_io_delay[io];
      min = min - in->min_io_delay[io];
    }
  } else if (p->owner == 2) {
    if (p->u.g.g->visited == 1) {
      return;
    }
    p->u.g.g->visited = 1;
    int inc = 0;
    int oport = 0;
    for (auto pp : p->u.g.g->p) {
      if (pp == p) {
        break;
      }
      iport++;
    }
    if (p->u.g.g->type != 0) {
      inc = 1;
    }
    max = max + inc;
    min = min + inc;
    oport = p->u.g.g->io_map[iport];
    for (auto cp : n->cp[p->u.g.g->p[oport]->c]) {
      find_delay(n, sp, cp, min, max);
    }
    max = max - inc;
    min = min - inc;
  }
}

//Function to count delay from every input
//to every output
void map_io_delay (node *n) {
  int min = 0;
  int max = 0;
  for (auto p : n->p) {
    if (p->dir == 0) {
      continue;
    }
    for (auto cp : n->cp[p->c]) {
      if (cp->dir == 0 && cp->owner != 0) {
        continue;
      }
      if (cmp_owner(cp, p) == 1) {
        continue;
      }
      min = 0;
      max = 0;
      find_delay(n, p, cp, min, max);
      unmark_node_visited(n);
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
		}
    if (pp == p) {
      continue;
    }
    if (pp->dir == 0) {
      continue;
    }
    if (pp->owner == 0) {
      pp->visited = 1;
      continue;
    } else if (pp->owner == 1) {
      pp->visited = 1;
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
      pp->visited = 1;
      port *op;
      op = pp->u.g.g->p[0];
      topo_sort(n, op, st);
    }
  }
  p->visited = 1;
  st.push_back(p);

}

//Function to find max delays
void find_max_delay(node *n, port * sp, std::map<port *, int> &pd){

  unsigned int delay = 0;

  for (auto ip : n->cp[sp->c]) {
    if (ip == sp) {
      continue;
    }
    if (ip->dir == 0) {
      continue;
    }
    if (ip->owner == 0) {
      continue;
    } else if (ip->owner == 1) {
      unsigned int iport = 0;
      for (auto ipp : ip->u.i.in->p) {
        if (ipp == ip) {
          break;
        }
        iport++;
      }
      for (auto oport : ip->u.i.in->n->io_map[iport]) {
        std::pair<int,int> io;
        io.first = iport;
        io.second = oport;
        port *op = ip->u.i.in->p[oport];
        delay = ip->u.i.in->n->max_io_delay[io];
        if (pd[op] < pd[sp] + delay) {
          pd[op] = pd[sp] + delay;
        }
      }
    } else if (ip->owner == 2) {
      port *op;
      op = ip->u.g.g->p[0];
      if (ip->u.g.g->type != 0) {
        delay = 1;
      } else {
        delay = 0;
      }
      if (pd[op] < pd[sp] + delay) {
        pd[op] = pd[sp] + delay;
      }
    }
  }
}

//Dijkstra min path algorithm
//where output ports are "nodes"
void find_min_delay(node *n, port *p, std::map<port *, int> &pd) {

  unsigned int delay = 0;

  for (auto pp : n->cp[p->c]) {
    if (pp == p) {
      continue;
    }
    if (pp->visited == 1) {
      continue;
    } else {
      pp->visited = 1;
    }
    if (pp->owner == 0) {
      continue;
    } else if (pp->owner == 1) {
      if (pp->dir == 0) {
        continue;
      }
      unsigned int iport = 0;
      for (auto ip : pp->u.i.in->p) {
        if (ip == pp) {
          break;
        }
        iport++;
      }
      for (auto oport : pp->u.i.in->n->io_map[iport]) {
        std::pair<int, int> io;
        io.first = iport;
        io.second = oport;
        delay = pp->u.i.in->n->min_io_delay[io];
        port *opp = pp->u.i.in->p[oport];
        if (pd[p] + delay < pd[opp]) {
          pd[opp] = pd[p] + delay;
        }
      }
    } else if (pp->owner == 2) {
      if (pp->dir == 0) {
        continue;
      }
      port *op = pp->u.g.g->p[0];
      if (pp->u.g.g->type != 0) {
        if (pd[p] + 1 < pd[op]) {
          pd[op] = pd[p] + 1;
        } else if (pd[p] < pd[op]) {
          pd[op] = pd[p];
        }
      }
    }
  }

  std::pair<port*, int> next;
  next.first = NULL;
  next.second = 10000;
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

//Function to process timing contraint
//timing a+/- : b+/- < c+/-
void process_time (node *n, act_spec *sp) {

  std::map<port *, int> mind;
  std::map<port *, int> maxd;

  port *ap = NULL; //a port
  port *bp = NULL; //b port
  port *cp = NULL; //c port

  char a[1024];
  char b[1024];
  char c[1024];
  char tmp[1024];

  act_connection *ac;
  act_connection *bc;
  act_connection *cc;

  Scope *cs;
  cs = n->proc->CurScope();

  ac = sp->ids[0]->Canonical(cs);
  bc = sp->ids[1]->Canonical(cs);
  cc = sp->ids[2]->Canonical(cs);

  Assert (ac, "What A?!\n");
  Assert (bc, "What B?!\n");
  Assert (cc, "What C?!\n");

  ac->toid()->sPrint(a,1024);
  bc->toid()->sPrint(b,1024);
  cc->toid()->sPrint(c,1024);

  //Check primary ports
  for (auto pp : n->p) {
   // if (pp->dir == 0) {
   //   mind[pp] = 100000;
   //   maxd[pp] = 100000;
   // }
    pp->c->toid()->sPrint(tmp, 1024);
    if (strcmp(a, tmp) == 0) {
      ap = pp;
    }
		if (strcmp(b, tmp) == 0) {
			bp = pp;
		}
		if (strcmp(c, tmp) == 0) {
			cp = pp;
		}
  }

  //Check instances ports
  for (auto in = n->cgh; in; in = in->next) {
    for (auto pp : in->p) {
      if (pp->dir == 1) {
        continue;
      } else if (pp->dir == 0) {
        mind[pp] = 100000;
        maxd[pp] = 0;
      }
      pp->c->toid()->sPrint(tmp, 1024);
      if (strcmp(a, tmp) == 0) {
        ap = pp;
      }
      if (strcmp(b, tmp) == 0) {
        bp = pp;
      }
      if (strcmp(c, tmp) == 0) {
        cp = pp;
      }
    }
  }

  //Check gates ports
  for (auto gn = n->gh; gn; gn = gn->next) {
    for (auto pp : gn->p) {
      if (pp->dir == 0) {
        mind[pp] = 100000;
        maxd[pp] = 0;
      }
    }
    gn->id->sPrint(tmp, 1024);
    if (strcmp(a, tmp) == 0) {
      ap = gn->p[0];
    }
    if (strcmp(b, tmp) == 0) {
      bp = gn->p[0];
    }
    if (strcmp(c, tmp) == 0) {
      cp = gn->p[0];
    }
  }

  Assert (ap, "Didn't find A port ?!\n");
  Assert (bp, "Didn't find B port ?!\n");
  Assert (cp, "Didn't find C port ?!\n");

	/*
	fprintf(stdout, "%s\n", n->proc->getName());
  fprintf(stdout ,"a-%i\tb-%i\tc-%i\n", ap->dir, bp->dir, cp->dir);
  ap->c->toid()->Print(stdout);
  fprintf(stdout,"\t");
  bp->c->toid()->Print(stdout);
  fprintf(stdout,"\t");
  cp->c->toid()->Print(stdout);
  fprintf(stdout,"\n");
	*/
  unsigned int min = 0;
  unsigned int max = 0;

  mind[ap] = 0;

  ap->visited = 1;

  find_min_delay (n, ap, mind);
  min = mind[cp];
  unmark_port_visited(n);
	/*
	for (auto dd : mind) {
		dd.first->c->toid()->Print(stdout);
		fprintf(stdout, "=%i\t",dd.second);
	}
	fprintf(stdout, "\n------------------------\n");
	*/
  std::vector<port *> stack;
  for (auto pp : n->p) {
    if (pp->dir == 1) {
      topo_sort (n, pp, stack);
      pp->visited = 1;
    }
  }

  int len = 0;
  len = stack.size();

  for (auto i = len - 1; i >= 0; i--) {
    find_max_delay (n, stack[i], maxd);
  }
	/*
	for (auto dd : stack) {
		dd->c->toid()->Print(stdout);
		fprintf(stdout, "\t");
	}
	fprintf(stdout, "\n------------------------\n");
	for (auto dd : maxd) {
		dd.first->c->toid()->Print(stdout);
		fprintf(stdout, "=%i\t",dd.second);
	}
	fprintf(stdout, "\n------------------------\n");
	*/
  unmark_port_visited(n);

  max = maxd[bp];

	//fprintf(stdout, "%i %i\n", min, max);

  if (max >= min) {
    cp->delay = max - min + 1;
  }

}

//Function to walk through all all specs and 
//pick timimng ones. Next it calls function
//to process and satisfy timing constraints
void process_time_spec (node *n) {
  act_spec *sp = n->spec;
  while (sp) {
    if (sp->type == -1) {
      process_time (n, sp);
    }
    sp = sp->next;
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
    map_io_delay(n);
    n->visited = 0;
  }
  for (auto n = g->hd; n; n = n->next) {
    if (n->spec) {
      process_time_spec(n);
    }
  }

}

}
