#ifndef SYM_TABLE_H
#define SYM_TABLE_H

#include "mcell_structs.h"

#ifndef ISAAC64_H
/* These guys come in for free if we're using Jenkins' random numbers */
typedef uint32_t ub4;   /* unsigned 4-byte quantities */
typedef unsigned char ub1;        /* unsigned 1-byte quantities */
#endif


struct species *new_species(void);
struct object *new_object(void);
struct release_pattern *new_release_pattern(void);
struct rxn *new_reaction(void);
struct rxn_pathname *new_reaction_pathname(void);
struct region *new_region(void);
struct file_stream *new_filestream(void);

ub4 jenkins_hash(ub1 *sym,ub4 length);
unsigned long hash(char *sym);
struct sym_table *retrieve_sym(char *sym, unsigned short sym_type,
  struct sym_table **hashtab);
struct sym_table *store_sym(char *sym, unsigned short sym_type,
  struct sym_table **hashtab, void *data);
struct sym_table **init_symtab(int size);

#endif
