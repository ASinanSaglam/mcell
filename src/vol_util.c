/**************************************************************************\
** File: vol_util.c                                                       **
**                                                                        **
** Purpose: Adds, subtracts, and moves particles around (bookkeeping).    **
**                                                                        **
** Testing status: compiles.  Worked earlier, but has been changed.       **
\**************************************************************************/

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "rng.h"
#include "mem_util.h"
#include "count_util.h"
#include "mcell_structs.h"
#include "vol_util.h"
#include "react.h"
#include "react_output.h"
#include "util.h"
#include "wall_util.h"

extern struct volume *world;


/*************************************************************************
inside_subvolume:
  In: pointer to vector3
      pointer to subvolume
  Out: nonzero if the vector is inside the subvolume.
*************************************************************************/

int inside_subvolume(struct vector3 *point,struct subvolume *subvol)
{
  return ( (point->x >= world->x_fineparts[ subvol->llf.x ] ) &&
           (point->x <= world->x_fineparts[ subvol->urb.x ] ) &&
           (point->y >= world->y_fineparts[ subvol->llf.y ] ) &&
           (point->y <= world->y_fineparts[ subvol->urb.y ] ) &&
           (point->z >= world->z_fineparts[ subvol->llf.z ] ) &&
           (point->z <= world->z_fineparts[ subvol->urb.z ] ) );
}


/*************************************************************************
find_course_subvolume:
  In: pointer to vector3
  Out: pointer to the course subvolume that the vector is within
*************************************************************************/

struct subvolume* find_course_subvol(struct vector3 *loc)
{
  int i,j,k;
  i = bisect(world->x_partitions,world->nx_parts,loc->x);
  j = bisect(world->y_partitions,world->ny_parts,loc->y);
  k = bisect(world->z_partitions,world->nz_parts,loc->z);
  return 
    &( world->subvol
      [
        k + (world->nz_parts-1)*(j + (world->ny_parts-1)*i)
      ]
    );
}


/*************************************************************************
traverse_subvol:
  In: pointer to our current subvolume
      pointer to a vector3 of where we want to be
      which direction we're traveling to get there
  Out: subvolume that's closest to where we want to be in our direction
  Note: have to traverse BSP trees to do this
*************************************************************************/

struct subvolume* traverse_subvol(struct subvolume *here,struct vector3 *point,int which)
{
  int flag = 1<<which;
  int left_path;
  struct bsp_tree *branch;
  
  if ((here->is_bsp & flag) == 0) return (struct subvolume*)here->neighbor[which];
  else
  {
    printf("I am here\n");
    branch = (struct bsp_tree*) here->neighbor[which];
    while (branch != NULL)
    {
      if ( (branch->flags & X_AXIS) != 0 )
      {
        if ( point->x <= world->x_fineparts[ branch->partition ] ) left_path = 1;
        else left_path = 0;
      }
      else
      {
        if ( (branch->flags & Y_AXIS) != 0 )
        {
          if ( point->y <= world->y_fineparts[ branch->partition ] ) left_path = 1;
          else left_path = 0;
        }
        else /* Must be Z_AXIS */
        {
          if ( point->z <= world->z_fineparts[ branch->partition ] ) left_path = 1;
          else left_path = 0;
        }
      }
      if (left_path)
      {
        if ((branch->flags & BRANCH_L) == 0) return (struct subvolume*) branch->left;
        else branch = (struct bsp_tree*) branch->left;
      }
      else
      {
        if ((branch->flags & BRANCH_R) == 0) return (struct subvolume*) branch->right;
        else branch = (struct bsp_tree*) branch->right;
      }
    }
  }
  
  return NULL;
}



/*************************************************************************
collide_sv_time:
  In: pointer to a vector3 of where we are (*here)
      pointer to a vector3 of where we want to be
      our current subvolume
  Out: time to hit the closest wall of the subvolume
*************************************************************************/

double collide_sv_time(struct vector3 *here,struct vector3 *move,struct subvolume *sv)
{
  double dx,dy,dz,tx,ty,tz,t;
  int whichx,whichy,whichz,which;
  
  whichx = whichy = whichz = 1;
  
  if (move->x > 0) dx = world->x_fineparts[ sv->urb.x ] - here->x;
  else { dx = world->x_fineparts[ sv->llf.x ] - here->x; whichx = 0; }
  
  if (move->y > 0) dy = world->y_fineparts[ sv->urb.y ] - here->y;
  else { dy = world->y_fineparts[ sv->llf.y ] - here->y; whichy = 0; }
  
  if (move->z > 0) dz = world->z_fineparts[ sv->urb.z ] - here->z;
  else { dz = world->z_fineparts[ sv->llf.z ] - here->z; whichz = 0; }
  
  tx = dx * move->y * move->z; if (tx<0) tx = -tx;
  ty = move->x * dy * move->z; if (ty<0) ty = -ty;
  tz = move->x * move->y * dz; if (tz<0) tz = -tz;
  
  if (tx<ty || move->y==0.0)
  {
    if (tx<tz || move->z==0.0) { t = dx / move->x; which = X_NEG + whichx; }
    else { t = dz / move->z; which = Z_NEG + whichz; }
  }
  else /* ty<tx */
  {
    if (ty<tz || move->z==0.0) { t = dy / move->y; which = Y_NEG + whichy; }
    else { t = dz / move->z; which = Z_NEG + whichz; }
  }
  
  return t;
}



/*************************************************************************
next_subvol:
  In: pointer to a vector3 of where we are (*here)
      pointer to a vector3 of where we want to be
      our current subvolume
  Out: next subvolume along that vector or NULL if the endpoint is 
         in the current subvolume.  *here is updated to just inside
         the next subvolume.
*************************************************************************/

struct subvolume* next_subvol(struct vector3 *here,struct vector3 *move,struct subvolume *sv)
{
  double dx,dy,dz,tx,ty,tz,t;
  int whichx,whichy,whichz,which;
  
  whichx = whichy = whichz = 1;
  
  if (move->x > 0) dx = world->x_fineparts[ sv->urb.x ] - here->x;
  else { dx = world->x_fineparts[ sv->llf.x ] - here->x; whichx = 0; }
  
  if (move->y > 0) dy = world->y_fineparts[ sv->urb.y ] - here->y;
  else { dy = world->y_fineparts[ sv->llf.y ] - here->y; whichy = 0; }
  
  if (move->z > 0) dz = world->z_fineparts[ sv->urb.z ] - here->z;
  else { dz = world->z_fineparts[ sv->llf.z ] - here->z; whichz = 0; }
  
  tx = dx * move->y * move->z; if (tx<0) tx = -tx;
  ty = move->x * dy * move->z; if (ty<0) ty = -ty;
  tz = move->x * move->y * dz; if (tz<0) tz = -tz;
  
