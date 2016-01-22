/******************************************************************************
 *
 * Copyright (C) 2006-2015 by
 * The Salk Institute for Biological Studies and
 * Pittsburgh Supercomputing Center, Carnegie Mellon University
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
******************************************************************************/

#ifndef INCLUDED_MACROMOLECULE_H
#define INCLUDED_MACROMOLECULE_H

#include "mcell_structs.h"

/*
 * A subunit relation defines a particular semantic relation between
 * different subunits in a macromolecule (sucn as "dimer partner").
 *
 * Subunit relations are referred to by name in a few places, such as
 * cooperative rate rules.  Each subunit relation must, given a
 * starting subunit, be able to identify the related subunit under the
 * mapping established by this relation.
 *
 * Currently, this mapping is a bijection, and forward and reverse
 * lookups may both be done by array lookup keyed on subunit index
 * within the complex.
 */
struct subunit_relation {
  char const *name;   /* Name of relation (used for reference in rule tables) */
  int const *target;  /* Array of target subunit from each source subunit */
  int const *inverse; /* Inverse of 'target' mapping */
};

/*
 * A complex rate is an ordered table of rules, mapping states of a
 * macromolecule (that is, of the subunits of the macromolecule) onto
 * reaction rates for subunit reactions.  In particular, the state
 * which may control reaction rate for a particular reaction for
 * subunit N are the species and orientations of all "related" subunits
 * (i.e. all subunits whose indices appear in the N-th slot in the
 * target array of any of the defined relations for the complex).
 *
 * The complex rate table will have one row for every rule in the
 * table, and will have as many columns as the complex has relations.
 * Surface macromolecules add one extra column in case we want to match
 * on the orientation of the complex itself.  Note that many rate rules
 * may use only a subset of the available relations, but for
 * simplicity, each rate table includes columns for the full set of
 * relations.
 *
 * We move down the table from the first row to the last, accepting the
 * first rule we find that matches.  This means that if the first rule
 * in the table is the "DEFAULT" rule, no other rules will ever be
 * matched.
 *
 * Semantically, each rule in the table consists of up to M clauses,
 * each of the form:
 *
 *    RELATION == SPECIES
 * or:
 *    RELATION != SPECIES
 *
 * where RELATION is a named subunit_relation defined for this
 * macromolecule, and SPECIES is a defined species with optional
 * orientation.  No relation may appear in a rule more than once, and
 * the rule is matched if all clauses match.  Any relation not
 * mentioned in a rule is automatically matched.
 *
 * The rows and columns of the table are stored in 4 arrays within the
 * rate.  If there are M columns and N rows in the rule table, then:
 *
 *    invert is an array of M*N integers, 0 if we're matching for
 *        equality on this relation, 1 if we're matching for
 *        inequality.
 *
 *    neighbors is an array of M*N species, containing NULL if we don't
 *        care about the species, (i.e. this relation is not
 *        mentioned), or a pointer to the species to match.
 *
 *    orientations is an array of M*N 8-bit signed integers, 1 if the
 *        orientation being matched (or rejected) is the same as the
 *        orientation of the reference subunit, -1 if the orientation
 *        is opposite, and 0 if we don't care about the orientation.
 *        orientations is NULL for volume macromolecules.
 *
 *    rates is an array of N doubles, giving the reaction rate
 *        associated with each rule.
 *
 * The DEFAULT rule, then, consists of all 0 for invert and
 * orientation, and all NULL for neighbors.  Each rate table implicitly
 * ends with a DEFAULT rule and a rate of 0.0 if no DEFAULT rule is
 * specified in the MDL.
 */
struct complex_rate {
  struct complex_rate *next; /* link to next rate table */
  char const *name;          /* name of this rate rule table */

  int num_rules;              /* count of rules in this table */
  int num_neighbors;          /* count of clauses in each rule */
  struct species **neighbors; /* species for rate rule clauses */
  int *invert;                /* invert flags for rate rule clauses */
  signed char *orientations;  /* orients for rate rule clauses */
  double *rates;              /* rates for each rule */
};

/*
 * A complex species is an extension of the base species used for most
 * molecules in MCell.  It contains details of how to initialize the subunits
 * when placing a complex of this species.  It also contains a table of
 * relations, a linked list of rate tables, and a linked list of counters.
 */
struct complex_species {
  struct species base; /* base species */

  int num_subunits;              /* num subunits */
  struct species **subunits;     /* initial species for each subunit */
  signed char *orientations;     /* initial orients for each subunit */
  struct vector3 *rel_locations; /* relative subunit locations */

  int num_relations;                        /* count of relations */
  struct subunit_relation const *relations; /* array of relations */

  struct complex_rate *rates; /* list of rate tables */

  struct complex_counters *counters; /* counters for this species, or NULL */
};

/*
 * A complex counter is used to count subunit state configurations in
 * macromolecules.  Presently, it uses tables like the complex rate tables.
 *
 * subunit_to_rules_range is a hash table giving ranges of indices in the
 * tables, keyed by species.
 *
 */
struct complex_counter {
  struct complex_counter *next;               /* Link to next counter */
  struct pointer_hash subunit_to_rules_range; /* Map from subunit species to
                                                 index */
  int *su_rules_indices; /* Array of indices into rules */

  struct species **neighbors; /* species for match rules */
  signed char *orientations;  /* orients for match rules */
  int *invert;                /* invert flag for match rules */
  int *counts;                /* Counts for match rules */
  int this_orient;            /* Complex orient for these counter */
};

/*
 * complex_counters is a collection of counters by region.  in_world is the set
 * of complex counters for WORLD.  region_to_counter is a map from region to
 * counters.
 */
struct complex_counters {
  struct complex_counter in_world; /* WORLD counters */

  struct pointer_hash region_to_counter; /* counters by region */
  struct complex_counter *in_regions;    /* All counters */
  int num_region_counters;               /* Num counters */
};

/*
 * Relation state info -- used as intermediary representation before counting
 * is properly initialized.  It is simply a digested form of the parsed
 * information.  The relation is represented as an index into the
 * complex_species table of relations.
 */
struct macro_relation_state {
  struct macro_relation_state *next; /* link to next */
  struct species *mol;               /* species for clause */
  int relation;                      /* idx of relation */
  short invert;                      /* invert flag */
  short orient;                      /* orient for clause */
};

/*
 * Count request info -- used as intermediary representation before counting
 * is properly initialized.  It ties together the expression tree for the
 * counter with the info needed to build the rule table.
 */
struct macro_count_request {
  struct macro_count_request *next;            /* link to next */
  struct output_expression *paired_expression; /* pointer to tied expression */
  struct complex_species *the_complex; /* pointer to complex owning this count
                                          */
  struct species *subunit_state; /* species of reference subunit */
  struct macro_relation_state *relation_states; /* list of relation states for
                                                   this count */
  struct sym_entry *location; /* "where" info for count */
  short master_orientation;   /* macromol orientation for this count */
  short subunit_orientation;  /* orient of reference subunit */
};

/* Given a macromolecule subunit, find its index within the complex. */
int macro_subunit_index(struct abstract_molecule const *subunit);


#endif
