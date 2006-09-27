%{
  #include <stdio.h> 
  #include <string.h> 
  #include <stdlib.h>
  #include <unistd.h>
  #include <stdarg.h>
  #include <fcntl.h>
  #include "rng.h"
  #include "util.h"
  #include "mcell_structs.h"
  #include "strfunc.h"
  #include "argparse.h"

  #define YYPARSE_PARAM argparse_param
  #define YYLEX_PARAM argparse_param

  #define argpvp ((struct argparse_vars *)YYPARSE_PARAM)
  #define volp argpvp->vol
%}


%union {
int tok;
char *str;
double dbl;
} 


%{
  #include "arglex.flex.c"
%}


%pure_parser

%name-prefix="arg"
%output="argparse.bison.c"

%token <tok> REAL INTEGER HELP_OPT LOG_FILE_OPT LOG_FREQ_OPT FILE_NAME
%token <tok> INFO_OPT SEED_OPT ITERATIONS_OPT CHECKPOINT_OPT ERR_FILE_OPT
%token <tok> EOF_TOK
%type <dbl> int_arg
/*
%type <dbl> real_arg num_arg 
*/

%right '='
%left '+' '-'
%left '*' '/'
%left UNARYMINUS

%%

option_format: 
	option_list
;

option_list: option
	| option_list option
;