  if (tx<ty || move->y==0.0)
  {
    if (tx<tz || move->z==0.0) { t = dx / move->x; which = X_NEG + whichx; }
    else { t = dz / move->z; which = Z_NEG + whichz; }
  }
  else /* ty<tx */
  {
    if (ty<tz || move->z==0.0) { t = dy / move->y; which = Y_NEG + whichy; }
    else { t = dz / move->z; which = Z_NEG + whichz; }
  }
      
  if (t>=1.0)
  {
    here->x += move->x;
    here->y += move->y;
    here->z += move->z;
    
    return NULL;
  }
  else
  {
    here->x += t*move->x;
    here->y += t*move->y;
    here->z += t*move->z;
    
    t = 1.0-t;
    
    move->x *= t;
    move->y *= t;
    move->z *= t;
    
    return traverse_subvol(sv,here,which);
  }
}
  


/*************************************************************************
find_subvolume:
  In: pointer to a vector3 of where we are
      pointer to a subvolume we might be in or near
  Out: subvolume that we are in
*************************************************************************/

struct subvolume* find_subvolume(struct vector3 *loc,struct subvolume *guess)
{
  struct subvolume *sv;
  struct vector3 center;
  
  if (guess == NULL) sv = find_course_subvol(loc);
  else sv = guess;
  
  center.x = 0.5*(world->x_fineparts[ sv->llf.x ] + world->x_fineparts[ sv->urb.x ]);
  center.y = 0.5*(world->y_fineparts[ sv->llf.y ] + world->y_fineparts[ sv->urb.y ]);
  center.z = 0.5*(world->z_fineparts[ sv->llf.z ] + world->z_fineparts[ sv->urb.z ]);
  
  while (loc->x < world->x_fineparts[ sv->llf.x ] )
  {
    sv = traverse_subvol(sv , &center , X_NEG);
    center.x = 0.5*(world->x_fineparts[ sv->llf.x ] + world->x_fineparts[ sv->urb.x ]);
  }
  while (loc->x > world->x_fineparts[ sv->urb.x ] )
  {
    sv = traverse_subvol(sv , &center , X_POS);
    center.x = 0.5*(world->x_fineparts[ sv->llf.x ] + world->x_fineparts[ sv->urb.x ]);
  }
  center.x = loc->x;
  
  while (loc->y < world->y_fineparts[ sv->llf.y ] )
  {
    sv = traverse_subvol(sv , &center , Y_NEG);
    center.y = 0.5*(world->y_fineparts[ sv->llf.y ] + world->y_fineparts[ sv->urb.y ]);
  }
  while (loc->y > world->y_fineparts[ sv->urb.y ] )
  {
    sv = traverse_subvol(sv , &center , Y_POS);
    center.y = 0.5*(world->y_fineparts[ sv->llf.y ] + world->y_fineparts[ sv->urb.y ]);
  }
  center.y = loc->y;

  while (loc->z < world->z_fineparts[ sv->llf.z ] )
  {
    sv = traverse_subvol(sv , &center , Z_NEG);
    center.z = 0.5*(world->z_fineparts[ sv->llf.z ] + world->z_fineparts[ sv->urb.z ]);
  }
  while (loc->z > world->z_fineparts[ sv->urb.z ] )
  {
    sv = traverse_subvol(sv , &center , Z_POS);
    center.z = 0.5*(world->z_fineparts[ sv->llf.z ] + world->z_fineparts[ sv->urb.z ]);
  }
  center.z = loc->z;
  
  return sv;
}  


/*************************************************************************
insert_molecule
  In: pointer to a molecule that we're going to place in local storage
      pointer to a molecule that may be nearby
  Out: pointer to the new molecule (copies data from molecule passed in),
       or NULL if out of memory
*************************************************************************/

struct molecule* insert_molecule(struct molecule *m,struct molecule *guess)
{
  struct molecule *new_m;
  struct subvolume *sv;
  
  if (guess == NULL) sv = find_subvolume(&(m->pos),NULL);
  else if ( inside_subvolume(&(m->pos),guess->subvol) ) sv = guess->subvol;
  else sv = find_subvolume(&(m->pos),guess->subvol);
  
  new_m = mem_get(sv->local_storage->mol);
  if(new_m == NULL) {
	fprintf(stderr, "Out of memory:trying to save intermediate results.\n");        int i = emergency_output();
        fprintf(stderr, "Fatal error: out of memory during inserting %s molecule.\nAttempt to write intermediate results had %d errors.\n", m->properties->sym->name, i);
        exit(EXIT_FAILURE);
  }

  memcpy(new_m,m,sizeof(struct molecule));

  new_m->birthplace = sv->local_storage->mol;
  new_m->next = NULL;
  new_m->subvol = sv;
  new_m->next_v = sv->mol_head;
  sv->mol_head = new_m;
  sv->mol_count++;
  new_m->properties->population++;

  if (new_m->properties->flags & COUNT_CONTENTS)
  {
    count_me_by_region( (struct abstract_molecule*)new_m , 1 , NULL );
  }
  
  if ( schedule_add(sv->local_storage->timer,new_m) ) {
	fprintf(stderr, "Out of memory:trying to save intermediate results.\n");
        int i = emergency_output();
        fprintf(stderr, "Fatal error: out of memory during inserting %s molecule.\nAttempt to write intermediate results had %d errors.\n", m->properties->sym->name, i);
        exit(EXIT_FAILURE);

  } 
  return new_m;
}


/*************************************************************************
excert_molecule:
  In: pointer to a molecule that we're going to remove from local storage
  Out: no return value; molecule is marked for removal.
*************************************************************************/

void excert_molecule(struct molecule *m)
{
  if (m->properties->flags & COUNT_CONTENTS)
  {
    count_me_by_region( (struct abstract_molecule*)m , -1 , NULL );
  }
  m->subvol->mol_count--;
  m->properties->n_deceased++;
  m->properties->cum_lifetime += m->t - m->birthday;
  m->properties->population--;
  m->properties = NULL;
}


/*************************************************************************
insert_molecule_list:
  In: pointer to a linked list of molecules to copy into subvolumes.
  Out: 0 on success, 1 on memory allocation error; molecules are placed
       in their subvolumes.
*************************************************************************/

int insert_molecule_list(struct molecule *m)
{
  struct molecule *new_m,*guess;
  
  guess=NULL;
  while (m != NULL)
  {
    new_m = insert_molecule(m,guess);
    if(new_m == NULL) { 
	fprintf(stderr, "Out of memory:trying to save intermediate results.\n");
        int i = emergency_output();
        fprintf(stderr, "Fatal error: out of memory during inserting %s molecule.\nAttempt to write intermediate results had %d errors.\n", m->properties->sym->name, i);
        exit(EXIT_FAILURE);
    }
    guess = new_m;
    m = (struct molecule*)m->next;
  }
  
  return 0;
}


/*************************************************************************
migrate_molecule:
  In: pointer to a molecule already in a subvolume
      pointer to the new subvolume to move it to
  Out: pointer to moved molecule.  The molecule's position is updated
       but it is not rescheduled.  Returns NULL if out of memory.
*************************************************************************/

