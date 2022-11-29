/*=======================================================================
  A simple parser for "self" format

  The circuit format (called "self" format) is based on outputs of
  a ISCAS 85 format translator written by Dr. Sandeep Gupta.
  The format uses only integers to represent circuit information.
  The format is as follows:

1        2        3        4           5           6 ...
------   -------  -------  ---------   --------    --------
0 GATE   outline  0 IPT    #_of_fout   #_of_fin    inlines
                  1 BRCH
                  2 XOR(currently not implemented)
                  3 OR
                  4 NOR
                  5 NOT
                  6 NAND
                  7 AND

1 PI     outline  0        #_of_fout   0

2 FB     outline  1 BRCH   inline

3 PO     outline  2 - 7    0           #_of_fin    inlines




                                    Author: Chihang Chen
                                    Date: 9/16/94

=======================================================================*/

/*=======================================================================
  - Write your program as a subroutine under main().
    The following is an example to add another command 'lev' under main()

enum e_com {READ, PC, HELP, QUIT, LEV};
#define NUMFUNCS 5
int cread(), pc(), quit(), lev();
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", lev, CKTLD},
};

lev()
{
   ...
}
=======================================================================*/

#include <algorithm>
#include <cstring>
#include <ctype.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <bitset>
#include <utility>
using namespace std;

#define MAXLINE 81               /* Input buffer size */
#define MAXNAME 31               /* File name size */

#define Upcase(x) ((isalpha(x) && islower(x))? toupper(x) : (x))
#define Lowcase(x) ((isalpha(x) && isupper(x))? tolower(x) : (x))

enum e_com {READ, PC, HELP, QUIT, LOGICSIM, RFL, PFS, RTG, DFS};
enum e_state {EXEC, CKTLD};         /* Gstate values */
enum e_ntype {GATE, PI, FB, PO};    /* column 1 of circuit format */
enum e_gtype {IPT, BRCH, XOR, OR, NOR, NOT, NAND, AND};  /* gate types */

struct cmdstruc {
   char name[MAXNAME];        /* command syntax */
   int (*fptr)(char *cp);             /* function pointer of the commands */
   enum e_state state;        /* execution state sequence */
};

typedef struct n_struc {
   unsigned indx;             /* node index(from 0 to NumOfLine - 1 */
   unsigned num;              /* line number(May be different from indx */
   enum e_gtype type;         /* gate type */
   unsigned fin;              /* number of fanins */
   unsigned fout;             /* number of fanouts */
   struct n_struc **unodes;   /* pointer to array of up nodes */
   struct n_struc **dnodes;   /* pointer to array of down nodes */
   int level;                   /* level of the gate output */
   int value;                 /* value of the gate output */
   int f_value;
} NSTRUC;                     

/*----------------- Command definitions ----------------------------------*/
#define NUMFUNCS 9
int cread(char *cp), pc(char *cp), help(char *cp), quit(char *cp), logicsim(char *cp), rfl(char *cp), pfs(char *cp), rtg(char *cp), dfs(char *cp);
void allocate(), clear();
string gname(int tp);
struct cmdstruc command[NUMFUNCS] = {
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LOGICSIM", logicsim, CKTLD},
   {"RFL", rfl, CKTLD},
   {"PFS", pfs, CKTLD},
   {"RTG", rtg, CKTLD},
   {"DFS", dfs, CKTLD},
};

/*------------------------------------------------------------------------*/
enum e_state Gstate = EXEC;     /* global exectution sequence */
NSTRUC *Node;                   /* dynamic array of nodes */
NSTRUC **Pinput;                /* pointer to array of primary inputs */
NSTRUC **Poutput;               /* pointer to array of primary outputs */
int Nnodes;                     /* number of nodes */
int Npi;                        /* number of primary inputs */
int Npo;                        /* number of primary outputs */
int Done = 0;                   /* status bit to terminate program */
vector<int> node_queue;
/*------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: shell
description:
  This is the main program of the simulator. It displays the prompt, reads
  and parses the user command, and calls the corresponding routines.
  Commands not reconized by the parser are passed along to the shell.
  The command is executed according to some pre-determined sequence.
  For example, we have to read in the circuit description file before any
  action commands.  The code uses "Gstate" to check the execution
  sequence.
  Pointers to functions are used to make function calls which makes the
  code short and clean.
-----------------------------------------------------------------------*/
main()
{
   enum e_com com;
   int com_int;
   char cline[MAXLINE], wstr[MAXLINE], *cp;

   while(!Done) {
      printf("\nCommand>");
      fgets(cline, MAXLINE, stdin);
      if(sscanf(cline, "%s", wstr) != 1) continue;
      cp = wstr;
      while(*cp){
	*cp= Upcase(*cp);
	cp++;
      }
      cp = cline + strlen(wstr);
      com = READ;
      com_int = 0;
      while(com_int < NUMFUNCS && strcmp(wstr, command[com].name)) com = static_cast<e_com>(com_int++);
      if(com < NUMFUNCS) {
         if(command[com].state <= Gstate) (*command[com].fptr)(cp);
         else printf("Execution out of sequence!\n");
      }
      else system(cline);
   }
}