option: HELP_OPT
{
  return(1);
}
	| INFO_OPT
{
  volp->info_opt=1;
}
	| mdl_infile_cmd
{
}
	| seed_cmd 
{
}
	| iterations_cmd 
{
}
	| checkpoint_cmd
{
}
	| log_file_cmd
{
}
	| log_freq_cmd
{
}
	| err_file_cmd
{
}
	| EOF_TOK
{
  if (volp->mdl_infile_name==NULL) {
    sprintf(argpvp->arg_err_msg,"No MDL file name specified");
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
  return(0);
};

mdl_infile_cmd: FILE_NAME
{
  if (volp->mdl_infile_name!=NULL) {
    sprintf(argpvp->arg_err_msg,"Multiple MDL file names specified: %s",argpvp->cval);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
  volp->mdl_infile_name=strdup(argpvp->cval);
  if (volp->mdl_infile_name == NULL) {
    sprintf(argpvp->arg_err_msg,"File '%s', Line %ld: Out of memory while parsing command line arguments: %s",__FILE__, (long)__LINE__, argpvp->cval);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
  free((void *)argpvp->cval);
  argpvp->cval = NULL;
};


seed_cmd: SEED_OPT int_arg
{
  volp->seed_seq=(int) $<dbl>2;
#ifdef USE_RAN4
  if (volp->seed_seq < 1 || volp->seed_seq > 3000) {
    sprintf(argpvp->arg_err_msg,"Random sequence number %d not in range 1 to 3000",
      volp->seed_seq);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
#endif
};

iterations_cmd: ITERATIONS_OPT int_arg
{
  volp->iterations=(long long) $<dbl>2;
  if (volp->iterations < 0) {
    sprintf(argpvp->arg_err_msg,"Iterations %lld is less than 0",
      volp->iterations);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
};


checkpoint_cmd: CHECKPOINT_OPT FILE_NAME
{
  volp->chkpt_infile = strdup(argpvp->cval);
  if (volp->chkpt_infile == NULL) {
    sprintf(argpvp->arg_err_msg,"File '%s', LINE %ld: Out of memory while parsing command line arguments: %s",__FILE__, (long)__LINE__, argpvp->cval);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
  free((void *)argpvp->cval);
  argpvp->cval = NULL;
  if ((volp->chkpt_infs=fopen(volp->chkpt_infile,"rb"))==NULL) {
    sprintf(argpvp->arg_err_msg,"Cannot open input checkpoint file: %s",volp->chkpt_infile);
    argerror(argpvp->arg_err_msg,argpvp);
    volp->chkpt_init=1;
    return(1);  
  }
  volp->chkpt_init=0;
  volp->chkpt_flag = 1;
  fclose(volp->chkpt_infs);
  volp->chkpt_infs=NULL;

};

log_file_cmd: LOG_FILE_OPT FILE_NAME
{
  volp->log_file_name=strdup(argpvp->cval);
  if (volp->log_file_name == NULL) {
    sprintf(argpvp->arg_err_msg,"File '%s', Line %ld: Out of memory while parsing command line arguments: %s", __FILE__, (long)__LINE__, argpvp->cval);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
  free((void *)argpvp->cval);
  argpvp->cval = NULL;
  if ((volp->log_file=fopen(volp->log_file_name,"w"))==NULL) {
    sprintf(argpvp->arg_err_msg,"Cannot open output log file: %s",
      volp->log_file_name);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
  if(volp->err_file == NULL){
     volp->err_file = volp->log_file;
  }
};

log_freq_cmd: LOG_FREQ_OPT int_arg
{
  volp->log_freq=(int) $<dbl>2;
  if (volp->log_freq<1)
  {
    argerror("Iteration report update interval must be at least 1 iteration.");
    return 1;
  }
};

err_file_cmd: ERR_FILE_OPT FILE_NAME
{
  volp->err_file_name=strdup(argpvp->cval);
  if (volp->err_file_name == NULL) {
    sprintf(argpvp->arg_err_msg,"File '%s', Line %ld: Out of memory while parsing command line arguments: %s", __FILE__, (long)__LINE__, argpvp->cval);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
  free((void *)argpvp->cval);
  argpvp->cval = NULL;
  if ((volp->err_file=fopen(volp->err_file_name,"w"))==NULL) {
    sprintf(argpvp->arg_err_msg,"Cannot open output error file: %s",
      volp->err_file_name);
    argerror(argpvp->arg_err_msg,argpvp);
    return(1);
  }
};

int_arg: INTEGER {$$=(double)argpvp->ival;}
;

/*
real_arg: REAL {$$=argpvp->rval;}
;

num_arg: INTEGER {$$=(double)argpvp->ival;}
	| REAL {$$=argpvp->rval;}
;
*/

%%



void argerror(char *s,...)
{
  va_list ap;
  FILE *log_file = NULL;
  struct argparse_vars *apvp = NULL;

  log_file=stderr;
  va_start(ap,s);
  apvp=va_arg(ap,struct argparse_vars *);
  if(apvp == NULL){
     fprintf(stderr,"\nFile '%s', Line %ld: error found.\n", __FILE__, (long)__LINE__);
     return;
  }
  va_end(ap);

  if (apvp->vol->log_file!=NULL) {
    log_file=apvp->vol->log_file;
  }

  fprintf(log_file,"\nMCell: command-line argument syntax error: %s\n",s);
  fflush(log_file);
  return;
}


#if 0
int argparse_init(int argc, char *argv[], struct volume *vol)
{
  struct argparse_vars arg_var;
  FILE *fs1,*fs2;
  int fildes[2];
  int i;


  arg_var.vol=vol;

  vol->log_freq=100;
  vol->log_file_name=NULL;
  vol->log_file=stdout;
  vol->err_file=stderr;
  vol->seed_seq=1;
  vol->info_opt=0;
  vol->mdl_infile_name=NULL;

  if ((arg_var.arg_err_msg=(char *)malloc(1024*sizeof(char)))==NULL) {
    fprintf(vol->log_file,"MCell: out of memory storing arg_err_msg\n");
    return(1);
  }

  if (pipe(fildes)) {
    fprintf(vol->log_file,"MCell: cannot open pipe\n");
    return(1);
  }
  if ((fs1 = fdopen(fildes[0],"r"))==NULL) {
    fprintf(vol->log_file,"MCell: cannot open pipe\n");
    return(1);
  }
  if ((fs2 = fdopen(fildes[1],"w"))==NULL) {
    fprintf(vol->log_file,"MCell: cannot open pipe\n");
    return(1);
  }

  for (i=1;i<argc;i++) {
    fprintf(fs2,"%s ",argv[i]);
    fflush(fs2);
  }
  fclose(fs2);

  argrestart(fs1);
  if (argparse((void *) &arg_var)) {
     return(1);
  }
  fclose(fs1);

  return(0);
}
#endif


int argparse_init(int argc, char *argv[], struct volume *vol)
{
  struct argparse_vars arg_var;
  struct yy_buffer_state *arg_input_buffer = NULL;
  char *tempstr = NULL, *arg_string = NULL;
  int i;


  arg_var.vol=vol;

  vol->log_freq=-1; /* No user-specified value */
  vol->log_file_name=NULL;
  vol->err_file_name=NULL;
  vol->log_file=stdout;
  vol->err_file=stderr;
  vol->seed_seq=1;
  vol->info_opt=0;
  vol->mdl_infile_name=NULL;

  if ((arg_var.arg_err_msg=(char *)malloc(1024*sizeof(char)))==NULL) {
    fprintf(vol->err_file, "File '%s', Line %ld: out of memory storing arg_err_msg\n", __FILE__, (long)__LINE__);
    return(1);
  }

  arg_string=strdup("");
  for (i=1;i<argc;i++) {
    if (i==1) {
      free(arg_string);
      arg_string = NULL;
      if ((arg_string=strdup(argv[i]))==NULL) {
        fprintf(vol->err_file,"File '%s', Line %ld: out of memory storing arg_string\n", __FILE__, (long)__LINE__);
        return(1);
      }
    }
    else {
      tempstr=arg_string; 
      if ((arg_string=my_strcat(arg_string," "))==NULL) {
        fprintf(vol->err_file,"File '%s', Line %ld: out of memory storing arg_string\n", __FILE__, (long)__LINE__);
        return(1);
      }
      free(tempstr);
      tempstr = NULL;

      tempstr=arg_string; 
      if ((arg_string=my_strcat(arg_string,argv[i]))==NULL) {
        fprintf(vol->err_file,"File '%s', Line %ld: out of memory storing arg_string\n", __FILE__, (long)__LINE__);
        return(1);
      }
      free(tempstr);
      tempstr = NULL;
    }
  }

  arg_input_buffer=arg_scan_string(arg_string);

  if (argparse((void *) &arg_var)) {
     return(1);
  }

  arg_delete_buffer(arg_input_buffer);

  return(0);
}