struct molecule* migrate_molecule(struct molecule *m,struct subvolume *new_sv)
{
  struct molecule *new_m;

  new_m = mem_get(new_sv->local_storage->mol);
  if (new_m==NULL){ 
	fprintf(stderr, "Out of memory:trying to save intermediate results.\n");        int i = emergency_output();
        fprintf(stderr, "Fatal error: out of memory during migrating  %s molecule.\nAttempt to write intermediate results had %d errors.\n", m->properties->sym->name, i);
        exit(EXIT_FAILURE);
  }
  if (new_m==m) printf("Unsane!\n");
  
  memcpy(new_m,m,sizeof(struct molecule));
  new_m->birthplace = new_sv->local_storage->mol;
  
  new_m->next = NULL;
  new_m->subvol = new_sv;
  new_m->next_v = new_sv->mol_head;
  new_sv->mol_head = new_m;
  new_sv->mol_count++;
 
  m->subvol->mol_count--;
  m->properties = NULL;

  return new_m;    
}



int eval_rel_region_3d(struct release_evaluator *expr,struct waypoint *wp,struct region_list *in_regions,struct region_list *out_regions)
{
  struct region *r;
  struct region_list *rl;
  int found_l,found_r;
  
  found_l=0;
  if (expr->op & REXP_LEFT_REGION)
  {
    r = (struct region*)expr->left;
    for (rl=wp->regions ; rl!=NULL ; rl=rl->next)
    {
      if (rl->reg == r)
      {
        found_l=1;
        break;
      }
    }
    if (found_l)
    {
      for (rl=out_regions ; rl!=NULL ; rl=rl->next)
      {
        if (rl->reg==r)
        {
          found_l=0;
          break;
        }
      }
    }
    else
    {
      for (rl=in_regions ; rl!=NULL ; rl=rl->next)
      {
        if (rl->reg==r)
        {
          found_l=1;
          break;
        }
      }
    }
  }
  else found_l = eval_rel_region_3d(expr->left,wp,in_regions,out_regions);
  
  if (expr->op & REXP_NO_OP) return found_l;
  
  found_r=0;
  if (expr->op & REXP_RIGHT_REGION)
  {
    r = (struct region*)expr->right;
    for (rl=wp->regions ; rl!=NULL ; rl=rl->next)
    {
      if (rl->reg == r)
      {
        found_r=1;
        break;
      }
    }
    if (found_r)
    {
      for (rl=out_regions ; rl!=NULL ; rl=rl->next)
      {
        if (rl->reg==r)
        {
          found_r=0;
          break;
        }
      }
    }
    else
    {
      for (rl=in_regions ; rl!=NULL ; rl=rl->next)
      {
        if (rl->reg==r)
        {
          found_r=1;
          break;
        }
      }
    }
  }
  else found_r = eval_rel_region_3d(expr->right,wp,in_regions,out_regions);
  
  if (expr->op & REXP_UNION) return (found_l || found_r);
  else if (expr->op & REXP_INTERSECTION) return (found_l && found_r);
  else if (expr->op & REXP_SUBTRACTION) return (found_l && !found_r);

  return 0;
}


int release_inside_regions(struct release_site_obj *rso,struct molecule *m,int n)
{
  struct molecule *new_m;
  struct release_region_data *rrd;
  struct region_list *extra_in,*extra_out;
  struct region_list *rl,*rl2;
  struct subvolume *sv = NULL;
  struct wall_list *wl;
  struct vector3 delta;
  struct vector3 *origin;
  struct waypoint *wp;
  double t;
  struct vector3 hit;
  int bad_location;
  int i;
  
  rrd = rso->region_data;
  new_m = NULL;
  m->curr_cmprt = NULL;
  m->previous_grid = NULL;
  m->index = -1;
  
  while (n>0)
  {
    m->pos.x = rrd->llf.x + (rrd->urb.x-rrd->llf.x)*rng_dbl(world->rng);
    m->pos.y = rrd->llf.y + (rrd->urb.y-rrd->llf.y)*rng_dbl(world->rng);
    m->pos.z = rrd->llf.z + (rrd->urb.z-rrd->llf.z)*rng_dbl(world->rng);
    
    if (sv == NULL) sv = find_subvolume(&(m->pos),NULL);
    else if ( !inside_subvolume(&(m->pos),sv) )
    {
      sv = find_subvolume(&(m->pos),sv);
    }
    
    extra_in=extra_out=NULL;
    wp = &(world->waypoints[sv->index]);
    origin = &(wp->loc);
    delta.x = m->pos.x - origin->x;
    delta.y = m->pos.y - origin->y;
    delta.z = m->pos.z - origin->z;
    
    bad_location = 0;
    
    for (wl=sv->wall_head ; wl!=NULL ; wl=wl->next)
    {
      i = collide_wall(origin,&delta,wl->this_wall,&t,&hit);
      
      if (i!=COLLIDE_MISS)
      {
        if ( (t>-EPS_C && t<EPS_C) || (t>1.0-EPS_C && t<1.0+EPS_C) )
        {
          bad_location = 1;
          break;
        }
        for (rl=wl->this_wall->regions ; rl!=NULL ; rl=rl->next)
        {
          rl2 = (struct region_list*)mem_get(sv->local_storage->regl);
          rl2->reg = rl->reg;
          
          if (i==COLLIDE_FRONT)
          {
            rl2->next = extra_in;
            extra_in = rl2;
          }
          else if (i==COLLIDE_BACK)
          {
            rl2->next = extra_out;
            extra_out = rl2;
          }
          else
          {
            bad_location = 1;
            break;
          }
        }
      }
    }
    if (bad_location) continue;
    
    for (rl=extra_in ; rl!=NULL ; rl=rl->next)
    {
      if (rl->reg==NULL) continue;
      for (rl2=extra_out ; rl2!=NULL ; rl2=rl2->next)
      {
        if (rl2->reg==NULL) continue;
        if (rl->reg==rl2->reg)
        {
          rl->reg = NULL;
          rl2->reg = NULL;
          break;
        }
      }
    }

    i = eval_rel_region_3d(rrd->expression,wp,extra_in,extra_out);
    if (!i) continue;
    
    m->subvol = sv;
    new_m =  insert_molecule(m,new_m);
    
    if (new_m==NULL) return 1;
    
    n--;
  }
  
  return 0;
}