/*-----------------------------------------------------------------------
input: circuit description file name
output: nothing
called by: main
description:
  This routine reads in the circuit description file and set up all the
  required data structure. It first checks if the file exists, then it
  sets up a mapping table, determines the number of nodes, PI's and PO's,
  allocates dynamic data arrays, and fills in the structural information
  of the circuit. In the ISCAS circuit description format, only upstream
  nodes are specified. Downstream nodes are implied. However, to facilitate
  forward implication, they are also built up in the data structure.
  To have the maximal flexibility, three passes through the circuit file
  are required: the first pass to determine the size of the mapping table
  , the second to fill in the mapping table, and the third to actually
  set up the circuit information. These procedures may be simplified in
  the future.
-----------------------------------------------------------------------*/
int cread(char *cp)
{
   char buf[MAXLINE];
   int ntbl, *tbl, i, j, k, nd, tp, fo, fi, ni = 0, no = 0;
   FILE *fd;
   NSTRUC *np;

   sscanf(cp, "%s", buf);
   if((fd = fopen(buf,"r")) == NULL) {
      printf("File %s does not exist!\n", buf);
      return 1;
   }
   if(Gstate >= CKTLD) clear();
   Nnodes = Npi = Npo = ntbl = 0;
   while(fgets(buf, MAXLINE, fd) != NULL) {
      if(sscanf(buf,"%d %d", &tp, &nd) == 2) {
         if(ntbl < nd) ntbl = nd;
         Nnodes ++;
         if(tp == PI) Npi++;
         else if(tp == PO) Npo++;
      }
   }
   tbl = (int *) malloc(++ntbl * sizeof(int));

   fseek(fd, 0L, 0);
   i = 0;
   while(fgets(buf, MAXLINE, fd) != NULL) {
      if(sscanf(buf,"%d %d", &tp, &nd) == 2) tbl[nd] = i++;
   }
   allocate();

   fseek(fd, 0L, 0);
   while(fscanf(fd, "%d %d", &tp, &nd) != EOF) {
      np = &Node[tbl[nd]];
      np->num = nd;
      np->value = -1;
      if(tp == PI) Pinput[ni++] = np;
      else if(tp == PO) Poutput[no++] = np;
      switch(tp) {
         case PI:
         case PO:
         case GATE:
            fscanf(fd, "%d %d %d", &np->type, &np->fout, &np->fin);
            break;
         
         case FB:
            np->fout = np->fin = 1;
            fscanf(fd, "%d", &np->type);
            break;

         default:
            printf("Unknown node type!\n");
            exit(-1);
         }
      np->unodes = (NSTRUC **) malloc(np->fin * sizeof(NSTRUC *));
      np->dnodes = (NSTRUC **) malloc(np->fout * sizeof(NSTRUC *));
      for(i = 0; i < np->fin; i++) {
         fscanf(fd, "%d", &nd);
         np->unodes[i] = &Node[tbl[nd]];
         }
      for(i = 0; i < np->fout; np->dnodes[i++] = NULL);
      }
   for(i = 0; i < Nnodes; i++) {
      for(j = 0; j < Node[i].fin; j++) {
         np = Node[i].unodes[j];
         k = 0;
         while(np->dnodes[k] != NULL) k++;
         np->dnodes[k] = &Node[i];
         }
      }
   fclose(fd);
   Gstate = CKTLD;
   printf("==> OK\n");

   return 0;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main
description:
  The routine prints out the circuit description from previous READ command.
-----------------------------------------------------------------------*/
int pc(char *cp)
{
   int i, j;
   NSTRUC *np;
   // char *gname();
   
   printf(" Node   Type \tIn     \t\t\tOut    \n");
   printf("------ ------\t-------\t\t\t-------\n");
   for(i = 0; i<Nnodes; i++) {
      np = &Node[i];
      printf("\t\t\t\t\t");
      for(j = 0; j<np->fout; j++) printf("%d ",np->dnodes[j]->num);
      printf("\r%5d  %s\t", np->num, gname(np->type).c_str());
      for(j = 0; j<np->fin; j++) printf("%d ",np->unodes[j]->num);
      printf("\n");
   }
   printf("Primary inputs:  ");
   for(i = 0; i<Npi; i++) printf("%d ",Pinput[i]->num);
   printf("\n");
   printf("Primary outputs: ");
   for(i = 0; i<Npo; i++) printf("%d ",Poutput[i]->num);
   printf("\n\n");
   printf("Number of nodes = %d\n", Nnodes);
   printf("Number of primary inputs = %d\n", Npi);
   printf("Number of primary outputs = %d\n", Npo);

   return 0;
}

/*-----------------------------------------------------------------------
input: output_values (initial - may be empty), also uses the node_queue
output: output_values (after evaluation)
called by: logicsim
description:
  Event-Driven Simulation
  The routine evaluates the circuit and returns the PO values for the updated PI values.
-----------------------------------------------------------------------*/
map<int,int> eval_gates (map<int,int> &output_values) {

   int i, j, old_value;
   NSTRUC *np;

   //now, go through the elements in the node_queue and evaluate
   while(node_queue.size() > 0) {
      np = &Node[node_queue[0]];    // read the node data
      old_value = np->value;     // save old value (to be compared with evaluated value to determine if the value has changed)
      switch(np->type) {
         case 0:  // PI
            for (i = 0; i < np->fout; i++) {
                  node_queue.push_back(np->dnodes[i]->indx); // add downstream elements to the queue
               }
            break;      
         case 1:  // BRANCH
            np->value = np->unodes[0]->value;
            break;
         case 2:  // XOR
            np->value = np->unodes[0]->value; 
            for (j = 1; j < np->fin; j++) {
               if (np->value == -1 || np->unodes[j]->value == -1) {
                  np->value = -1;
                  break;
               }
               else {
                  np->value = np->value ^ np->unodes[j]->value;
               }
            }
            break; 
         case 3:  // OR
            np->value = 0;  
            for (j = 0; j < np->fin; j++) {
               if (np->unodes[j]->value == 1) {
                  np->value = 1;
                  break;
               }
               else if (np->unodes[j]->value == -1) {
                  np->value = -1;
               }
            } 
            break;
         case 4:  // NOR
            np->value = 1;    
            for (j = 0; j < np->fin; j++) {
               if (np->unodes[j]->value == 1) {
                  np->value = 0;
                  break;
               }
               else if (np->unodes[j]->value == -1) {
                  np->value = -1;
               }
            }
            break; 
         case 5: // NOT
            if (np->unodes[0]->value == -1) {
               np->value = -1;
            }
            else {
               np->value = !(np->unodes[0]->value); 
            }
            break; 
         case 6:  // NAND
            np->value = 0;    
            for (j = 0; j < np->fin; j++) {
               if (np->unodes[j]->value == 0) {
                  np->value = 1;
                  break;
               }
               else if (np->unodes[j]->value == -1) {
                  np-> value = -1;
               }
            }
            break; 
         case 7:  // AND
            np->value = 1;    
            for (j = 0; j < np->fin; j++) {
               if (np->unodes[j]->value == 0) {
                  np->value = 0;
                  break;
               }
               else if (np->unodes[j]->value == -1) {
                  np-> value = -1;
               }
            }
            break; 
      }

      node_queue.erase(node_queue.begin());  // remove the first element as it has been evaluated
      if (old_value != np->value || old_value == -1) {
         for (i = 0; i < np->fout; i++) {
            node_queue.push_back(np->dnodes[i]->indx); // add downstream elements to the queue
         }
      }

      if (np->fout == 0) {
         output_values[np->num] = np->value;    // modify the updated PO value
      }
   }

   return output_values;
}

/*-----------------------------------------------------------------------
input: PI pattern file
output: PO output file
called by: main
description:
  The routine evaluates the circuit and prints the PO values into a file.
-----------------------------------------------------------------------*/
int logicsim(char *cp)
{
   int i, j;
   NSTRUC *np;
   char in_buf[MAXLINE], out_buf[MAXLINE];
   sscanf(cp, "%s %s", in_buf, out_buf);

   vector<vector<int> > input_patterns;
   vector<int> input_pattern_line;

   ifstream input_file;
   input_file.open(in_buf);
   string input_line, token;
   if ( input_file.is_open() ) {
      while ( input_file ) {
         getline (input_file, input_line);   // read line from pattern file
         stringstream X(input_line);
         while (getline(X, token, ',')) {
            input_pattern_line.push_back(stoi(token));   // create a vector of ints with all elements in the line
         }
         input_patterns.push_back(input_pattern_line);
         input_pattern_line.clear();
      }
      input_file.close();
   }
   else {
      cout << "Couldn't open file\n";
   }

   map<int, int> output_values;     // dictionary to hold PO values

   ofstream output_file;
   output_file.open(out_buf);

   // event driven simulation
   int k, l;
   for (k = 1; k < input_patterns.size()-1; k++) {    // iterate over all the rows
      // cout << "Iterating over row " << k << endl; 
      for (i = 0; i < input_patterns[0].size(); i++) {     // iterate over all the PIs in the Kth row
         // cout << "Evaluating PI " << input_patterns[0][i] << endl;
         for (j = 0; j < Nnodes; j++){    // iterate over all the nodes
            if (Node[j].num == input_patterns[0][i]) {
               // cout << "Previous value of PI is " << input_patterns[k-1][i] << " - New value is " << input_patterns[k][i] << endl;
               Node[j].value = input_patterns[k][i];
               if (k == 1) {
                  node_queue.push_back(j);
                  // cout << Node[j].num << endl;
               } else if (input_patterns[k-1][i] != input_patterns[k][i]) {
                  for (l = 0; l < Node[j].fout; l++) {
                     node_queue.push_back(Node[j].dnodes[l]->indx); // add elements downstream of PI to the queue
                  }
               }
               break;
            }
         }
      }

      output_values = eval_gates(output_values);      // function call to evaluate the circuit

      // print outputs
      // for( map<int, int>::iterator i= output_values.begin(); i != output_values.end(); i++)
      // {
      //    cout << (*i).first << ": " << (*i).second << endl;
      // }

      if ( output_file ) {
         // PO node numbers
         if ( k == 1) {
            i = 0;
            for (auto const& element : output_values) {
               if (i == 1) {
                  output_file << ",";
               }
               i = 1;
               output_file << element.first;
            }
            output_file << endl;
         }
         // PO values
         i = 0;
         for (auto const& element : output_values) {
            if (i == 1) {
               output_file << ",";
            }
            i = 1;
            output_file << element.second;
         }
         output_file << endl;
      }
      else {
         cout << "Couldn't create file\n";
      }

   }
   cout << "OK" << endl;
   return 0;
}

/*-----------------------------------------------------------------------
input: none
output: PO output file
called by: main
description:
  The routine reduces the fault list using the checkpoint theorem.
-----------------------------------------------------------------------*/
int rfl(char *cp)
{
   int i, j;
   NSTRUC *np;

   char out_buf[MAXLINE];
   sscanf(cp, "%s", out_buf);

   vector<pair<int, int> > fault_list;     // dictionary to hold PO values
   pair<int,int> fault;    // temporarily holds the faults

   ofstream output_file;
   output_file.open(out_buf);

   // checkpoint theorem
   for (i = 0; i < Nnodes; i++){    // iterate over all the nodes
      np = &Node[i];
      if (np->type == 0 || np->type == 1) {     // check if the node type is PI or BRANCH
         fault.first = np->num;
         for (j = 0; j < 2; j++) {     // assign value of 0 and 1 to the faulty node
            fault.second = j; 
            fault_list.push_back(fault);     // add the fault to the fault_list
         }
      }
   }

   // write to file
   if ( output_file ) {
      for (i = 0; i < fault_list.size(); i++) {
         output_file << fault_list[i].first << "@" << fault_list[i].second << endl;
      }
   } else {
      cout << "Couldn't create file\n";
   }

   cout << "OK" << endl;
   return 0;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: pfs
description:
  The routine levelizes the previously read circuit and pushes nodes into the node_queue in the order of evaluation.
-----------------------------------------------------------------------*/
void lev()
{
   int i,j,k,l,level;
   int Nodes_done =0;
   NSTRUC *np;
   node_queue.clear();

   // Start with primary inputs
   for(i=0; i<Nnodes; i++){
	np=&Node[i];
      if(np->type==0){
         np->level=0;
         node_queue.push_back(np->indx);
         Nodes_done++;
      }
      else{
        np->level = -1;
      }
  }
   // All primary inputs have now been assigned level 0
   // All other nodes are at level -1 (undefined)

   while(Nodes_done!=Nnodes){
      for(j=0; j<Nnodes; j++){
         np=&Node[j];
         if(np->level == -1) {    // Node is at an undefined level
            if((np->type == 1) && (np->unodes[0]->level != -1)) {     // Node is a branch and upstream node is not at undefined level
               np->level=np->unodes[0]->level+1;
               node_queue.push_back(np->indx);
               Nodes_done++;				 
            }
            else if(np->type>1) {    // Node is a gate
               int flag=1;
               level=0;
               for(k=0; k<np->fin; k++) {
                  if(np->unodes[k]->level != -1){
                     if (np->unodes[k]->level>level) {      // upstream node is not at undefined level and at a greater level than recorded level
                        level=np->unodes[k]->level;
                     }
                  }
                  else {
                     flag=0;	
                     break;
                  }
               }
               if (flag==1){
                  np->level=level+1;
                  node_queue.push_back(np->indx);
                  Nodes_done++;
               }
            }
         }
      }
   }
}


int dfs(char *cp) {

  int i, j, index;
  NSTRUC *np;
  // vector<pair<int, int> > fault_list;
  set<pair<int, int>> det_fault_list; // set / insert
map<int,set<pair<int,int>>> all_fault;
  pair<int, int> f_val;
  pair<int, int> temp;

  char in_buf[MAXLINE], out_buf[MAXLINE];
  sscanf(cp, "%s %s", in_buf, out_buf);
  logicsim(cp);
  lev();
  // ofstream output_file;
  // output_file.open(out_buf);
  // rfl(in_buf);
  for (i = 0; i < node_queue.size(); i++) {
    np = &Node[node_queue[i]]; 
    if (np->type == 0) {
      f_val.first = np->num;
      if (np->value == 0) {
        f_val.second = 1;
        np->f_value = 1;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      } else 
	  {
        f_val.second = 0;
        np->f_value = 0;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      }
    } 
	else if (np->type == 1) {     // branch
      f_val.first = np->num;
      f_val.second = np->unodes[0]->f_value;
      np->f_value = np->unodes[0]->f_value;
det_fault_list = all_fault[np->unodes[0]->indx];
      det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
    } 
	else if (np->type == 2) {     // xor
      // xor or nor not nand and
      for (j = 0; j < np->fin; j++) {
       // f_val.first = np->unodes[j]->num;
        //f_val.second = np->unodes[j]->f_value;
det_fault_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
        //det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
      }
      f_val.first = np->num;
      if (np->value == 0) {
        f_val.second = 1;
        np->f_value = 1;
      } 
	  else {
        f_val.second = 0;
        np->f_value = 0;
      }
      det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
    } 
	else if (np->type == 3) {     // or
      if (np->value == 0) {
        f_val.second = 1;
        //np->f_value = 1;
        for (j = 0; j < np->fin; j++) {
		det_fault_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());

          //f_val.first = np->unodes[j]->num;
          //det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        }
        f_val.first = np->num;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      } 
	  else {
        int count = 0;
	//all_fault.erase(np->indx);
        for (j = 0; j < np->fin; j++) {
          if (np->unodes[j]->value == 1) {
            f_val.first = np->unodes[j]->num;
			index = np->unodes[j]->indx;
            f_val.second = 0;
            count++;
          }
        }
        if (count == 1) {
		det_fault_list.insert(all_fault[index].begin(),all_fault[index].end());	

//det_fault_list.insert(f_val)
          //det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        }
        f_val.first = np->num;
        f_val.second = 0;
        np->f_value = 0;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      }
    } 
	else if (np->type == 4) {     // nor
      if (np->value == 1) {
        //f_val.second = 1;
        for (j = 0; j < np->fin; j++) {
		det_fault_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
 //         f_val.first = np->unodes[j]->num;
   //       det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        }
        f_val.first = np->num;
        f_val.second = 0;
        np->f_value = 0;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      } 
	  else {
        int count = 0;
	//all_fault.erase(np->indx);
        for (j = 0; j < np->fin; j++) {
          if (np->unodes[j]->value == 1) {
            f_val.first = np->unodes[j]->num;
			index = np->unodes[j]->indx;
            f_val.second = 0;
            count++;
          }
        }
        if (count == 1) {
//det_fault_list = all_fault[np->unodes[0]->indx];
		det_fault_list.insert(all_fault[index].begin(),all_fault[index].end());	

//          det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        }
        f_val.first = np->num;
        f_val.second = 1;
        np->f_value = 1;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      }
    } 
	else if (np->type == 5) {     // not    
      f_val.first = np->unodes[0]->num;
det_fault_list = all_fault[np->unodes[0]->indx];
      if (np->unodes[0]->value == 0) {
        //f_val.second = 1;
        //det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        f_val.first = np->num;
        f_val.second = 0;
        np->f_value = 0;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      } 
	  else {
       // f_val.second = 0;
       // det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        f_val.first = np->num;
        f_val.second = 1;
        np->f_value = 1;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      }
    }
	else if (np->type == 6) {     // nand
      if (np->value == 0) {
  //      f_val.second = 0;
        for (j = 0; j < np->fin; j++) {
		det_fault_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
//          f_val.first = np->unodes[j]->num;
//          det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        }
        f_val.first = np->num;
        f_val.second = 1;
        np->f_value = 1;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      }
	  else {
        int count = 0;
	//all_fault.erase(np->indx);
        for (j = 0; j < np->fin; j++) {
          if (np->unodes[j]->value == 0) {
            f_val.first = np->unodes[j]->num;
			index = np->unodes[j]->indx;
            f_val.second = 1;
            count++;
          }
        }
        if (count == 1) {
//det_fault_list = all_fault[np->unodes[0]->indx];
		det_fault_list.insert(all_fault[index].begin(),all_fault[index].end());	
          //det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        }
        f_val.first = np->num;
        f_val.second = 0;
        np->f_value = 0;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      }
    } 
	else if (np->type == 7) {         // and
      if (np->value == 1) {
        f_val.second = 0;
//det_fault_list = all_fault[np->unodes[0]->indx];
        for (j = 0; j < np->fin; j++) {
		det_fault_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());

//          f_val.first = np->unodes[j]->num;
  //        det_fault_list.insert(f_val);
//all_fault[np->indx]=det_fault_list;
        }
        f_val.first = np->num;
        f_val.second = 0;
        np->f_value = 0;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      }
	  else {
        int count = 0;
	//all_fault.erase(np->indx);
        for (j = 0; j < np->fin; j++) {
          if (np->unodes[j]->value == 0) {
            f_val.first = np->unodes[j]->num;
			index = np->unodes[j]->indx;
            f_val.second = 1;
            count++;
          }
        }
        if (count == 1) {
//det_fault_list = all_fault[np->unodes[0]->indx];
  //        det_fault_list.insert(f_val);
 		det_fault_list.insert(all_fault[index].begin(),all_fault[index].end());	
//all_fault[np->indx]=det_fault_list;
        }

        f_val.first = np->num;
        f_val.second = 1;
        np->f_value = 1;
        det_fault_list.insert(f_val);
all_fault[np->indx]=det_fault_list;
      }
    }
  
  }
  /*
  sort(det_fault_list.begin(),det_fault_list.end());
  vector<int>::iterator it =
  unique(det_fault_list.begin(),det_fault_list.end());
  det_fault_list.resize(distance(det_fault_list.begin(),it));


      std::vector<pair<int,int> > result; // Will contain the symmetric
  difference std::set_symmetric_difference(fault_list.begin(), fault_list.end(),
                                    det_fault_list.begin(),
  det_fault_list.end(), std::back_inserter(result));
  */
  // write to file
  ofstream output_file;
  output_file.open(out_buf);
//cout<<"here"<<endl;
  if (output_file) {
    for (auto const &element : all_fault[np->indx]) {
      output_file << element.first << "@" << element.second << endl;
    }
  } else {
    cout << "Couldn't create file\n";
  }

  cout << "OK" << endl;
  return 0;
}

/*-----------------------------------------------------------------------
input: test patterns, fault list
output: detectable faults list
called by: main
description:
  The routine evaluates the circuit and determines the faults that can be detected for a given test pattern.
  - levlize and add nodes to node_queue

  - for each row in input pattern file read input pattern to update values
  -- get the fault list vector<pair<int,int>>
  -- for each iteration (faults/width of int)
  --- create a vector<pair<int,int>> of 32/64 to represent each bit and the corresponding fault
  --- evaluate each node - return an int
  --- check if the node is in the fault list (read the fault list)
  --- = if in the fault list, then modify the corresponding bit
  --- store each node as an int in a map<int,int> (node->num, bits vector)
-----------------------------------------------------------------------*/
int pfs(char *cp)
{
   int i, j;
   NSTRUC *np;
   char in_pattern_buf[MAXLINE], in_faults_buf[MAXLINE], out_buf[MAXLINE];
   sscanf(cp, "%s %s %s", in_pattern_buf, in_faults_buf, out_buf);

   // read input patterns
   vector<vector<int> > input_patterns;
   vector<int> input_pattern_line;
   ifstream input_file;
   input_file.open(in_pattern_buf);
   string input_line, token;
   if ( input_file.is_open() ) {
      while ( input_file ) {
         getline (input_file, input_line);   // read line from pattern file
         stringstream X(input_line);
         while (getline(X, token, ',')) {
            input_pattern_line.push_back(stoi(token));
         }
         if (input_line != "") {
            input_patterns.push_back(input_pattern_line);
         }
         input_pattern_line.clear();
      }
      input_file.close();
   }
   else {
      cout << "Couldn't open file\n";
   }

   // read fault list
   vector<pair<int,int> > fault_list;
   pair<int,int> fault;
   ifstream fault_file;
   fault_file.open(in_faults_buf);
   string fault_line;
   if ( fault_file.is_open() ) {
      while ( fault_file ) {
         getline (fault_file, fault_line);   // read line from fault list file
         stringstream X(fault_line);
         i = 0;
         while (getline(X, token, '@')) {
            if (i == 0 ) {
               fault.first = (stoi(token));
               i = 1;
            } else {
               fault.second = stoi(token);
            }
         }
         if (fault_line != "") {
            fault_list.push_back(fault);
         }
      }
      fault_file.close();
   }
   else {
      cout << "Couldn't open file\n";
   }

   // levelize
   lev();

   set<pair<int,int> > detected_faults;     // stores the final faults to be written out
   vector<pair<int,int> > bit_faults;      // stores the bit position of each fault
   bitset<sizeof(int)*8> value;     // used to easily manipulate bits in the value
   int k, l, m;
   int expected_value;
   for (k = 1; k < input_patterns.size(); k++) {    // iterate over all the rows of test patterns
      // iterate over the faults and add to the respective bit position
      for (l = 0; l < fault_list.size(); l = l + (sizeof(int)*8) - 1) {
         bit_faults.clear();
         bit_faults.push_back(make_pair(-1,-1));      // 0th element is used for fault-free circuit
         for (m = 0; (m < (sizeof(int)*8)-1 && m < (fault_list.size() - ((sizeof(int)*8)-1)*l)); m++) {
            bit_faults.push_back(fault_list[l+m]);
         }
         // evaluate the gates and inject faults
         for (i = 0; i < input_patterns[0].size(); i++) {     // iterate over all the PIs in the Kth row
            for (j = 0; j < Nnodes; j++){    // iterate over all the nodes
               if (Node[j].num == input_patterns[0][i]) {      // set PI to input pattern
                  if (input_patterns[k][i] == 0) {
                     value.reset();
                  } else {
                     value.set();
                  }
                  Node[j].value = value.to_ulong();
               }
            }
         }

         NSTRUC *np;
         //now, go through the elements in the node_queue and evaluate
         for (i = 0; i < node_queue.size(); i++) {
            np = &Node[node_queue[i]];    // read the node data
            switch(np->type) {
               case 0:  // PI
                  break;      
               case 1:  // BRANCH
                  np->value = np->unodes[0]->value;
                  break;
               case 2:  // XOR
                  np->value = np->unodes[0]->value; 
                  for (j = 1; j < np->fin; j++) {
                     np->value = np->value ^ np->unodes[j]->value;
                  }
                  break; 
               case 3:  // OR
                  np->value = 0;  
                  for (j = 0; j < np->fin; j++) {
                     np->value = np->value | np->unodes[j]->value;
                  } 
                  break;
               case 4:  // NOR
                  np->value = 0;    
                  for (j = 0; j < np->fin; j++) {
                     np->value = np->value | np->unodes[j]->value;
                  }
                  np->value = ~np->value;
                  break; 
               case 5: // NOT
                  np->value = ~(np->unodes[0]->value); 
                  break; 
               case 6:  // NAND
                  np->value = -1;      // initialize all bit positions to 1
                  for (j = 0; j < np->fin; j++) {
                     np->value = np->value & np->unodes[j]->value;
                  }
                  np->value = ~np->value;
                  break; 
               case 7:  // AND
                  np->value = -1;      // initialize all bit positions to 1
                  for (j = 0; j < np->fin; j++) {
                     np->value = np->value & np->unodes[j]->value;
                  }
                  break; 
            }

            // inject fault
            value = bitset<sizeof(int)*8>(np->value);

            // check if node has fault in bit_faults
            for (j = 0; j<bit_faults.size(); j++) {
               if (bit_faults[j].first == np->num) {
                  value.set(j,bit_faults[j].second);
               }
            }
            np->value = value.to_ulong();
            if (np->fout == 0) {
               expected_value = value[0];
               for (j = 1; j < sizeof(int)*8; j++) {
                  if (value.test(j) != expected_value) {
                     detected_faults.insert(bit_faults[j]);
                  }
               }
            }
         }
      }
   }
   
   ofstream output_file;
   output_file.open(out_buf);

   if ( output_file ) {
      for (auto const& element : detected_faults) {
         output_file << element.first << "@" << element.second << endl;
      }
   }
   else {
      cout << "Couldn't create file\n";
   }

   cout << "OK" << endl;
   return 0;
}

/*-----------------------------------------------------------------------
input: none
output: PO output file
called by: main
description:
  The routine generates random test patterns
-----------------------------------------------------------------------*/
int rtg(char *cp)
{
   int i, j;
   NSTRUC *np;

   char ntot_buf[MAXLINE], nTFCR_buf[MAXLINE], test_pattern_buf[MAXLINE], fc_buf[MAXLINE];
   sscanf(cp, "%s %s %s %s", ntot_buf, nTFCR_buf, test_pattern_buf,fc_buf);

   int ntot = stoi(ntot_buf);
   int nTFCR = stoi(nTFCR_buf);

   vector<pair<int, int> > fault_list;     // dictionary to hold PO values
   pair<int,int> fault;    // temporarily holds the faults

   vector<int> PI;
   vector<int> input_values;
   vector<vector<int> > test_patterns;

   // get all faults
   for (i = 0; i < Nnodes; i++){    // iterate over all the nodes
      np = &Node[i];

      // add to PI vector
      if (np->fin == 0) {
         PI.push_back(np->num);
      }

      // add faults
      fault.first = np->num;
      for (j = 0; j < 2; j++) {     // assign value of 0 and 1 to the faulty node
         fault.second = j; 
         fault_list.push_back(fault);     // add the fault to the fault_list
      }
   }

   ofstream output_test_pattern_file;
   output_test_pattern_file.open(test_pattern_buf);
   // print test patterns to a file
   int flag = 0;
   if ( output_test_pattern_file ) {
      for (i = 0; i < PI.size(); i++) {
         if (flag == 0) {
            output_test_pattern_file << PI[i];
            flag = 1;
         } else {
            output_test_pattern_file << "," << PI[i];
         }
      }
      flag = 0;
      output_test_pattern_file << endl;
   } else {
      cout << "Couldn't create file\n";
   }
   
   ofstream output_fc_file;
   output_fc_file.open(fc_buf);

   set<pair<int,int> > detected_faults;

   int test_patterns_generated = 0;
   srand(time(0));
   while (test_patterns_generated != ntot) {
      test_patterns.clear();
      test_patterns.push_back(PI);
      for (i = 0; i < nTFCR; i++) {
         for (j = 0; j < PI.size(); j++) {
            input_values.push_back(rand()%2);
         }
         test_patterns_generated++;
         test_patterns.push_back(input_values);
         input_values.clear();
      }
      
      // write the fault list and test patterns to temp files inorder to pass them to PFS
      ofstream output_file;
      output_file.open("fault_list_temp.txt");
      if ( output_file ) {
         for (i = 0; i < fault_list.size(); i++) {
            output_file << fault_list[i].first << "@" << fault_list[i].second << endl;
         }
      } else {
         cout << "Couldn't create file\n";
      }
      output_file.close();
      
      flag = 0;
      output_file.open("test_pattern_temp.txt");
      if ( output_file ) {
         for (i = 0; i < test_patterns.size(); i++) {
            for (j = 0; j < test_patterns[0].size(); j++) {
               if (flag == 0) {
                  output_file << test_patterns[i][j];
                  flag = 1;
               } else {
                  output_file << "," << test_patterns[i][j];
               }
            }
            output_file << endl;
            flag = 0;
         }
      } else {
         cout << "Couldn't create file\n";
      }
      output_file.close();

      string pfs_arguments = "test_pattern_temp.txt fault_list_temp.txt detected_faults_temp.txt";
      pfs(strdup(pfs_arguments.c_str()));

      // read fault list
      ifstream fault_file;
      fault_file.open("detected_faults_temp.txt");
      string fault_line, token;
      if ( fault_file.is_open() ) {
         while ( fault_file ) {
            getline (fault_file, fault_line);   // read line from fault list file
            stringstream X(fault_line);
            i = 0;
            while (getline(X, token, '@')) {
               if (i == 0 ) {
                  fault.first = (stoi(token));
                  i = 1;
               } else {
                  fault.second = stoi(token);
               }
            }
            if (fault_line != "") {
               detected_faults.insert(fault);
            }
         }
         fault_file.close();
      }
      else {
         cout << "Couldn't open file\n";
      }

      // print test patterns to a file
      flag = 0;
      if ( output_test_pattern_file ) {
         for (i = 1; i < test_patterns.size(); i++) {
            for (j = 0; j < test_patterns[0].size(); j++) {
               if (flag == 0) {
                  output_test_pattern_file << test_patterns[i][j];
                  flag = 1;
               } else {
                  output_test_pattern_file << "," << test_patterns[i][j];
               }
            }
            output_test_pattern_file << endl;
            flag = 0;
         }
      } else {
         cout << "Couldn't create file\n";
      }

      // print FC report
      if ( output_fc_file ) {
         output_fc_file << fixed << setprecision(2) << detected_faults.size()*100.0/fault_list.size() << endl;
      } else {
         cout << "Couldn't create file\n";
      }

   }
   cout << "OK" << endl;
   return 0;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
  The routine prints ot help inormation for each command.
-----------------------------------------------------------------------*/
int help(char *cp)
{
   printf("READ filename - ");
   printf("read in circuit file and creat all data structures\n");
   printf("PC - ");
   printf("print circuit information\n\n");
   printf("The following commands are expecting that the folders for phase-2 can easily be accessed from the working directory\n\n");
   printf("LOGICSIM - ");
   printf("simulate the logic\n");
   printf("> logicsim LOGICSIM/c17test.txt c17.out\n");
   printf("RFL - ");
   printf("reduces the fault list - prints the RFL to the output file (c17_rfl.out in the following command)\n");
   printf("> rfl c17_rfl.out\n");
   printf("PFS - ");
   printf("performs parallel fault simulation\n");
   printf("> pfs P_D_FS/input/c17_test_in.txt RFL/c17_rfl.txt c17.out\n");
   printf("RTG - ");
   printf("generates random test patterns and calculates FC\n");
   printf("> rtg ntot nTFCR test_patterns.out fc.out\n");
   printf("HELP - ");
   printf("print this help information\n");
   printf("QUIT - ");
   printf("stop and exit\n");

   return 0;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: main 
description:
  Set Done to 1 which will terminates the program.
-----------------------------------------------------------------------*/
int quit(char *cp)
{
   Done = 1;
   
   return 0;
}

/*======================================================================*/

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
  This routine clears the memory space occupied by the previous circuit
  before reading in new one. It frees up the dynamic arrays Node.unodes,
  Node.dnodes, Node.flist, Node, Pinput, Poutput, and Tap.
-----------------------------------------------------------------------*/
void clear()
{
   int i;

   for(i = 0; i<Nnodes; i++) {
      free(Node[i].unodes);
      free(Node[i].dnodes);
   }
   free(Node);
   free(Pinput);
   free(Poutput);
   node_queue.clear();
   Gstate = EXEC;
}

/*-----------------------------------------------------------------------
input: nothing
output: nothing
called by: cread
description:
  This routine allocatess the memory space required by the circuit
  description data structure. It allocates the dynamic arrays Node,
  Node.flist, Node, Pinput, Poutput, and Tap. It also set the default
  tap selection and the fanin and fanout to 0.
-----------------------------------------------------------------------*/
void allocate()
{
   int i;

   Node = (NSTRUC *) malloc(Nnodes * sizeof(NSTRUC));
   Pinput = (NSTRUC **) malloc(Npi * sizeof(NSTRUC *));
   Poutput = (NSTRUC **) malloc(Npo * sizeof(NSTRUC *));
   for(i = 0; i<Nnodes; i++) {
      Node[i].indx = i;
      Node[i].fin = Node[i].fout = 0;
   }
}

/*-----------------------------------------------------------------------
input: gate type
output: string of the gate type
called by: pc
description:
  The routine receive an integer gate type and return the gate type in
  character string.
-----------------------------------------------------------------------*/
string gname(int tp)
{
   switch(tp) {
      case 0: return("PI");
      case 1: return("BRANCH");
      case 2: return("XOR");
      case 3: return("OR");
      case 4: return("NOR");
      case 5: return("NOT");
      case 6: return("NAND");
      case 7: return("AND");
   }
}
/*========================= End of program ============================*/
