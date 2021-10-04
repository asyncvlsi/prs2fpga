#include <vector>
#include <string.h>
#include <act/graph.h>

namespace fpga{

void process_arb(node *n, act_spec *sp) {
  std::vector<port *> tmp;
  char tmp1[1024];
  char tmp2[1024];
  unsigned int type = 0;
  type = sp->type -1;
  for (auto i = 0; i < sp->count; i++) {
    sp->ids[i]->sPrint(tmp1, 1024);
    for (auto p : n->p) {
      if (p->dir == 0) { continue; }
      p->c->toid()->sPrint(tmp2, 1024);
      if (strcmp(tmp1, tmp2) == 0) {
        p->forced = type;
        tmp.push_back(p);
      }
    }
    for (auto gn = n->gh; gn; gn = gn->next) {
      for (auto p : gn->p) {
        if (p->dir == 1) { continue; }
        p->c->toid()->sPrint(tmp2, 1024);
        if (strcmp(tmp1, tmp2) == 0) {
          p->forced = type;
          tmp.push_back(p);
          break;
        }
      }
    }
    for (auto in = n->cgh; in; in = in->next) {
      for (auto p : in->p) {
        if (p->dir == 1) { continue; }
        p->c->toid()->sPrint(tmp2, 1024);
        if (strcmp(tmp1, tmp2) == 0) {
          p->forced = type;
          tmp.push_back(p);
          break;
        }
      }
    }   
  }
  n->excl[sp->type -2] = tmp;
  tmp.clear();
}

void add_arb (project *p) {

  graph *g = p->g;

  for (auto n = g->hd; n; n = n->next) {
    if (n->spec) {
      act_spec *sp = n->spec;
      while (sp) {
        if (sp->type == 2) {
          process_arb(n,sp);
          p->need_hi_arb = 1;
        } else if (sp->type == 3) {
          process_arb(n,sp);
          p->need_lo_arb = 1;
        }
        sp = sp->next;
      }
    }
  }
}

}
