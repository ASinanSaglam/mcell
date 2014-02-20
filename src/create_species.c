/***********************************************************************************
 *                                                                                 *
 * Copyright (C) 2006-2013 by                                                      *
 * The Salk Institute for Biological Studies and                                   *
 * Pittsburgh Supercomputing Center, Carnegie Mellon University                    *
 *                                                                                 *
 * This program is free software; you can redistribute it and/or                   *
 * modify it under the terms of the GNU General Public License                     *
 * as published by the Free Software Foundation; either version 2                  *
 * of the License, or (at your option) any later version.                          *
 *                                                                                 *
 * This program is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of                  *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                   *
 * GNU General Public License for more details.                                    *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License               *
 * along with this program; if not, write to the Free Software                     *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. *
 *                                                                                 *
 ***********************************************************************************/

#include <math.h>

#include "libmcell.h"
#include "logging.h"
#include "sym_table.h"
#include "create_species.h"
#include "diffuse_util.h"



/**************************************************************************
 assemble_mol_species:
    Helper function to assemble a molecule species from its component pieces.
   
    NOTE: A couple of comments regarding the unit conversions below:
    Internally, mcell works with with the per species length
    normalization factor
   
       new_species->space_step = sqrt(4*D*t), D = diffusion constant (1)
   
    If the user supplies a CUSTOM_SPACE_STEP or SPACE_STEP then
    it is assumed to correspond to the average diffusion step and
    is hence equivalent to lr_bar in 2 or 3 dimensions for surface and
    volume molecules, respectively:
   
    lr_bar_2D = sqrt(pi*D*t)       (2)
    lr_bar_3D = 2*sqrt(4*D*t/pi)   (3)
   
    Hence, given a CUSTOM_SPACE_STEP/SPACE_STEP we need to
    solve eqs (2) and (3) for t and obtain new_species->space_step
    via equation (1)
   
    2D:
     lr_bar_2D = sqrt(pi*D*t) => t = (lr_bar_2D^2)/(pi*D)
   
    3D:
     lr_bar_3D = 2*sqrt(4*D*t/pi) => t = pi*(lr_bar_3D^2)/(16*D)
   
    The remaining coefficients are:
   
     - 1.0e8 : needed to convert D from cm^2/s to um^2/s
     - global_time_unit, length_unit, r_length_unit: mcell
       internal time/length conversions.

 In: state: the simulation state
     sym_ptr:   symbol for the species
     D_ref: reference diffusion constant
     D:     diffusion constant
     is_2d: 1 if the species is a 2D molecule, 0 if 3D
     custom_time_step: time_step for the molecule (<0.0 for a custom space
                       step, >0.0 for custom timestep, 0.0 for default
                       timestep)
     target_only: 1 if the molecule cannot initiate reactions
 Out: the species, or NULL if an error occurred
**************************************************************************/
struct species*
assemble_mol_species(MCELL_STATE* state,
                     struct sym_table *sym_ptr,
                     double D_ref,
                     double D,
                     int is_2d,
                     double custom_time_step,
                     int target_only,
                     double max_step_length)
{
  // Fill in species info 
  
  // The global time step must be defined before creating any species since it
  // is used in calculations involving custom time and space steps
  double global_time_unit = state->time_unit;
  struct species *new_species = (struct species *) sym_ptr->value;

  if (is_2d) {
    new_species->flags |= ON_GRID;
  }
  else {
    new_species->flags &= ~ON_GRID;
  }

  new_species->D = D;
  new_species->D_ref = D_ref;
  new_species->time_step = custom_time_step;

  if (new_species->D_ref == 0) {
    new_species->D_ref = new_species->D;
  }
  if (target_only) {
    new_species->flags |= CANT_INITIATE;
  }
  if (max_step_length > 0) {
    new_species->flags |= SET_MAX_STEP_LENGTH;
  }

  // Determine the actual space step and time step

  if (new_species->D == 0) /* Immobile (boring) */
  {
    new_species->space_step = 0.0;
    new_species->time_step = 1.0;
  }
  else if (new_species->time_step != 0.0) /* Custom timestep */
  {
    if (new_species->time_step < 0) /* Hack--negative value means custom space step */
    {
      double lr_bar = -new_species->time_step;
      if (is_2d)
      {
        new_species->time_step = 
          lr_bar * lr_bar / (MY_PI * 1.0e8 * new_species->D * global_time_unit);
      }
      else
      {
        new_species->time_step =
          lr_bar * lr_bar * MY_PI / 
          (16.0 * 1.0e8 * new_species->D * global_time_unit);
      }
      new_species->space_step =
        sqrt(4.0 * 1.0e8 * new_species->D * new_species->time_step * global_time_unit)
        * state->r_length_unit;
    }
    else
    {
      new_species->space_step =
        sqrt(4.0 * 1.0e8 * new_species->D * new_species->time_step)
        * state->r_length_unit;
      new_species->time_step /= global_time_unit;
    }
  }
  else if (state->space_step == 0) /* Global timestep */
  {
    new_species->space_step =
      sqrt(4.0 * 1.0e8 * new_species->D * global_time_unit)
      * state->r_length_unit;
    new_species->time_step=1.0;
  }
  else /* Global spacestep */
  {
    double space_step = state->space_step * state->length_unit;
    if (is_2d)
    {
      new_species->time_step = space_step * space_step /
        (MY_PI* 1.0e8 * new_species->D * global_time_unit);
    }
    else
    {
      new_species->time_step =
        space_step * space_step * MY_PI /
        (16.0 * 1.0e8 * new_species->D * global_time_unit);
    }
    new_species->space_step =
      sqrt(4.0 * 1.0e8 * new_species->D * new_species->time_step * global_time_unit)
      * state->r_length_unit;
  }

  new_species->refl_mols = NULL;
  new_species->transp_mols = NULL;
  new_species->absorb_mols = NULL;
  new_species->clamp_conc_mols = NULL;

  return new_species;

}



