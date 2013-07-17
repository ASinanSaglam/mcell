#ifndef MCELL_REACT
#define MCELL_REACT

#include "mcell_structs.h"
#include <stdbool.h>

/* In react_trig.c */
struct rxn* trigger_unimolecular(u_int hash,struct abstract_molecule *reac);
int trigger_surface_unimol(struct abstract_molecule *reac,struct wall *w, struct rxn **matching_rxns);
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
int trigger_intersect(u_int hashA,struct abstract_molecule *reacA,
  short orientA,struct wall *w, struct rxn **matching_rxns, int allow_rx_transp,  int allow_rx_reflec, int allow_rx_absorb_reg_border);

int check_for_unimolecular_reaction(struct abstract_molecule* mol);

struct rxn* pick_unimolecular_reaction(struct abstract_molecule* a);

int find_unimol_reactions_with_surf_classes(struct abstract_molecule* reacA,
                                            struct wall* w,
                                            u_int hashA,
                                            int orientA,
                                            int num_matching_rxns,
                                            int allow_rx_transp,
                                            int allow_rx_reflec,
                                            int allow_rx_absorb_reg_border,
                                            struct rxn **matching_rxns);


int find_surface_mol_reactions_with_surf_classes(int orientA,
                                            struct species *scl,  
                                            int num_matching_rxns,
                                            int allow_rx_transp,
                                            int allow_rx_reflec,
                                            int allow_rx_absorb_reg_border,
                                            struct rxn **matching_rxns);

int find_volume_mol_reactions_with_surf_classes(int orientA,
                                            struct species *scl,  
                                            int num_matching_rxns,
                                            int allow_rx_transp,
                                            int allow_rx_reflec,
                                            int allow_rx_absorb_reg_border,
                                            struct rxn **matching_rxns);


/* In react_cond.c */
double timeof_unimolecular(struct rxn *rx, struct abstract_molecule *a);
int which_unimolecular(struct rxn *rx, struct abstract_molecule *a);
int test_bimolecular(struct rxn *rx, double scaling, double local_prob_factor, struct abstract_molecule *a1, struct abstract_molecule *a2);
int test_many_bimolecular(struct rxn **rx, double *scaling, int n, int *chosen_pathway, struct abstract_molecule **complexes, int *complex_limits);
int test_many_bimolecular_all_neighbors(struct rxn **rx, double *scaling, double local_prob_factor, int n, int *chosen_pathway, struct abstract_molecule **complexes, int *complex_limits);
int test_many_reactions_all_neighbors(struct rxn **rx, double *scaling, double *local_prob_factor, int n, int *chosen_pathway);
int test_intersect(struct rxn *rx,double scaling);
int test_many_intersect(struct rxn **rx,double scaling, int n, int *chosen_pathway);
struct rxn * test_many_unimol(struct rxn **rx, int n, struct abstract_molecule *a);
void update_probs(struct rxn *rx,double t);


/* In react_outc.c */
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
int is_compatible_surface(void *req_species, struct wall *w);


/* ALL_INSIDE: flag that indicates that all reactants lie inside their
 *             respective restrictive regions 
 * ALL_OUTSIDE: flag that indicates that all reactants lie outside 
 *              their respective restrictive regions 
 * GRID1_IN_GRID2_OUT: flag that indicates that  reactant "grid_1" lies 
 *                     inside and reactant "grid_2" lies outside of their 
 *                     respective restrictive regions 
 * GRID1_OUT_GRID2_IN: flag that indicates that  reactant "grid_1" lies outside
 *                     and reactant "grid_2" lies inside of their 
 *                     respective restrictive regions 
 * GRID1_IN: flag that indicates that only reactant "grid_1" has
 *          restrictive regions on the object and it lie s
 *          inside it's restrictive region.  
 * GRID1_OUT: flag that indicates that only reactant "grid_1" has
 *            restrictive regions on the object and it lies
 *            outside it's restrictive region. 
 * GRID2_IN: flag that indicates that only reactant "grid_2" has
 *           restrictive regions on the object and it lies
 *           inside it's restrictive region.  
 * GRID2_OUT: flag that indicates that only reactant "grid_2" has
 *            restrictive regions on the object and it lies
 *            outside it's restrictive region.  */
#define ALL_INSIDE 0x01
#define ALL_OUTSIDE 0x02
#define GRID1_IN_GRID2_OUT 0x04
#define GRID1_OUT_GRID2_IN 0x08
#define GRID1_IN 0x10
#define GRID1_OUT 0x20 
#define GRID2_IN 0x40
#define GRID2_OUT 0x80


int determine_molecule_region_topology(struct grid_molecule *grid_1,
                                       struct grid_molecule *grid_2,
                                       struct region_list **rlp_wall_1_ptr,
                                       struct region_list **rlp_wall_2_ptr,
                                       struct region_list **rlp_obj_1_ptr,
                                       struct region_list **rlp_obj_2_ptr,
                                       bool is_unimol);

bool product_tile_can_be_reached(struct wall *target, 
                                 struct region_list *rlp_head_wall_1,
                                 struct region_list *rlp_head_wall_2,
                                 struct region_list *rlp_head_obj_1,
                                 struct region_list *rlp_head_obj_2,
                                 int grid_bitmask,
                                 bool is_unimol);


#endif
