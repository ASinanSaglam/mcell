/**************************************************************************\
** File: react_cond.c                                                     **
**                                                                        **
** Purpose: Determines whether or not (or when) a reaction occurs         **
**                                                                        **
** Testing status: partially validated (see validate_react_cond.c)        **
\**************************************************************************/


#include <math.h>

#include "rng.h"
#include "mcell_structs.h"

extern struct volume *world;


/*************************************************************************
test_unimolecular:
  In: the reaction we're testing
  Out: -1 if no reaction occurs in one timestep
       int containing the number of the outward pathway if it does
*************************************************************************/

int test_unimolecular(struct rxn *rx)
{
  int m,M,avg;
  double p = rng_double( world->seed++ );
  
  m = 0;
  M = rx->n_pathways-1;
  if (p > rx->cum_rates[ rx->n_pathways-1 ]) return -1;

  while (M-m > 1)
  {
    avg = (M+m)/2;
    if (p > rx->cum_rates[avg]) m = avg;
    else M = avg;
  }
  
  if (m==M) return m;
  if (p > rx->cum_rates[m]) return M;
  else return m;
}


/*************************************************************************
timeof_unimolecular:
  In: the reaction we're testing
  Out: double containing the number of timesteps until the reaction occurs
*************************************************************************/

double timeof_unimolecular(struct rxn *rx)
{
  double p = rng_double( world->seed++ );
  double k_tot = rx->cum_rates[ rx->n_pathways - 1 ];
  return -log( p )/k_tot;
}


/*************************************************************************
which_unimolecular:
  In: the reaction we're testing
  Out: int containing which unimolecular reaction occurs (one must occur)
*************************************************************************/

int which_unimolecular(struct rxn *rx)
{
  int m,M,avg;
  double p = rng_double( world->seed++ );
  
  m = 0;
  M = rx->n_pathways-1;
  
  p = p * rx->cum_rates[ rx->n_pathways-1 ];
  
  while (M-m > 1)
  {
    avg = (M+m)/2;
    if (p > rx->cum_rates[avg]) m = avg;
    else M = avg;
  }
  
  if (m==M) return m;
  if (p > rx->cum_rates[m]) return M;
  else return m;
}


/*************************************************************************
test_bimolecular
  In: the reaction we're testing
      a probability multiplier depending on how many timesteps we've
        moved at once (1.0 means one timestep) and/or missing interaction area
  Out: -1 if no reaction occurs
       int containing which reaction occurs if one does occur
*************************************************************************/

int test_bimolecular(struct rxn *rx,double time_mult)
{
  int m,M,avg;
  double p = rng_double( world->seed++ ) / time_mult;  /* FIXME: convert to use multiples */
  
  if ( p > rx->cum_rates[ rx->n_pathways-1 ] ) return -1;
  
  m = 0;
  M = rx->n_pathways-1;
  
  if ( p > rx->cum_rates[M] )
  {
    printf("BROKEN!!!\n");
    return -1;
  }
  
  while (M-m > 1)
  {
    avg = (M+m)/2;
    if (p > rx->cum_rates[avg]) m = avg;
    else M = avg;
  }
  
  if (m==M) return m;
  if (p > rx->cum_rates[m]) return M;
  else return m;
}


/*************************************************************************
test_intersect
  In: the reaction we're testing
      a probability multiplier depending on how many timesteps we've
        moved at once (1.0 means one timestep)
  Out: -1 if no reaction occurs
       int containing which reaction occurs if one does occur
       0 if the reaction is a reflection that always occurs
         (this is checked for first)
*************************************************************************/

int test_intersect(struct rxn *rx,double time_mult)
{
  int m,M,avg;
  double p;
  
  if (rx->cum_rates[0] >= 1.0) return 0;  /* Shortcut for reflections */
  
  p = rng_double( world->seed++ ) / time_mult;
  
  if ( p > rx->cum_rates[ rx->n_pathways-1 ] ) return -1;

  m = 0;
  M = rx->n_pathways-1;
  
  if ( p > rx->cum_rates[M] ) return -1;
  
  while (M-m > 1)
  {
    avg = (M+m)/2;
    if (p > rx->cum_rates[avg]) m = avg;
    else M = avg;
  }
  
  if (m==M) return m;
  if (p > rx->cum_rates[m]) return M;
  else return m;
}


/*************************************************************************
check_rates:
  In: A reaction struct
      The current time
  Out: No return value.  Rates are updated if necessary.
       Memory isn't reclaimed.
  Note: This isn't meant for really heavy-duty use (multiple pathways
        with rapidly changing rates)--if you want that, the code should
        probably be rewritten to accumulate probability changes from the
        list as it goes (and the list should be sorted by pathway, too).
*************************************************************************/

void check_rates(struct rxn *rx,double t)
{
  int j,k;
  double dprob;
  struct t_func *tv;
  int did_something = 0;
  
  for ( tv = rx->rate_t ; tv!= NULL && tv->time < t ; tv = tv->next )
  {
    j = tv->path;
    if (j == 0) dprob = tv->value - rx->cum_rates[0];
    else dprob = tv->value - (rx->cum_rates[j]-rx->cum_rates[j-1]);

    for (k = tv->path ; k < rx->n_pathways ; k++) rx->cum_rates[k] += dprob;
    did_something++;
  }
  
  rx->rate_t = tv;
  
  if (!did_something) return;
  
  if (rx->n_reactants==1)
  {
    printf("Rate %.4e set for %s[%d] -> ",rx->cum_rates[0],
           rx->players[0]->sym->name,rx->geometries[0]);

    for (k = rx->product_idx[0] ; k < rx->product_idx[1] ; k++)
    {
      if (rx->players[k]==NULL) printf("NIL ");
      else printf("%s[%d] ",rx->players[k]->sym->name,rx->geometries[k]);
    }
    printf("\n");
  }
  else
  {
    printf("Rate %.4e (s) set for %s[%d] + %s[%d] -> ",rx->cum_rates[0],
           rx->players[0]->sym->name,rx->geometries[0],
           rx->players[1]->sym->name,rx->geometries[1]);
    for (k = rx->product_idx[0] ; k < rx->product_idx[1] ; k++)
    {
      if (rx->players[k]==NULL) printf("NIL ");
      else printf("%s[%d] ",rx->players[k]->sym->name,rx->geometries[k]);
    }
    printf("\n");
  }
}