/**************************************************************************
 new_mol_species:
    Create a new species. There must not yet be a molecule or named reaction
    pathway with the supplied name.

 In: state: the simulation state
     name:  name for the new species
     sym_ptr:   symbol for the species
 Out: 0 on success, positive integer on failure
**************************************************************************/
int
new_mol_species(MCELL_STATE* state, char *name, struct sym_table *sym_ptr)
{
  // Molecule already defined
  if (retrieve_sym(name, state->mol_sym_table) != NULL) {
    return 2;
  }
  // Molecule already defined as a named reaction pathway
  else if (retrieve_sym(name, state->rxpn_sym_table) != NULL) {
    return 3;
  }
  *sym_ptr = *store_sym(name, MOL, state->mol_sym_table, NULL);
  // Out of memory while creating molecule
  if (sym_ptr == NULL) {
    return 4;
  }

  return 0;
}



/**************************************************************************
 ensure_rdstep_tables_built:
    Build the r_step/d_step tables if they haven't been built yet.

 In: state: the simulation state
 Out: 0 on success, positive integer on failure
**************************************************************************/
int
ensure_rdstep_tables_built(MCELL_STATE* state)
{
  if (state->r_step != NULL &&
      state->r_step_surface != NULL &&
      state->d_step != NULL)
  {
    return 0;
  }

  if (state->r_step == NULL)
  {
    // Out of memory while creating r_step data for molecule
    if ((state->r_step = init_r_step(state->radial_subdivisions)) == NULL) {
      return 5;
    }
  }

  if (state->r_step_surface == NULL)
  {
    state->r_step_surface = init_r_step_surface(state->radial_subdivisions);
    // Cannot store r_step_surface data.
    if (state->r_step_surface == NULL) {
      return 6;
    }
  }

  if (state->d_step == NULL)
  {
    // Out of memory while creating d_step data for molecule
    if ((state->d_step = init_d_step(state->radial_directions, &state->num_directions)) == NULL) {
      return 7;
    }

    // Num directions, rounded up to the nearest 2^n - 1
    state->directions_mask = state->num_directions;
    state->directions_mask |= (state->directions_mask >> 1);
    state->directions_mask |= (state->directions_mask >> 2);
    state->directions_mask |= (state->directions_mask >> 4);
    state->directions_mask |= (state->directions_mask >> 8);
    state->directions_mask |= (state->directions_mask >> 16);
    // Internal error: bad number of default RADIAL_DIRECTIONS (max 131072).
    if (state->directions_mask > (1<<18)) {
      return 8;
    }
  }

  return 0;
}