/*************************************************************************
release_molecules:
  In: pointer to a release event
  Out: 0 on success, 1 on failure; next event is scheduled and molecule(s)
       are released into the world as specified.
*************************************************************************/
int release_molecules(struct release_event_queue *req)
{
  struct release_site_obj *rso;
  struct release_pattern *rpat;
  struct molecule m;
  struct grid_molecule g;
  struct abstract_molecule *ap;
  struct molecule *mp;
  struct grid_molecule *gp;
  struct molecule *guess;
  int i,number;
  struct vector3 *diam_xyz;
  struct vector3 pos;
  double diam,vol;
  double t,k;
  
  if(req == NULL) return 0;
  rso = req->release_site;
  rpat = rso->pattern;
 
  if ((rso->mol_type->flags & NOT_FREE) == 0)
  {
    mp = &m;  /* Avoid strict-aliasing message */
    ap = (struct abstract_molecule*)mp;
    ap->flags = TYPE_3D | IN_VOLUME;
  }
  else
  {
    gp = &g;  /* Avoid strict-aliasing message */
    ap = (struct abstract_molecule*)gp;
    ap->flags = TYPE_GRID | IN_SURFACE;
  }
  ap->flags |= IN_SCHEDULE + ACT_NEWBIE;

  if(req->train_counter == 0)
  {
	req->train_counter++;
  }
  
  guess = NULL;
 
  /* Set molecule characteristics. */
  ap->t = req->event_time;
  ap->properties = rso->mol_type;
  ap->t2 = 0.0;
  ap->birthday = ap->t;
  
  if (trigger_unimolecular(rso->mol_type->hashval , ap) != NULL) ap->flags += ACT_REACT;
  if (rso->mol_type->space_step > 0.0) ap->flags += ACT_DIFFUSE;
  
  switch(rso->release_number_method)
  {
    case CONSTNUM:
      number = rso->release_number;
      break;
    case GAUSSNUM:
      if (rso->standard_deviation > 0)
      {
	number = (int) rng_gauss(world->rng)*rso->standard_deviation + rso->release_number;
      }
      else
      {
        rso->release_number_method = CONSTNUM;
        number = rso->release_number;
      }
      break;
    case VOLNUM:
      if (rso->standard_deviation > 0)
      {
	diam = rng_gauss(world->rng)*rso->standard_deviation;
      }
      else
      {
        diam = rso->mean_diameter;
      }
      switch (rso->release_shape)
      {
        case SHAPE_SPHERICAL:
          vol = MY_PI * diam*diam;
          number = (int) (world->effector_grid_density * rso->concentration * vol);
          break;
        case SHAPE_SPHERICAL_SHELL:
          vol = (MY_PI/6.0) * diam*diam*diam;
          number = (int) (N_AV * 1e-15 * rso->concentration * vol);
          break;
        case SHAPE_RECTANGULAR:
          vol = diam*diam*diam;
          number = (int) (N_AV * 1e-15 * rso->concentration * vol);
          break;
        default:
          vol = 0;
          number = 0;
          break;
      }
      number = (int) (N_AV * 1e-15 * rso->concentration * vol);
      break;
   default:
     number = 0;
     break;
  }
  
  if (rso->release_shape == SHAPE_REGION)
  {
    if (ap->flags & TYPE_3D)
    {
      i = release_inside_regions(rso,(struct molecule*)ap,number);
      if (i) return 1;
      fprintf(world->log_file, "Releasing type = %s\n", req->release_site->mol_type->sym->name);
    }
    else
    {
      i = release_onto_regions(rso,(struct grid_molecule*)ap,number);
      if (i) return 1;
      fprintf(world->log_file, "Releasing type = %s\n", req->release_site->mol_type->sym->name);
    }
  }
  else  /* Guaranteed to be 3D molecule */
  {
    m.curr_cmprt = NULL;
    m.previous_grid = NULL;
    m.index = -1;
    
    diam_xyz = rso->diameter;
    if (diam_xyz != NULL)
    {
      for (i=0;i<number;i++)
      {
        do /* Pick values in unit square, toss if not in unit circle */
        {
          pos.x = (rng_dbl(world->rng)-0.5);
          pos.y = (rng_dbl(world->rng)-0.5);
          pos.z = (rng_dbl(world->rng)-0.5);
        } while ( (rso->release_shape == SHAPE_SPHERICAL || rso->release_shape == SHAPE_ELLIPTIC || rso->release_shape == SHAPE_SPHERICAL_SHELL)
                  && pos.x*pos.x + pos.y*pos.y + pos.z*pos.z >= 0.25 );
        
        if (rso->release_shape == SHAPE_SPHERICAL_SHELL)
        {
          double r;
          r = sqrt( pos.x*pos.x + pos.y*pos.y + pos.z*pos.z)*2.0;
          if (r==0.0) { pos.x = 0.0; pos.y = 0.0; pos.z = 0.5; }
          else { pos.x /= r; pos.y /= r; pos.z /= r; }
        }
        
        m.pos.x = pos.x*diam_xyz->x + req->location.x;
        m.pos.y = pos.y*diam_xyz->y + req->location.y;
        m.pos.z = pos.z*diam_xyz->z + req->location.z;
        
        guess = insert_molecule(&m,guess);  /* Insert copy of m into world */
        if (guess == NULL) return 1;
      }
      fprintf(world->log_file, "Releasing type = %s\n", req->release_site->mol_type->sym->name);
    }
    else
    {
      m.pos.x = req->location.x;
      m.pos.y = req->location.y;
      m.pos.z = req->location.z;
      
      for (i=0;i<number;i++)
      {
         guess = insert_molecule(&m,guess);
         if (guess == NULL) return 1;
      }
      fprintf(world->log_file, "Releasing type = %s\n", req->release_site->mol_type->sym->name);
    }
  }
 
  /* Exit if no more releases should be scheduled. */
  if(req->train_counter == rpat->number_of_trains)
  {
      if((rpat->release_interval == 0) ||
         (req->event_time + EPSILON > req->train_high_time + rpat->train_duration))
      {
            return 0;
      }
  }
  

  /* Otherwise schedule next release event. */
  if(rpat->release_interval > 0)
  {
    if (rso->release_prob < 1.0)
    {
      k = -log( 1.0 - rso->release_prob );
      t = -log( rng_dbl(world->rng) ) / k;  /* Poisson dist. */
      req->event_time += rpat->release_interval * (ceil(t)-1.0); /* Rounded to integers */
    }else{
  	req->event_time += rpat->release_interval;
    }
  }
    /* we may need to move to the next train. */
  if (req->event_time > req->train_high_time + rpat->train_duration)
  {
      req->train_high_time += rpat->train_interval;
      req->event_time = req->train_high_time;
      req->train_counter++;
  }
    
  if (req->train_counter <= rpat->number_of_trains)
  {
      if ( schedule_add(world->releaser,req) ){
	fprintf(stderr, "Out of memory:trying to save intermediate results.\n");
        int i = emergency_output();
        fprintf(stderr, "Fatal error: out of memory during release molecule event.\nAttempt to write intermediate results had %d errors.\n", i);
        exit(EXIT_FAILURE);
      } 
  }

 
  return 0;
}

/*************************************************************************
find_exponential_params:
  In: value of f(0)
      value of f(N)
      difference between f(1) and f(0)
      number of data points
      pointer to where we store the scaling factor A
      pointer to the constant offset B
      pointer to the rate of decay k
  Out: no return value.  This is a utility function that uses bisection
       to solve for A,B,k to find an exponentially increasing function
         f(n) = A*exp(n*k)+B
       subject to the contstraints
         f(0) = c
         f(1) = c+d
         f(N) = C
*************************************************************************/

