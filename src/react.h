#ifndef MCELL_REACT
#define MCELL_REACT

#include "mcell_structs.h"


/* In react_trig.c */
struct rxn* trigger_unimolecular(u_int hash,struct abstract_molecule *reac);
struct rxn* trigger_surface_unimol(struct abstract_molecule *reac,struct wall *w);
int trigger_bimolecular_preliminary(u_int hashA,u_int hashB,
  struct species *reacA,struct species *reacB);
int trigger_trimolecular_preliminary(u_int hashA, u_int hashB, u_int hashC,
  struct species *reacA, struct species *reacB, struct species *reacC);
int trigger_bimolecular(u_int hashA,u_int hashB,
  struct abstract_molecule *reacA,struct abstract_molecule *reacB,
  short orientA,short orientB, struct rxn **matching_rxns);
int trigger_trimolecular(u_int hashA,u_int hashB, u_int hashC,
  struct species *reacA,struct species *reacB,
  struct species *reacC, int orientA, int orientB, int orientC, 
  struct rxn **matching_rxns);
struct rxn* trigger_intersect(u_int hashA,struct abstract_molecule *reacA,
  short orientA,struct wall *w);


/* In react_cond.c */
int test_unimolecular(struct rxn *rx);
double timeof_unimolecular(struct rxn *rx, struct abstract_molecule *a);
double timeof_special_unimol(struct rxn *rxuni,struct rxn *rxsurf, struct abstract_molecule *a);
int which_unimolecular(struct rxn *rx, struct abstract_molecule *a);
int is_surface_unimol(struct rxn *rxuni,struct rxn *rxsurf, struct abstract_molecule *a);
int test_bimolecular(struct rxn *rx,double scaling, struct abstract_molecule *a1, struct abstract_molecule *a2);
int test_many_bimolecular(struct rxn **rx,double *scaling, int n, int *chosen_pathway, struct abstract_molecule **complexes, int *complex_limits);
int test_intersect(struct rxn *rx,double scaling);
void check_probs(struct rxn *rx,double t);


/* In react_outc.c */
int outcome_products(struct wall *w,struct volume_molecule *reac_m,
  struct grid_molecule *reac_g,struct rxn *rx,int path,struct storage *local,
  short orientA,short orientB,double t,struct vector3 *hitpt,
  struct abstract_molecule *reacA,struct abstract_molecule *reacB,
  struct abstract_molecule *moving);
int outcome_products_trimol_reaction(struct wall *w,
  struct volume_molecule *reac_m, struct grid_molecule *reac_g,
  struct rxn *rx,int path,struct storage *local,
  short orientA, short orientB, short orientC,
  double t,struct vector3 *hitpt,
  struct abstract_molecule *reacA,struct abstract_molecule *reacB,
  struct abstract_molecule *reacC, struct abstract_molecule *moving);
int outcome_unimolecular(struct rxn *rx,int path,
  struct abstract_molecule *reac,double t);
int outcome_bimolecular(struct rxn *rx,int path,
  struct abstract_molecule *reacA,struct abstract_molecule *reacB,
  short orientA,short orientB,double t,struct vector3 *hitpt,
  struct vector3 *loc_okay);
int outcome_trimolecular(struct rxn *rx,int path,
  struct abstract_molecule *reacA,struct abstract_molecule *reacB,
  struct abstract_molecule *reacC, short orientA, short orientB, 
  short orientC, double t,struct vector3 *hitpt,struct vector3 *loc_okay);
int outcome_intersect(struct rxn *rx, int path, struct wall *surface,
  struct abstract_molecule *reac,short orient,double t,struct vector3 *hitpt,
  struct vector3 *loc_okay);
int reaction_wizardry(struct magic_list *incantation,struct wall *surface,struct vector3 *hitpt,double t);
int is_compatible_surface(void *req_species, struct wall *w);

#endif
