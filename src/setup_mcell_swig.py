#!/usr/bin/env python

# commands to run setup of swig files 
	# swig -python mcell.i  
	# vim mcell_swig_setup.py 

"""
setup.py file for SWIG example
"""

from distutils.core import setup, Extension


mcell_module = Extension('_mcellSwig',
                           sources=['mcellSwig_wrap.c', 'mcell_react_out.c', 'mcell_misc.c', 'util.c', 'argparse.c', 'mcell_objects.c', 'mcell_reactions.c', 'mcell_release.c', 'mcell_species.c', 'mcell_viz.c', 'mcell_surfclass.c', 'strfunc.c', 'mcell_init.c', 'chkpt.c', 'init.c', 'react_output.c', 'version_info.c', 'sched_util.c', 'wall_util.c', 'mem_util.c', 'react_trig.c', 'react_cond.c', 'count_util.c', 'vol_util.c', 'diffuse.c', 'vector.c', 'diffuse_util.c', 'sym_table.c', 'triangle_overlap.c', 'logging.c', 'grid_util.c', 'react_outc.c', 'react_util.c', 'react_outc_trimol.c', 'diffuse_trimol.c', 'isaac64.c', 'rng.c', 'viz_output.c', 'mcell_run.c', 'volume_output.c']

			)                           

setup (name = 'mcellSwig',
       version = '0.1',
       author      = "SWIG Docs",
       description = """Simple swig example from docs""",
       ext_modules = [mcell_module],
       py_modules = ["mcellSwig"],
       )