void find_exponential_params(double c,double C,double d,double N,double *A,double *B, double *k)
{
  double k_min,k_max,k_mid,f;
  int i;
  
  k_min = 0;
  k_max = log(GIGANTIC)/N;
  for (i=0;i<720;i++)
  {
    k_mid = 0.5*(k_min + k_max);
    f = c + (exp(N*k_mid)-1.0)*d/(exp(k_mid)-1.0);
    if (C > f) k_min = k_mid;
    else k_max = k_mid;
    if ((k_max-k_min)/(k_max+k_min) < EPS_C) break;
  }
  
  *k = k_mid;
  *A = d / ( exp(*k) - 1.0 );
  *B = c - *A;
}


/*************************************************************************
set_partitions:
  In: nothing.  Uses struct volume *world, assumes bounding box is set.
  Out: 0 on success, 1 on error; coarse and fine partitions are set.
*************************************************************************/

/*FIXME: I am impossible to understand.  Comprehensibilize this function!*/
int set_partitions()
{
  double f_min,f_max,f,df,dfx,dfy,dfz;
  int i,j;
  double steps_min,steps_max;
  double x_aspect,y_aspect,z_aspect;
  int x_in,y_in,z_in;
  int x_start,y_start,z_start;
  double A,B,k;
  struct vector3 part_min,part_max;
  double smallest_spacing;
  
  smallest_spacing = 0.1/world->length_unit;  /* 100nm */
  if (2*world->rx_radius_3d > smallest_spacing) smallest_spacing=2*world->rx_radius_3d;

  if (world->n_fineparts != 4096 + 16384 + 4096)
  {
    world->n_fineparts = 4096 + 16384 + 4096;
    world->x_fineparts = (double*)malloc(sizeof(double)*world->n_fineparts);
    world->y_fineparts = (double*)malloc(sizeof(double)*world->n_fineparts);
    world->z_fineparts = (double*)malloc(sizeof(double)*world->n_fineparts);
  }
  if((world->x_fineparts == NULL) || (world->y_fineparts == NULL) ||
        (world->z_fineparts == NULL))
  {
    fprintf(world->err_file, "Out of memory while trying to create partitions.\n");
    return 1;
  }

  dfx = 1e-3 + (world->bb_max.x - world->bb_min.x)/8191.0;
  dfy = 1e-3 + (world->bb_max.y - world->bb_min.y)/8191.0;
  dfz = 1e-3 + (world->bb_max.z - world->bb_min.z)/8191.0;
 
  f_min = world->bb_min.x - dfx;
  f_max = world->bb_max.x + dfx;
  if (f_max - f_min < smallest_spacing)
  {
    printf("Rescaling: was %.3f to %.3f, now ",f_min*world->length_unit,f_max*world->length_unit);
    f = smallest_spacing - (f_max-f_min);
    f_max += 0.5*f;
    f_min -= 0.5*f;
    printf("%.3f to %.3f\n",f_min*world->length_unit,f_max*world->length_unit);
  }
  part_min.x = f_min;
  part_max.x = f_max;
  df = (f_max - f_min)/16383.0;
  for (i=0;i<16384;i++)
  {
    world->x_fineparts[ 4096 + i ] = f_min + df*((double)i);
  }
  find_exponential_params(-f_min,1e12,df,4096,&A,&B,&k);
  for (i=1;i<=4096;i++) world->x_fineparts[4096-i] = -(A*exp(i*k)+B);
  find_exponential_params(f_max,1e12,df,4096,&A,&B,&k);
  for (i=1;i<=4096;i++) world->x_fineparts[4096+16383+i] = A*exp(i*k)+B;
  dfx = df;

  f_min = world->bb_min.y - dfy;
  f_max = world->bb_max.y + dfy;
  if (f_max - f_min < smallest_spacing)
  {
    printf("Rescaling: was %.3f to %.3f, now ",f_min*world->length_unit,f_max*world->length_unit);
    f = smallest_spacing - (f_max-f_min);
    f_max += 0.5*f;
    f_min -= 0.5*f;
    printf("%.3f to %.3f\n",f_min*world->length_unit,f_max*world->length_unit);
  }
  part_min.y = f_min;
  part_max.y = f_max; 
  df = (f_max - f_min)/16383.0;
  for (i=0;i<16384;i++)
  {
    world->y_fineparts[ 4096 + i ] = f_min + df*((double)i);
  }
  find_exponential_params(-f_min,1e12,df,4096,&A,&B,&k);
  for (i=1;i<=4096;i++) world->y_fineparts[4096-i] = -(A*exp(i*k)+B);
  find_exponential_params(f_max,1e12,df,4096,&A,&B,&k);
  for (i=1;i<=4096;i++) world->y_fineparts[4096+16383+i] = A*exp(i*k)+B;
  dfy = df;

  f_min = world->bb_min.z - dfz;
  f_max = world->bb_max.z + dfz;
  if (f_max - f_min < smallest_spacing)
  {
    printf("Rescaling: was %.3f to %.3f, now ",f_min*world->length_unit,f_max*world->length_unit);
    f = smallest_spacing - (f_max-f_min);
    f_max += 0.5*f;
    f_min -= 0.5*f;
    printf("%.3f to %.3f\n",f_min*world->length_unit,f_max*world->length_unit);
  }
  part_min.z = f_min;
  part_max.z = f_max;
  df = (f_max - f_min)/16383.0;
  for (i=0;i<16384;i++)
  {
    world->z_fineparts[ 4096 + i ] = f_min + df*((double)i);
  }
  find_exponential_params(-f_min,1e12,df,4096,&A,&B,&k);
  for (i=1;i<=4096;i++) world->z_fineparts[4096-i] = -(A*exp(i*k)+B);
  find_exponential_params(f_max,1e12,df,4096,&A,&B,&k);
  for (i=1;i<=4096;i++) world->z_fineparts[4096+16383+i] = A*exp(i*k)+B;
  dfz = df;
  
  f = part_max.x - part_min.x;
  f_min = f_max = f;
  f = part_max.y - part_min.y;
  if (f < f_min) f_min = f;
  else if (f > f_max) f_max = f;
  f = part_max.z - part_min.z;
  if (f < f_min) f_min = f;
  else if (f > f_max) f_max = f;
  
  if (world->speed_limit == 0)
  {
    steps_min = f_min;
    steps_max = f_max;
  }
  else
  {
    steps_min = f_min / world->speed_limit;
    steps_max = f_max / world->speed_limit;
  }
 
  /* Verify that partitions are not closer than interaction diameter. */
  if (world->x_partitions!=NULL)
  {
    for (i=1;i<world->nx_parts;i++)
    {
      if (world->x_partitions[i] - world->x_partitions[i-1] < 2*world->rx_radius_3d)
      {
        fprintf(world->err_file,"Error: X partitions closer than interaction diameter\n");
        fprintf(world->err_file,"  X partition #%d at %g\n",i,world->length_unit*world->x_partitions[i-1]);
        fprintf(world->err_file,"  X partition #%d at %g\n",i+1,world->length_unit*world->x_partitions[i]);
        fprintf(world->err_file,"  Interaction diameter %g\n",2*world->length_unit*world->rx_radius_3d);
        return 1;
      }
    }
  }
  if (world->y_partitions!=NULL)
  {
    for (i=1;i<world->ny_parts;i++)
    {
      if (world->y_partitions[i] - world->y_partitions[i-1] < 2*world->rx_radius_3d)
      {
        fprintf(world->err_file,"Error: Y partitions closer than interaction diameter\n");
        fprintf(world->err_file,"  Y partition #%d at %g\n",i,world->length_unit*world->y_partitions[i-1]);
        fprintf(world->err_file,"  Y partition #%d at %g\n",i+1,world->length_unit*world->y_partitions[i]);
        fprintf(world->err_file,"  Interaction diameter %g\n",2*world->length_unit*world->rx_radius_3d);
        return 1;
      }
    }
  }
  if (world->z_partitions!=NULL)
  {
    for (i=1;i<world->nz_parts;i++)
    {
      if (world->z_partitions[i] - world->z_partitions[i-1] < 2*world->rx_radius_3d)
      {
        fprintf(world->err_file,"Error: Z partitions closer than interaction diameter\n");
        fprintf(world->err_file,"  Z partition #%d at %g\n",i,world->length_unit*world->z_partitions[i-1]);
        fprintf(world->err_file,"  Z partition #%d at %g\n",i+1,world->length_unit*world->z_partitions[i]);
        fprintf(world->err_file,"  Interaction diameter %g\n",2*world->length_unit*world->rx_radius_3d);
        return 1;
      }
    }
  }
  

  /* go with automatic partitioning */
  if (world->x_partitions == NULL ||
      world->y_partitions == NULL ||
      world->z_partitions == NULL)
  {
    if (steps_max / MAX_TARGET_TIMESTEP > MAX_COARSE_PER_AXIS)
    {
      world->nx_parts = world->ny_parts = world->nz_parts = MAX_COARSE_PER_AXIS;
    }
    else if (steps_min / MIN_TARGET_TIMESTEP < MIN_COARSE_PER_AXIS)
    {
      world->nx_parts = world->ny_parts = world->nz_parts = MIN_COARSE_PER_AXIS;
    }
    else
    {
      world->nx_parts = steps_min / MIN_TARGET_TIMESTEP;
      if (world->nx_parts > MAX_COARSE_PER_AXIS)
        world->nx_parts = MAX_COARSE_PER_AXIS;
      if ((world->nx_parts & 1) != 0) world->nx_parts += 1;
      
      world->ny_parts = world->nz_parts = world->nx_parts;
    }
    
    world->x_partitions = (double*) malloc( sizeof(double) * world->nx_parts );
    world->y_partitions = (double*) malloc( sizeof(double) * world->ny_parts );
    world->z_partitions = (double*) malloc( sizeof(double) * world->nz_parts );
  
    if((world->x_partitions == NULL) || (world->y_partitions == NULL) ||
        (world->z_partitions == NULL))
    {
      fprintf(world->err_file, "Out of memory while trying to create partitions.\n");
      return 1;
    }

    x_aspect = (part_max.x - part_min.x) / f_max;
    y_aspect = (part_max.y - part_min.y) / f_max;
    z_aspect = (part_max.z - part_min.z) / f_max;
    
    x_in = floor( (world->nx_parts - 2) * x_aspect + 0.5 );
    y_in = floor( (world->ny_parts - 2) * y_aspect + 0.5 );
    z_in = floor( (world->nz_parts - 2) * z_aspect + 0.5 );
    if (x_in < 2) x_in = 2;
    if (y_in < 2) y_in = 2;
    if (z_in < 2) z_in = 2;
    
    smallest_spacing = 2*world->rx_radius_3d;
    if ( (part_max.x-part_min.x)/(x_in-1) < smallest_spacing )
    {
      x_in = 1 + floor((part_max.x-part_min.x)/smallest_spacing);
    }
    if ( (part_max.y-part_min.y)/(y_in-1) < smallest_spacing )
    {
      y_in = 1 + floor((part_max.y-part_min.y)/smallest_spacing);
    }
    if ( (part_max.z-part_min.z)/(z_in-1) < smallest_spacing )
    {
      z_in = 1 + floor((part_max.z-part_min.z)/smallest_spacing);
    }
    
    if (x_in < 2) x_in = 2;
    if (y_in < 2) y_in = 2;
    if (z_in < 2) z_in = 2;
    x_start = (world->nx_parts - x_in)/2;
    y_start = (world->ny_parts - y_in)/2;
    z_start = (world->nz_parts - z_in)/2;
    if (x_start < 1) x_start = 1;
    if (y_start < 1) y_start = 1;
    if (z_start < 1) z_start = 1;
    
    f = (part_max.x - part_min.x) / (x_in - 1);
    world->x_partitions[0] = world->x_fineparts[1];
    for (i=x_start;i<x_start+x_in;i++)
    {
      world->x_partitions[i] = world->x_fineparts[4096 + (i-x_start)*16384/(x_in-1)];
    }
    for (i=x_start-1;i>0;i--)
    {
      for (j=0 ; world->x_partitions[i+1]-world->x_fineparts[4095-j] < f ; j++) {}
      world->x_partitions[i] = world->x_fineparts[4095-j];
    }
    for (i=x_start+x_in;i<world->nx_parts-1;i++)
    {
      for (j=0 ; world->x_fineparts[4096+16384+j]-world->x_partitions[i-1] < f ; j++) {}
      world->x_partitions[i] = world->x_fineparts[4096+16384+j];
    }
    world->x_partitions[world->nx_parts-1] = world->x_fineparts[4096+16384+4096-2];
    
    f = (part_max.y - part_min.y) / (y_in - 1);
    world->y_partitions[0] = world->y_fineparts[1];
    for (i=y_start;i<y_start+y_in;i++)
    {
      world->y_partitions[i] = world->y_fineparts[4096 + (i-y_start)*16384/(y_in-1)];
    }
    for (i=y_start-1;i>0;i--)
    {
      for (j=0 ; world->y_partitions[i+1]-world->y_fineparts[4095-j] < f ; j++) {}
	world->y_partitions[i] = world->y_fineparts[4095-j];
    }
    for (i=y_start+y_in;i<world->ny_parts-1;i++)
    {
      for (j=0 ; world->y_fineparts[4096+16384+j]-world->y_partitions[i-1] < f ; j++) {}
      world->y_partitions[i] = world->y_fineparts[4096+16384+j];
    }
    world->y_partitions[world->ny_parts-1] = world->y_fineparts[4096+16384+4096-2];
    
    f = (part_max.z - part_min.z) / (z_in - 1);
    world->z_partitions[0] = world->z_fineparts[1];
    for (i=z_start;i<z_start+z_in;i++)
    {
      world->z_partitions[i] = world->z_fineparts[4096 + (i-z_start)*16384/(z_in-1)];
    }
    for (i=z_start-1;i>0;i--)
    {
      for (j=0 ; world->z_partitions[i+1]-world->z_fineparts[4095-j] < f ; j++) {}
      world->z_partitions[i] = world->z_fineparts[4095-j];
    }
    for (i=z_start+z_in;i<world->nz_parts-1;i++)
    {
      for (j=0 ; world->z_fineparts[4096+16384+j]-world->z_partitions[i-1] < f ; j++) {}
      world->z_partitions[i] = world->z_fineparts[4096+16384+j];
    }
    world->z_partitions[world->nz_parts-1] = world->z_fineparts[4096+16384+4096-2];

  }
  else
  {

    double *dbl_array;

/* We need to keep the outermost partition away from the world bounding box */

    dfx += 1e-3;
    dfy += 1e-3;
    dfz += 1e-3;
    
    if (world->x_partitions[1] + dfx > world->bb_min.x)
    {
      if (world->x_partitions[1] - dfx < world->bb_min.x)
	world->x_partitions[1] = world->bb_min.x-dfx;
      else
      {
	dbl_array = (double*) malloc( sizeof(double)*(world->nx_parts+1) );
	if (dbl_array == NULL)
	{ 
	  fprintf(world->err_file, "Out of memory while trying to create partitions.\n");
	  return 1;
	}
  
	dbl_array[0] = world->x_partitions[0];
	dbl_array[1] = world->bb_min.x - dfx;
	memcpy(&(dbl_array[2]),&(world->x_partitions[1]),sizeof(double)*(world->nx_parts-1));
	free( world->x_partitions );
	world->x_partitions = dbl_array;
	world->nx_parts++;
      }
    }
    if (world->x_partitions[world->nx_parts-2] - dfx < world->bb_max.x)
    {
      if (world->x_partitions[world->nx_parts-2] + dfx > world->bb_max.x)
	world->x_partitions[world->nx_parts-2] = world->bb_max.x + dfx;
      else
      {
	dbl_array = (double*) malloc( sizeof(double)*(world->nx_parts+1) );
	if (dbl_array == NULL)
	{ 
	  fprintf(world->err_file, "Out of memory while trying to create partitions.\n");
	  return 1;
	}
  
	dbl_array[world->nx_parts] = world->x_partitions[world->nx_parts-1];
	dbl_array[world->nx_parts-1] = world->bb_max.x + dfx;
	memcpy(dbl_array,world->x_partitions,sizeof(double)*(world->nx_parts-1));
	free( world->x_partitions );
	world->x_partitions = dbl_array;
	world->nx_parts++;
	}
    }
     if (world->y_partitions[1] + dfy > world->bb_min.y)
    {
      if (world->y_partitions[1] - dfy < world->bb_min.y)
	world->y_partitions[1] = world->bb_min.y-dfy;
      else
      {
	dbl_array = (double*) malloc( sizeof(double)*(world->ny_parts+1) );
	if (dbl_array==NULL)
	{ 
	  fprintf(world->err_file, "Out of memory while trying to create partitions.\n");
	  return 1;
	}
  
	dbl_array[0] = world->y_partitions[0];
	dbl_array[1] = world->bb_min.y - dfy;
	memcpy(&(dbl_array[2]),&(world->y_partitions[1]),sizeof(double)*(world->ny_parts-1));
	free( world->y_partitions );
	world->y_partitions = dbl_array;
	world->ny_parts++;
      }
    }
    if (world->y_partitions[world->ny_parts-2] - dfy < world->bb_max.y)
    {
      if (world->y_partitions[world->ny_parts-2] + dfy > world->bb_max.y)
	world->y_partitions[world->ny_parts-2] = world->bb_max.y + dfy;
      else
      {
	dbl_array = (double*) malloc( sizeof(double)*(world->ny_parts+1) );
	if (dbl_array==NULL)
	{
	  fprintf(world->err_file, "Out of memory while trying to create partitions.\n");
	  return 1;
	}
  
	dbl_array[world->ny_parts] = world->y_partitions[world->ny_parts-1];
	dbl_array[world->ny_parts-1] = world->bb_max.y + dfy;
	memcpy(dbl_array,world->y_partitions,sizeof(double)*(world->ny_parts-1));
	free( world->y_partitions );
	world->y_partitions = dbl_array;
	world->ny_parts++;
      }
    }
    if (world->z_partitions[1] + dfz > world->bb_min.z)
    {
      if (world->z_partitions[1] - dfz < world->bb_min.z)
	world->z_partitions[1] = world->bb_min.z-dfz;
      else
      {
	dbl_array = (double*) malloc( sizeof(double)*(world->nz_parts+1) );
	if (dbl_array==NULL)
	{
	  fprintf(world->err_file, "Out of memory while trying to create partitions.\n");
	  return 1;
	} 
  
	dbl_array[0] = world->z_partitions[0];
	dbl_array[1] = world->bb_min.z - dfz;
	memcpy(&(dbl_array[2]),&(world->z_partitions[1]),sizeof(double)*(world->nz_parts-1));
	free( world->z_partitions );
	world->z_partitions = dbl_array;
	world->nz_parts++;
      }
    }
    if (world->z_partitions[world->nz_parts-2] - dfz < world->bb_max.z)
    {
      if (world->z_partitions[world->nz_parts-2] + dfz > world->bb_max.z)
	world->z_partitions[world->nz_parts-2] = world->bb_max.z + dfz;
      else
      {
	dbl_array = (double*) malloc( sizeof(double)*(world->nz_parts+1) );
	if (dbl_array==NULL){
	  fprintf(world->err_file, "Out of memory while trying to create partitions.\n");
	  return 1;
	} 
  
	dbl_array[world->nz_parts] = world->z_partitions[world->nz_parts-1];
	dbl_array[world->nz_parts-1] = world->bb_max.z + dfz;
	memcpy(dbl_array,world->z_partitions,sizeof(double)*(world->nz_parts-1));
	free( world->z_partitions );
	world->z_partitions = dbl_array;
	world->nz_parts++;
      }
    }
   
    world->x_partitions[0] = world->x_fineparts[1];
    for (i=1;i<world->nx_parts-1;i++)
    {
      world->x_partitions[i] = 
        world->x_fineparts[ 
          bisect_near( 
            world->x_fineparts , world->n_fineparts ,
            world->x_partitions[i]
          )
        ];
    }
    world->x_partitions[world->nx_parts-1] = world->x_fineparts[4096+16384+4096-2];

    world->y_partitions[0] = world->y_fineparts[1];
    for (i=1;i<world->ny_parts-1;i++)
    {
      world->y_partitions[i] = 
        world->y_fineparts[ 
          bisect_near( 
            world->y_fineparts , world->n_fineparts ,
            world->y_partitions[i]
          )
        ];
    }
    world->y_partitions[world->ny_parts-1] = world->y_fineparts[4096+16384+4096-2];

    world->z_partitions[0] = world->z_fineparts[1];
    for (i=1;i<world->nz_parts-1;i++)
    {
      world->z_partitions[i] = 
        world->z_fineparts[ 
          bisect_near( 
            world->z_fineparts , world->n_fineparts ,
            world->z_partitions[i]
          )
        ];
    }
    world->z_partitions[world->nz_parts-1] = world->z_fineparts[4096+16384+4096-2];
  }
  
  printf("X partitions: ");
  printf("-inf ");
  for (i=1;i<world->nx_parts - 1;i++) printf("%.5f ",world->length_unit * world->x_partitions[i]);
  printf("inf");
  printf("\n");
  printf("Y partitions: ");
  printf("-inf ");
  for (i=1;i<world->ny_parts - 1;i++) printf("%.5f ",world->length_unit * world->y_partitions[i]);
  printf("inf");
  printf("\n");
  printf("Z partitions: ");
  printf("-inf ");
  for (i=1;i<world->nz_parts - 1;i++) printf("%.5f ",world->length_unit * world->z_partitions[i]);
  printf("inf");
  printf("\n");

  return 0;
}
/**********************************************************************
distance_point_line -- returns distance between point and line in 3D space
              The line is defined by 2 points
              The formulas are taken from "Computer Graphics" 
              by Michael Mortenson, ISBN 0-8311-1182-8, p.222
Parameters
	q -- location of the fixed point
	v0 -- first point on the line
	v1 -- second point on the line

Returns
	distance between the point and the line
**********************************************************************/
double distance_point_line(struct vector3 *q, struct vector3 *v0, struct vector3 *v1)
{
   double x,y,z; /* coordinates of the point q */
   double x0,y0,z0; /* coordinates of the point v0 */
   double x1,y1,z1; /* coordinates of the point v1 */
   double u; /* parameter in the equation of the line
                p(u) = p0 + u(p1 - p0) */
   double p_x,p_y,p_z; /* X,Y, and Z components of the vector p (line) */
   double nominator, denominator, d_min;

   x = q->x;
   y = q->y;
   z = q->z;

   x0 = v0->x;
   y0 = v0->y;
   z0 = v0->z;

   x1 = v1->x;
   y1 = v1->y;
   z1 = v1->z;
   
   nominator = (x1 - x0)*(x - x0) + (y1 - y0)*(y - y0) + (z1 - z0)*(z - z0);
   denominator = sqrt(pow((x1-x0),2) + pow((y1 - y0),2) + pow((z1 - z0),2));
   u = nominator/denominator;

   p_x = x0 +u*(x1 - x0);
   p_y = y0 +u*(y1 - y0);
   p_z = z0 +u*(z1 - z0);

   d_min = sqrt(pow((p_x - x),2) + pow((p_y - y),2) + pow((p_z - z),2)); 
   return d_min;
} 

/**********************************************************************
navigate_world -- returns index of the destination subvolume

Parameters
	cur_index -- index of the starting subvolume 
	direction -- direction of the movement
                     Valid values are X_NEG, X_POS, Y_NEG, Y_POS,
                                      Z_NEG, Z_POS.

Returns
	index of the destination neighbor subvolume ("face-to-face")
**********************************************************************/
int navigate_world(int curr_index, int direction)
{
        switch(direction)
        {
		case(X_NEG):
		   return curr_index - (world->nz_parts-1)*(world->ny_parts-1);
                   break;
                case(X_POS):
		   return curr_index + (world->nz_parts-1)*(world->ny_parts-1);
                   break;
                case(Y_NEG):  
		   return curr_index - (world->nz_parts-1);
                   break;
                case(Y_POS):
		   return curr_index + (world->nz_parts-1);
                   break;
                case(Z_NEG):
                   return curr_index - 1;
                   break;
                case(Z_POS):
                   return curr_index + 1;
                   break;
                default:
		   return INT_MAX;
                   break;

        }
 
}

/**********************************************************************
navigate_world_by_edge -- returns index of the destination subvolume

Parameters
	cur_index -- index of the starting subvolume 
	direction1 -- direction of the first movement
	direction2 -- direction of the second movement
                     Valid values are X_NEG, X_POS, Y_NEG, Y_POS,
                                      Z_NEG, Z_POS.

Returns
	index of the destination neighbor subvolume ("edge-to-edge")
**********************************************************************/
int navigate_world_by_edge(int curr_index, int direction1, int direction2)
{
  	int first_index, final_index;
        first_index = navigate_world(curr_index, direction1);
        final_index = navigate_world(first_index, direction2);
    
        return final_index; 
}
/**********************************************************************
navigate_world_by_corner -- returns index of the destination subvolume

Parameters
	cur_index -- index of the starting subvolume 
	direction1 -- direction of the first movement
	direction2 -- direction of the second movement
	direction3 -- direction of the third movement
                     Valid values are X_NEG, X_POS, Y_NEG, Y_POS,
                                      Z_NEG, Z_POS.

Returns
	index of the destination neighbor subvolume ("corner-to-corner")
**********************************************************************/
int navigate_world_by_corner(int curr_index, int direction1, int direction2, int direction3)
{
  	int first_index, second_index, final_index;
        first_index = navigate_world(curr_index, direction1);
        second_index = navigate_world(first_index, direction2);
        final_index = navigate_world(second_index, direction3);
    
        return final_index; 
}

/************************************************************************
   In: starting position of the molecule
       displacement (random walk) vector
       vector to store one corner of the bounding box
       vector to store the opposite corner of the bounding box 
   Out: No return value. The vectors are set to define the bounding box
        of the random walk movement that extends for R_INT in all
        directions.
************************************************************************/
void path_bounding_box(struct vector3 *loc, struct vector3 * displacement,
 struct vector3 *llf, struct vector3 *urb)
{
   struct vector3 final;  /* final position of the molecule after random walk */
   double R;     /* molecule interaction radius */

  
   R = world->rx_radius_3d; 
   vect_sum(loc, displacement, &final);

   llf->x = urb->x = loc->x;
   llf->y = urb->y = loc->y;
   llf->z = urb->z = loc->z;

   if(final.x < llf->x) {
         llf->x = final.x;
   }
   if(final.x > urb->x){
       urb->x = final.x;
   }
   if(final.y < llf->y) {
         llf->y = final.y;
   }
   if(final.y > urb->y){
       urb->y = final.y;
   }
   if(final.z < llf->z) {
         llf->z = final.z;
   }
   if(final.z > urb->z){
       urb->z = final.z;
   }
   /* Extend the bounding box at the distance R. */
   llf->x -= R;
   llf->y -= R;
   llf->z -= R;

   urb->x += R;
   urb->y += R;
   urb->z += R;
}

