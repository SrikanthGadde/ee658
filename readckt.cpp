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

EE658 Project Phase 2
Group 12

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
#include <regex>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
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
#include <chrono>

// macros for gate types
#define GATE_PI 0
#define GATE_BRANCH 1
#define GATE_XOR 2
#define GATE_OR 3
#define GATE_NOR 4
#define GATE_NOT 5
#define GATE_NAND 6
#define GATE_AND 7

// macros for logic
#define LOGIC_1 1
#define LOGIC_0 0
#define LOGIC_X -1
#define LOGIC_D 2
#define LOGIC_DBAR 3
#define LOGIC_UNSET 4

// macros for fault types
#define NOFAULT   -1
#define FAULT_SA0 0
#define FAULT_SA1 1

using namespace std;
//using clock = std::chrono::system_clock;
using sec = std::chrono::duration<double>; 
using ms = std::chrono::duration<double, std::milli>; 

#define MAXLINE 1000              /* Input buffer size */
#define MAXNAME 1000               /* File name size */

#define Upcase(x) ((isalpha(x) && islower(x))? toupper(x) : (x))
#define Lowcase(x) ((isalpha(x) && isupper(x))? tolower(x) : (x))

enum e_com {READ, PC, HELP, QUIT, LEV, LOGICSIM, RFL, PFS, RTG, DFS, PODEM, DALG, ATPG_DET, ATPG};
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
   int fault;                 /* logic value of the fault */
   int level_not_assign;
   int sa_parent[2];
   int assign_level;
} NSTRUC;                     

/*----------------- Command definitions ----------------------------------*/
#define NUMFUNCS 14
int cread(char *cp), pc(char *cp), help(char *cp), quit(char *cp), level(char *cp), logicsim(char *cp), rfl(char *cp), pfs(char *cp), rtg(char *cp), dfs(char *cp), podem(char *cp), dalg(char *cp), atpg_det(char *cp), atpg(char *cp);
void allocate(), clear();
string gname(int tp);
struct cmdstruc command[NUMFUNCS] = {
   {"ATPG_DET", atpg_det, EXEC},
   {"READ", cread, EXEC},
   {"PC", pc, CKTLD},
   {"HELP", help, EXEC},
   {"QUIT", quit, EXEC},
   {"LEV", level, CKTLD},
   {"LOGICSIM", logicsim, CKTLD},
   {"RFL", rfl, CKTLD},
   {"PFS", pfs, CKTLD},
   {"RTG", rtg, CKTLD},
   {"DFS", dfs, CKTLD},
   {"PODEM", podem, CKTLD},
   {"DALG", dalg, CKTLD},
   {"ATPG", atpg, EXEC},
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
vector<NSTRUC *> dFrontier;
NSTRUC* faultLocation;
int faultActivationVal;
int podem_count = 0;
string circuitName;
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

   // extract circuit name from path
   circuitName = buf;
   size_t sep = circuitName.find_last_of("\\/");
   if (sep != std::string::npos) {
      circuitName = circuitName.substr(sep + 1, circuitName.size() - sep - 1);
   }
   size_t dot = circuitName.find_last_of(".");
   if (dot != std::string::npos)
   {
      circuitName = circuitName.substr(0, dot);
   }

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
   set<pair<int, int>> det_fault_nc_list;
   set<pair<int, int>> temp_fault_list;
   set<pair<int, int>> temp_fault_list1;
   set<pair<int, int>> temp_fault_list2;
   set<pair<int, int>> temp_fault_list3;
   map<int,set<pair<int,int>>> all_fault;
   pair<int, int> f_val;
   pair<int, int> temp;
   set<pair<int,int> > final_faults;
   vector<int> control_input_index;

   char in_buf[MAXLINE], out_buf[MAXLINE];
   sscanf(cp, "%s %s", in_buf, out_buf);
   //logicsim(cp);

   lev();
   //logicsim
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
      return 1;
   }

   map<int, int> output_values;     // dictionary to hold PO values

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
               break;
            }
         }
      }

      output_values = eval_gates(output_values);      // function call to evaluate the circuit
      all_fault.clear();
      lev();

      for (i = 0; i < node_queue.size(); i++) {
         det_fault_list.clear();
         det_fault_nc_list.clear();
         temp_fault_list.clear();
         temp_fault_list1.clear();
         temp_fault_list2.clear();
         control_input_index.clear();
         np = &Node[node_queue[i]]; 
         if (np->type == 0) {      //PI
            f_val.first = np->num;
            f_val.second = !np->value;
            np->f_value = !np->value;
            det_fault_list.insert(f_val);
            all_fault[np->indx]=det_fault_list;
         } 
         else if (np->type == 1) {     // branch
            f_val.first = np->num;
            f_val.second = !np->value;
            np->f_value = !np->value;
            det_fault_list = all_fault[np->unodes[0]->indx];
            det_fault_list.insert(f_val);
            all_fault[np->indx]=det_fault_list;
         } 
         else if (np->type == 2) {     // xor
            temp_fault_list1.clear();
            temp_fault_list2.clear();
            for (j = 0; j < np->fin; j++) {
               // f_val.first = np->unodes[j]->num;
               //f_val.second = np->unodes[j]->f_value;
               det_fault_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
               if (j == 0) {
                  temp_fault_list3 = all_fault[np->unodes[j]->indx];
               } else {
                  set_intersection(temp_fault_list3.begin(), temp_fault_list3.end(), all_fault[np->unodes[1]->indx].begin(), all_fault[np->unodes[1]->indx].end(), std::inserter(temp_fault_list1, temp_fault_list1.begin()));
                  temp_fault_list3 = temp_fault_list1;
               }
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
            // todo
            set_difference(det_fault_list.begin(), det_fault_list.end(), temp_fault_list1.begin(), temp_fault_list1.end(), std::inserter(temp_fault_list2, temp_fault_list2.begin()));     
            all_fault[np->indx]=temp_fault_list2;
         } 
         else if (np->type == 3) {     // or
            if (np->value == 0) {
               for (j = 0; j < np->fin; j++) {
                  det_fault_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
               }
               f_val.second = 1;
               np->f_value = 1;
               f_val.first = np->num;
               det_fault_list.insert(f_val);
               all_fault[np->indx]=det_fault_list;
            } 
            else {
               int count = 0;
               //all_fault.erase(np->indx);
               for (j = 0; j < np->fin; j++) {
                  if (np->unodes[j]->value == 1) {
                     if (count == 0) {
                        index = np->unodes[j]->indx;
                     } else {
                        control_input_index.push_back(np->unodes[j]->indx);
                     }
                     count++;
                  } else {
                     det_fault_nc_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
                  }
               }

               det_fault_list.insert(all_fault[index].begin(),all_fault[index].end());	
               for (j = 0; j < control_input_index.size(); j++) {
                  temp_fault_list1.clear();
                  set_intersection(det_fault_list.begin(), det_fault_list.end(), all_fault[control_input_index[j]].begin(), all_fault[control_input_index[j]].end(), std::inserter(temp_fault_list1, temp_fault_list1.begin()));
                  det_fault_list = temp_fault_list1;
               }
               set_difference(det_fault_list.begin(), det_fault_list.end(), det_fault_nc_list.begin(), det_fault_nc_list.end(), std::inserter(temp_fault_list, temp_fault_list.begin()));
               f_val.first = np->num;
               f_val.second = 0;
               np->f_value = 0;
               det_fault_list = temp_fault_list;
               det_fault_list.insert(f_val);
               all_fault[np->indx]=det_fault_list;
            }
         } 
         else if (np->type == 4) {     // nor
            if (np->value == 1) {
               for (j = 0; j < np->fin; j++) {
                  det_fault_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
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
                     if (count == 0) {
                        index = np->unodes[j]->indx;
                     } else {
                        control_input_index.push_back(np->unodes[j]->indx);
                     }
                     count++;
                  } else {
                     det_fault_nc_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
                  }
               }

               det_fault_list.insert(all_fault[index].begin(),all_fault[index].end());	
               for (j = 0; j < control_input_index.size(); j++) {
                  temp_fault_list1.clear();
                  set_intersection(det_fault_list.begin(), det_fault_list.end(), all_fault[control_input_index[j]].begin(), all_fault[control_input_index[j]].end(), std::inserter(temp_fault_list1, temp_fault_list1.begin()));
                  det_fault_list = temp_fault_list1;
               }    
               set_difference(det_fault_list.begin(), det_fault_list.end(), det_fault_nc_list.begin(), det_fault_nc_list.end(), std::inserter(temp_fault_list, temp_fault_list.begin()));
               f_val.first = np->num;
               f_val.second = 1;
               np->f_value = 1;
               det_fault_list = temp_fault_list;
               det_fault_list.insert(f_val);
               all_fault[np->indx]=det_fault_list;
            }
         } 
         else if (np->type == 5) {     // not    
            det_fault_list = all_fault[np->unodes[0]->indx];
            f_val.first = np->num;
            f_val.second = !np->value;
            np->f_value = !np->value;
            det_fault_list.insert(f_val);
            all_fault[np->indx]=det_fault_list;
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
                     if (count == 0) {
                        index = np->unodes[j]->indx;            
                     } else {
                        control_input_index.push_back(np->unodes[j]->indx);
                     }
                     count++;
                  } else {
                     det_fault_nc_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
                  }
               }

               det_fault_list.insert(all_fault[index].begin(),all_fault[index].end());	
               for (j = 0; j < control_input_index.size(); j++) {
                  temp_fault_list1.clear();
                  set_intersection(det_fault_list.begin(), det_fault_list.end(), all_fault[control_input_index[j]].begin(), all_fault[control_input_index[j]].end(), std::inserter(temp_fault_list1, temp_fault_list1.begin()));
                  det_fault_list = temp_fault_list1;
               }
               set_difference(det_fault_list.begin(), det_fault_list.end(), det_fault_nc_list.begin(), det_fault_nc_list.end(), std::inserter(temp_fault_list, temp_fault_list.begin()));

               f_val.first = np->num;
               f_val.second = 0;
               np->f_value = 0;
               det_fault_list = temp_fault_list;
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
                     if (count == 0) {
                        index = np->unodes[j]->indx;
                     } else {
                        control_input_index.push_back(np->unodes[j]->indx);
                     }
                     count++;
                  } else {
                     det_fault_nc_list.insert(all_fault[np->unodes[j]->indx].begin(),all_fault[np->unodes[j]->indx].end());
                  }
               }

               det_fault_list.insert(all_fault[index].begin(),all_fault[index].end());	
               for (j = 0; j < control_input_index.size(); j++) {
                  temp_fault_list1.clear();
                  set_intersection(det_fault_list.begin(), det_fault_list.end(), all_fault[control_input_index[j]].begin(), all_fault[control_input_index[j]].end(), std::inserter(temp_fault_list1, temp_fault_list1.begin()));
                  det_fault_list = temp_fault_list1;
               }
               set_difference(det_fault_list.begin(), det_fault_list.end(), det_fault_nc_list.begin(), det_fault_nc_list.end(), std::inserter(temp_fault_list, temp_fault_list.begin()));

               f_val.first = np->num;
               f_val.second = 1;
               np->f_value = 1;
               det_fault_list = temp_fault_list;
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


   for (i = 0; i < Nnodes; i++) {
   if (Node[i].fout == 0) {
   for (auto const &element : all_fault[i]) {
   final_faults.insert(element);
   }
   }
   }
   }
   ofstream output_file;
   output_file.open(out_buf);
   //cout<<"here"<<endl;
   if (output_file) {
   for (auto const &element : final_faults) {
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
      return 1;
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
      return 1;
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
      return 1;
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
int rtg(char *cp) {
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


// levelization
int level(char *cp) {
   char out_buf[MAXLINE];
   sscanf(cp, "%s", out_buf);

   lev();
         ofstream output_test_pattern_file;
         output_test_pattern_file.open(out_buf);
         if ( output_test_pattern_file ) {
            output_test_pattern_file << circuitName << endl;
            int count_PI = 0;
            int count_PO = 0;
            int count_gates = 0;
            for (int i = 0; i < Nnodes; i++) {
               if (Node[i].fin == 0) {
                  count_PI++;
               }
               if (Node[i].fout == 0) {
                  count_PO++;
               }
               if (Node[i].type > 1) {
                  count_gates++;
               }
            }
            output_test_pattern_file << "#PI: " << count_PI << endl;
            output_test_pattern_file << "#PO: " << count_PO << endl;
            output_test_pattern_file << "Nodes: " << Nnodes << endl;
            output_test_pattern_file << "#Gates: " << count_gates << endl; 
            for (int i = 0; i < Nnodes; i++) {
               output_test_pattern_file << Node[i].num << " " << Node[i].level << endl ;
            }
         }
         output_test_pattern_file.close();

}


// --------------------------------------------------------Phase 3--------------------------------------------------
//----------------------------
// Functions for logic simulation - PODEM imply
void simFullCircuit();
void simGateRecursive(NSTRUC* g);
int simGate(NSTRUC* g);
int evalGate(vector<int> in, int c, int i);
int EvalXORGate(vector<int> in, int inv);
int LogicNot(int logicVal);
void setValueCheckFault(NSTRUC* g, int gateValue);
//-----------------------------

//----------------------------
// Functions for PODEM:
bool podemRecursion();
bool getObjective(NSTRUC* &g, int &v);
void updateDFrontier();
void backtrace(NSTRUC* &pi, int &piVal, NSTRUC* objGate, int objVal);

//--------------------------
// MAIN PODEM
clock_t podem_recursion_tStart;

int podem (char *cp) {
   podem_recursion_tStart = clock();
   char faultNode_buf[MAXLINE], faultValue_buf[MAXLINE];
   sscanf(cp, "%s %s", faultNode_buf, faultValue_buf);

   int faultNode = stoi(faultNode_buf);
   int faultValue = stoi(faultValue_buf);
   
   // set all gate values to X
   for (int i=0; i < Nnodes; i++) {
      Node[i].fault = NOFAULT;
      Node[i].value = LOGIC_X;
      if (Node[i].num == faultNode) {
         faultLocation = &Node[i];
         Node[i].fault = faultValue;
         faultActivationVal = (faultValue == FAULT_SA0) ? LOGIC_1 : LOGIC_0;
      }
   }

   // initialize the D frontier.
   dFrontier.clear();

   // call PODEM recursion function
   bool res = podemRecursion();

   // If success, print the test to the output file.
   int initial = 0; 
   if (res == true) {
   //   try {
   //       //std::regex rgx("(a-zA-Z0-9_+)\\.ckt");
   //       regex rgx(R"(([\w]+)\.ckt)");
   //    } catch (std::regex_error& e) {
   //       cout << e.code() << endl;
   //       if (e.code() == std::regex_constants::error_brack	)
   //          std::cerr << "The expression contained mismatched brackets ([ and ]).\n";
   //       else std::cerr << "Some other regex exception happened.\n";
   //    }
   //    regex rgx(R"(([\w]+)\.ckt)");
   //    smatch match;
   //    circuitName = "/home/viterbi/02/sgadde/ee658/ee658/circuits/c17.ckt";
   //    cout << circuitName << endl;
   //    if (regex_search(circuitName, match, rgx)){ 
   //       cout << "FOUND" << endl; 
   //       for ( auto i : match ){
   //          cout << i << "," ;}
   //       cout << match[0] << endl;
         string output_file = circuitName + "_PODEM_" + faultNode_buf + "@" + faultValue_buf + ".txt";
         ofstream output_test_pattern_file;
         output_test_pattern_file.open(output_file);
         if ( output_test_pattern_file ) {
            for (int i = 0; i < Nnodes; i++) {
               if (Node[i].fin == 0) {
                  if (initial == 1) {
                              output_test_pattern_file << ",";
                  }
                           output_test_pattern_file << Node[i].num;
                  initial = 1;
               }
            }
            output_test_pattern_file << endl;
            initial = 0;
            for (int i = 0; i < Nnodes; i++) {
               if (Node[i].fin == 0) {
                  if (initial == 1) {
                           output_test_pattern_file << ",";
                  }
                  if (Node[i].value == LOGIC_X) {
                           output_test_pattern_file << "X";
                  } else if (Node[i].value == LOGIC_D) {
                           output_test_pattern_file << "1";
                  } else if (Node[i].value == LOGIC_DBAR) {
                           output_test_pattern_file << "0";
                  } else {
                           output_test_pattern_file << Node[i].value;
                  }
                  initial = 1;
               }
            }
            output_test_pattern_file << endl;
         }
         output_test_pattern_file.close();
   }

   // If failure to find test, print a message to the output file
   else {
      //cout << "none found" << endl;
      return 1;
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////
// Start of functions for circuit simulation (PODEM Imply)
/** @brief Runs full circuit simulation
 *
 * Full-circuit simulation: set all non-PI gates to LOGIC_UNSET
 * and call the recursive simulate function on all PO gates.
 */
void simFullCircuit() {
  for (int i=0; i<Nnodes; i++) {
    NSTRUC* g = &Node[i];
    if (g->type != GATE_PI)
      g->value = LOGIC_UNSET;      
  }  
  for (int i=0; i<Nnodes; i++) {
   NSTRUC* g = &Node[i];
   if (g->fout == 0) {
      simGateRecursive(g);
   }
  }
}

// Recursive function to find and set the value on Gate* g.
// This function calls simGate and setValueCheckFault. 
/** @brief Recursive function to find and set the value on Gate* g.
 * \param g The gate to simulate.
 * This function prepares Gate* g to to be simulated by recursing
 * on its inputs (if needed).
 * 
 * Then it calls \a simGate(g) to calculate the new value.
 * 
 * Lastly, it will set the Gate's output value based on
 * the calculated value.
 * 
 */
void simGateRecursive(NSTRUC* g) {

  // If this gate has an already-set value, then we're done.
  if (g->value != LOGIC_UNSET)
    return;
  
  // Recursively call this function on this gate's predecessors to
  // ensure that their values are known.
  for (int i=0; i<g->fin; i++) {
    simGateRecursive(g->unodes[i]);
  }
  
  int gateValue = simGate(g);

  // After I have calculated this gate's value, check to see if a fault changes it and set.
  setValueCheckFault(g, gateValue);
}

/** @brief Simulate the value of the given Gate.
 *
 * This is a gate simulation function -- it will simulate the gate g
 * with its current input values and return the output value.
 * This function does not deal with the fault. (That comes later.)
 *
 */
int simGate(NSTRUC* g) {
  // Create a vector of the values of this gate's inputs.
  vector<int> inputVals;
  for (int i=0; i<g->fin; i++) {
    inputVals.push_back(g->unodes[i]->value);      
  }

  int gateType = g->type;
  int gateValue;
  // Now, set the value of this gate based on its logical function and its input values
  switch(gateType) {   
  case GATE_NAND: { gateValue = evalGate(inputVals, 0, 1); break; }
  case GATE_NOR: { gateValue = evalGate(inputVals, 1, 1); break; }
  case GATE_AND: { gateValue = evalGate(inputVals, 0, 0); break; }
  case GATE_OR: { gateValue = evalGate(inputVals, 1, 0); break; }
  case GATE_NOT: { gateValue = LogicNot(inputVals[0]); break; }
  case GATE_XOR: { gateValue = EvalXORGate(inputVals, 0); break; }
  case GATE_BRANCH: {gateValue = inputVals[0]; break; }
  default: break;//{ cout << "ERROR: Do not know how to evaluate gate type " << gateType << endl; assert(false);}
  }    

  return gateValue;
}

/** @brief Evaluate a NAND, NOR, AND, or OR gate.
 * \param in The logic value's of this gate's inputs.
 * \param c The controlling value of this gate type (e.g. c==0 for an AND or NAND gate)
 * \param i The inverting value for this gate (e.g. i==0 for AND and i==1 for NAND)
 * \returns The logical value produced by this gate (not including a possible fault on this gate).
 */
int evalGate(vector<int> in, int c, int i) {

  // Are any of the inputs of this gate the controlling value?
  bool anyC = find(in.begin(), in.end(), c) != in.end();
  
  // Are any of the inputs of this gate unknown?
  bool anyUnknown = (find(in.begin(), in.end(), LOGIC_X) != in.end());

  int anyD    = find(in.begin(), in.end(), LOGIC_D)    != in.end();
  int anyDBar = find(in.begin(), in.end(), LOGIC_DBAR) != in.end();


  // if any input is c or we have both D and D', then return c^i
  if ((anyC) || (anyD && anyDBar))
    return (i) ? LogicNot(c) : c;
  
  // else if any input is unknown, return unknown
  else if (anyUnknown)
    return LOGIC_X;

  // else if any input is D, return D^i
  else if (anyD)
    return (i) ? LOGIC_DBAR : LOGIC_D;

  // else if any input is D', return D'^i
  else if (anyDBar)
    return (i) ? LOGIC_D : LOGIC_DBAR;

  // else return ~(c^i)
  else
    return LogicNot((i) ? LogicNot(c) : c);
}

/** @brief Evaluate an XOR or XNOR gate.
 * \param in The logic value's of this gate's inputs.
 * \param inv The inverting value for this gate (e.g. i==0 for XOR and i==1 for XNOR)
 * \returns The logical value produced by this gate (not including a possible fault on this gate).
 */
int EvalXORGate(vector<int> in, int inv) {

  // if any unknowns, return unknown
  bool anyUnknown = (find(in.begin(), in.end(), LOGIC_X) != in.end());
  if (anyUnknown)
    return LOGIC_X;

  // Otherwise, let's count the numbers of ones and zeros for faulty and fault-free circuits.
  int onesFaultFree = 0;
  int onesFaulty = 0;

  for (int i=0; i<in.size(); i++) {
    switch(in[i]) {
    case LOGIC_0: {break;}
    case LOGIC_1: {onesFaultFree++; onesFaulty++; break;}
    case LOGIC_D: {onesFaultFree++; break;}
    case LOGIC_DBAR: {onesFaulty++; break;}
    default: {cout << "ERROR: Do not know how to process logic value " << in[i] << " in Gate::EvalXORGate()" << endl; return LOGIC_X;}
    }
  }
  
  int XORVal;

   if ((onesFaultFree%2 == 0) && (onesFaulty%2 ==0)) { 
      XORVal = LOGIC_0; }
   else if ((onesFaultFree%2 == 1) && (onesFaulty%2 ==1))
    { XORVal = LOGIC_1; }
  else if ((onesFaultFree%2 == 1) && (onesFaulty%2 ==0))
   { XORVal = LOGIC_D; }
  else
   { XORVal = LOGIC_DBAR; }

  return (inv) ? LogicNot(XORVal) : XORVal;

}


/** @brief Perform a logical NOT operation on a logical value using the LOGIC_* macros
 */
int LogicNot(int logicVal) {
  if (logicVal == LOGIC_1)
    return LOGIC_0;
  if (logicVal == LOGIC_0)
    return LOGIC_1;
  if (logicVal == LOGIC_D)
    return LOGIC_DBAR;
  if (logicVal == LOGIC_DBAR)
    return LOGIC_D;
  if (logicVal == LOGIC_X)
    return LOGIC_X;
      
  cout << "ERROR: Do not know how to invert " << logicVal << " in LogicNot(int logicVal)" << endl;
  return LOGIC_UNSET;
}


/** @brief Set the value of Gate* g to value gateValue, accounting for any fault on g.
 */
void setValueCheckFault(NSTRUC* g, int gateValue) {
  if ((g->fault == FAULT_SA0) && (gateValue == LOGIC_1)) 
  	g->value = LOGIC_D;
  else if ((g->fault == FAULT_SA0) && (gateValue == LOGIC_DBAR)) 
  	g->value = LOGIC_0;
  else if ((g->fault == FAULT_SA1) && (gateValue == LOGIC_0)) 
  	g->value = LOGIC_DBAR;
  else if ((g->fault == FAULT_SA1) && (gateValue == LOGIC_D)) 
  	g->value = LOGIC_1;
  else
  	g->value = gateValue;
}


// End of functions for circuit simulation
////////////////////////////////////////////////////////////
/** @brief A simple method to compute the set of gates on the D frontier.
 */
void updateDFrontier() {
  //  - clear the dFrontier vector (stored as the global variable dFrontier -- see the top of the file)
	dFrontier.clear();
	
  //  - loop over all gates in the circuit; for each gate, check if it should be on D-frontier; if it is, add it to the dFrontier vector.
	NSTRUC* np;

	for (int i=0; i< Nnodes; i++)	{
		np = &Node[i];
		if (np->value != LOGIC_X) {continue; }
		else {
			for (int j=0; j<np->fin; j++) { 
            if (np->unodes[j]->value == LOGIC_D || np->unodes[j]->value == LOGIC_DBAR) {
               dFrontier.push_back(np); 
               break;
            }
         }		
		}
	}
}


// Find the objective for myCircuit. The objective is stored in g, v.
/** @brief PODEM objective function.
 *  \param g Use this pointer to store the objective Gate your function picks.
 *  \param v Use this char to store the objective value your function picks.
 *  \returns True if the function is able to determine an objective, and false if it fails.
 */
bool getObjective(NSTRUC* &g, int &v) {

  // First you will need to check if the fault is activated yet.
  // Note that in the setup above we set up a global variable
  // Gate* faultLocation which represents the gate with the stuck-at
  // fault on its output. Use that when you check if the fault is
  // excited.

  
  // Another note: if the fault is not excited but the fault 
  // location value is not X, then we have failed to activate 
  // the fault. In this case getObjective should fail and Return false.  
	
	if (faultLocation->value == LOGIC_X) {g= faultLocation; v=faultActivationVal; 
		return true;}
	
	if (faultLocation->value == LOGIC_1 || faultLocation->value == LOGIC_0)
	{return false;} 
	//setValueCheckFault(faultLocation, faultLocation->getValue());

  // If the fault is already activated, then you will need to 
  // use the D-frontier to find an objective.
  // Before you use the D-frontier you should update it by running:
  updateDFrontier();

  // If the D frontier is empty after update, then getObjective fails
  // and should return false.
	
	if (dFrontier.empty()) {return false; }
	
  // getObjective needs to choose a gate from the D-Frontier.
	NSTRUC* d;	
	d = dFrontier[0];
	// Later, a possible optimization is to use the SCOAP observability metric or other smart methods to choose this carefully.
    
  // Lastly, set the values of g and v based on the gate you chose from the D-Frontier.
   for (int i=0; i<d->fin; i++) 
      {
         if (d->unodes[i]->value == LOGIC_X)
         {
            g = d->unodes[i]; break;
         }
      }
			
	if (d->type == GATE_AND || d->type == GATE_NAND) {v=LOGIC_1; }
	else if (d->type == GATE_OR || d->type == GATE_NOR) {v=LOGIC_0; }
	else if (d->type == GATE_XOR) {v=LOGIC_0; }
	else {v=LOGIC_X; }
	
  return true;

}


// Backtrace: given objective objGate and objVal, then figure out which input (pi) to set and which value (piVal) to set it to.
/** @brief PODEM backtrace function
 * \param pi Output: A Gate pointer to the primary input your backtrace function found.
 * \param piVal Output: The value you want to set that primary input to
 * \param objGate Input: The objective Gate (computed by getObjective)
 * \param objVal Input: the objective value (computed by getObjective)
 */
void backtrace(NSTRUC* &pi, int &piVal, NSTRUC* objGate, int objVal) {

	pi = objGate;
   int k1;
	int num_inversions;
	int gatetype = pi->type;
	
	if (gatetype == GATE_NOR || gatetype == GATE_NOT || gatetype == GATE_NAND)
		num_inversions=1;
	else num_inversions=0;
	
	while (pi->type != GATE_PI)
	{ 		
		for (k1=0; k1<pi->fin; k1++) 
			{ 
				if (pi->unodes[k1]->value == LOGIC_X) {
               pi=pi->unodes[k1]; break;} 
			}
			
			gatetype = pi->type;
			
		if (gatetype == GATE_NOR || gatetype == GATE_NOT || gatetype == GATE_NAND)
		{num_inversions++; }
	}
	
	if (num_inversions%2==1) {piVal= LogicNot(objVal); }
	else {piVal = objVal; }
	
}


/** @brief PODEM recursion.
 */
bool podemRecursion() {

   int time_elapsed = (clock() - podem_recursion_tStart)/(CLOCKS_PER_SEC);
   if (time_elapsed > 1) {
      return false;
   }

  // If D or D' is at an output, then return true
    int i,j,val;
	
   for (i = 0; i < Nnodes; i++) {
      if (Node[i].fout == 0) {
         val = Node[i].value;
         if (val == LOGIC_D || val == LOGIC_DBAR) {
            return true;
         }
      }
   }

   NSTRUC* g;
   int v;  

  // Call the getObjective function. Store the result in g and v.    
  // If getObjective fails, return false	
  
	bool obj = getObjective(g, v);
	if (obj == false ) return false;
	
  NSTRUC* pi;
  int piVal;
  
  // Call the backtrace function. Store the result in pi and piVal.
  backtrace(pi, piVal, g, v);
  
  // Set the value of pi to piVal. Use setValueCheckFault function to make sure if there is a fault on the PI gate, it correctly gets set.
  setValueCheckFault(pi, piVal);
  
  // Now, determine the implications of the input set by simulating the circuit by calling simFullCircuit(myCircuit);
  simFullCircuit();
   
  if (podemRecursion()) {return true;}
  // If the recursive call fails, set the opposite PI value, simulate it and recurse.
  // If this recursive call succeeds, return true.
  int notpiVal;
  notpiVal= LogicNot(piVal);
  
  setValueCheckFault(pi, notpiVal); 
  
  simFullCircuit();
  if (podemRecursion()) {return true;}
  
  // If we get to here, neither pi=v nor pi = v' worked. So, set pi to value X and 
  // return false.
  setValueCheckFault(pi, LOGIC_X);

  return false;
}


// ------------------------------------- DALG ------------------------------------------------------------------
deque<int> Dfront;
vector<int> Jfront;
vector<int> imply;
vector< vector<int> > test_vectors;
int Dalg_count;
pair<int,int> check_fault;
clock_t dalg_recursion_tStart;

bool unassign(int level) {  
   for(int kk=0;kk<Nnodes;kk++) { 
      if(Node[kk].assign_level>level ) {
         Node[kk].value=LOGIC_X; 
         Node[kk].assign_level=-1;
      } 
   } 
   return true;
}

bool check_Dfront (int node){
   NSTRUC *np; 
   np =&Node[node];
   int Dnum=0; 
   int Dbarnum=0, Onenum=0, Zeronum=0;
   for(int j=0;j<(np->fin); j++ ){
      if(np->unodes[j]->value == LOGIC_D) Dnum++;
      else if(np->unodes[j]->value == LOGIC_DBAR) Dbarnum++;
      else if(np->unodes[j]->value == LOGIC_1) Onenum++;
      else if(np->unodes[j]->value == LOGIC_0) Zeronum++;
   }
   if( (Dnum==0 && Dbarnum==0)|| np->value!=LOGIC_X ||(Dnum>0 && Dbarnum>0) || (Onenum>0 && (np->type==GATE_OR || np->type==GATE_NOR || np->type==GATE_NOT || np->type==GATE_BRANCH))
      || (Zeronum>0 && (np->type==GATE_NAND || np->type==GATE_AND || np->type==GATE_NOT || np->type==GATE_BRANCH )))
   { return false;}
   else { return true; }
}
   
int getDfront(){

   vector<int> tmp; 
   tmp.clear();
   Dfront.clear();
   tmp.push_back((int)(check_fault.first));

      for(int i=0; i<tmp.size(); i++){
         NSTRUC *np =&Node[tmp[i]];
         if(np->fout>0){
            for( int k=0;k<np->fout; k++){
               if  ( check_Dfront(np->dnodes[k]->indx) ){ 
                     Dfront.push_back(np->dnodes[k]->indx);
               }
               else if ( np->dnodes[k]->value==LOGIC_D || np->dnodes[k]->value==LOGIC_DBAR ){
                  tmp.push_back(np->dnodes[k]->indx);
               }
            }
         }
      }
      return 0;
}



bool imply_and_check(){
	
	for(int kk=0; kk< imply.size(); kk++){
	   NSTRUC *np =&Node[imply[kk]];
	   if(np->value != LOGIC_D && np->value != LOGIC_DBAR) {
	   //backward imply
		   if(np->type == GATE_PI) return true;

         else if( np->type == GATE_BRANCH) {		
            for(int jj=0;jj<(np->fin);jj++){
               if(np->value==LOGIC_0) { 
                  if(np->unodes[jj]->value==LOGIC_X) { 
                     np->unodes[jj]->value = LOGIC_0;   
                     imply.push_back(np->unodes[jj]->indx);
                  } else if (np->unodes[jj]->value!=LOGIC_0 ) {
                     return false;
                  }
               }
               if(np->value==LOGIC_1) { 
                  if(np->unodes[jj]->value==LOGIC_X) { 
                     np->unodes[jj]->value = LOGIC_1;   
                     imply.push_back(np->unodes[jj]->indx);
                  } else if (np->unodes[jj]->value!=LOGIC_1 ) {
                     return false;
                  }
               }
            } 
         
         }// branch
         // forward propagation

         else if (np->type == GATE_XOR) { 
            int num_inputX=0; 
            int oneXind=0; 
            int implyvalue=np->value; 
            for(int jj=0;jj<(np->fin);jj++) { 
               if(np->unodes[jj]->value==LOGIC_1 || np->unodes[jj]->value==LOGIC_0) {
                  implyvalue^=np->unodes[jj]->value;
               } else if (np->unodes[jj]->value==LOGIC_D || np->unodes[jj]->value==LOGIC_DBAR) {
                  return false;
               } else {
                  oneXind=jj;
                  num_inputX++; 
               }
            }
            if(num_inputX==1) { 
               np->unodes[oneXind]->value =  implyvalue;   
               imply.push_back(np->unodes[oneXind]->indx);   
            } else {
               Jfront.push_back(np->indx);
            } 
         } //xor
         // j frontier : 1. at least two inputs are unknown  2. no control value of input(so that output can be implied)
				
         else if  ( np->type == GATE_OR) {
            if(np->value==LOGIC_0)  { 
               for(int jj=0;jj<(np->fin);jj++){
                  if(np->unodes[jj]->value!=LOGIC_0 && np->unodes[jj]->value!=LOGIC_X) {
                     return false;
                  }
                  else if(np->unodes[jj]->value==LOGIC_X){ 
                     np->unodes[jj]->value = 0;   
                     imply.push_back(np->unodes[jj]->indx);   
                  }
               }   
            }
            if(np->value==LOGIC_1){ 
               bool has_controlvalue=false; 
               int unknown_num=0;
               int oneXind=0; 
               int Dnum=0; 
               int Dbarnum=0;
               for(int jj=0;jj<(np->fin);jj++) {
                     if(np->unodes[jj]->value==LOGIC_1) { 
                        has_controlvalue=true; 
                        break;
                     } else if (np->unodes[jj]->value==LOGIC_X) {
                        unknown_num++;  
                        oneXind=jj;
                     } else if (np->unodes[jj]->value==LOGIC_D) { 
                        Dnum++; 
                     } else if (np->unodes[jj]->value==LOGIC_DBAR) { 
                        Dbarnum++; 
                     }
               }
               if(has_controlvalue==false && unknown_num==0) {
                  if(Dnum==0 || Dbarnum==0) { 
                     return false;
                  }
               }
               else if(has_controlvalue==false && unknown_num ==1) {     
                  if(!(Dnum>0 && Dbarnum>0)) {
                     np->unodes[oneXind]->value =  LOGIC_1;   
                     imply.push_back(np->unodes[oneXind]->indx); 
                  }
               }              
               else if(has_controlvalue==false && unknown_num>1 && !((Dnum>0 && Dbarnum>0))) { 
                  Jfront.push_back(np->indx);
               }
            }
         }// or
				
         else if  ( np->type == GATE_NOR) {
            if(np->value==LOGIC_1)  { 
               for(int jj=0;jj<(np->fin);jj++){
                  if(np->unodes[jj]->value!=LOGIC_0 && np->unodes[jj]->value!=LOGIC_X) {
                     return false;
                  } else if (np->unodes[jj]->value==LOGIC_X) {
                     np->unodes[jj]->value = LOGIC_0;   
                     imply.push_back(np->unodes[jj]->indx);
                  }
               }   
            }
            if(np->value==LOGIC_0) {
               bool has_controlvalue=false; 
               int unknown_num=0;
               int oneXind=0; 
               int Dnum=0; 
               int Dbarnum=0;
               for(int jj=0;jj<(np->fin);jj++) {
                  if(np->unodes[jj]->value==LOGIC_1) { 
                     has_controlvalue=true; 
                     break;
                  } else if (np->unodes[jj]->value==LOGIC_X) {
                     unknown_num++;  
                     oneXind=jj;
                  } else if (np->unodes[jj]->value==LOGIC_D) {
                     Dnum++;
                  } else if (np->unodes[jj]->value==LOGIC_DBAR) {
                     Dbarnum++; 
                  }
               }
               if(has_controlvalue==false && unknown_num==0) {
                  if(Dnum==0 || Dbarnum==0) {
                     return false;
                  }
               } else if (has_controlvalue==false && unknown_num ==1) {     
                  if(!(Dnum>0 && Dbarnum>0)) {
                     np->unodes[oneXind]->value = LOGIC_1;   
                     imply.push_back(np->unodes[oneXind]->indx); 
                  }
               } else if (has_controlvalue==false && unknown_num>1 && !((Dnum>0 && Dbarnum>0))) {
                  Jfront.push_back(np->indx);
               }
            }
         } // nor

         else if  ( np->type == GATE_NOT) {
            if(np->value==LOGIC_0) {
               if(np->unodes[0]->value==LOGIC_X) { 
                  np->unodes[0]->value = LOGIC_1;   
                  imply.push_back(np->unodes[0]->indx);
               } else if(np->unodes[0]->value!=LOGIC_1) {
                  return false;
               }
            }
            if (np->value==LOGIC_1) {
               if (np->unodes[0]->value==LOGIC_X) {
                  np->unodes[0]->value = LOGIC_0;
                  imply.push_back(np->unodes[0]->indx);
               } else if (np->unodes[0]->value!=LOGIC_0) {
                  return false;
               }
            }
         } //not

         else if  ( np->type == GATE_NAND) {
            if(np->value==LOGIC_0)  { 
               for(int jj=0;jj<(np->fin);jj++){
                  if(np->unodes[jj]->value!=LOGIC_1 && np->unodes[jj]->value!=LOGIC_X) {
                     return false;
                  } else if (np->unodes[jj]->value==LOGIC_X) {
                     np->unodes[jj]->value = LOGIC_1;
                     imply.push_back(np->unodes[jj]->indx);
                  }
               }   
            }
            if (np->value==LOGIC_1) {
               bool has_controlvalue=false; 
               int unknown_num=0;
               int oneXind=0; 
               int Dnum=0; 
               int Dbarnum=0;
               for(int jj=0;jj<(np->fin);jj++) {
                  if(np->unodes[jj]->value==LOGIC_0) { 
                     has_controlvalue=true; 
                     break;
                  } else if (np->unodes[jj]->value==LOGIC_X) {
                     unknown_num++;  
                     oneXind=jj;
                  } else if (np->unodes[jj]->value==LOGIC_D) {
                     Dnum++; 
                  } else if (np->unodes[jj]->value==LOGIC_DBAR) {
                     Dbarnum++; 
                  }
               }
               if (has_controlvalue==false && unknown_num==0) {
                  if(Dnum==0 || Dbarnum==0) {
                     return false;
                  }
               } else if (has_controlvalue==false && unknown_num ==1) {     
                  if(!(Dnum>0 && Dbarnum>0)) {
                     np->unodes[oneXind]->value =  LOGIC_0;   
                     imply.push_back(np->unodes[oneXind]->indx); 
                  }
               } else if (has_controlvalue==false && unknown_num>1 && !((Dnum>0 && Dbarnum>0))) {
                  Jfront.push_back(np->indx);
               }
            }
         }// Nand
         
         else if  ( np->type == GATE_AND) {
            if(np->value==LOGIC_1)  { 
               for(int jj=0;jj<(np->fin);jj++){
                  if(np->unodes[jj]->value!=LOGIC_1 && np->unodes[jj]->value!=LOGIC_X) {
                     return false;
                  } else if (np->unodes[jj]->value==LOGIC_X) {
                     np->unodes[jj]->value = LOGIC_1;   
                     imply.push_back(np->unodes[jj]->indx);
                  }
               }   
            }
            if(np->value==LOGIC_0) { 
               bool has_controlvalue=false; 
               int unknown_num=0;
               int oneXind=0; 
               int Dnum=0; 
               int Dbarnum=0;
               for(int jj=0;jj<(np->fin);jj++) {
                  if(np->unodes[jj]->value==LOGIC_0) { 
                     has_controlvalue=true; 
                     break;
                  } else if (np->unodes[jj]->value==LOGIC_X) {
                     unknown_num++;  
                     oneXind=jj;
                  } else if (np->unodes[jj]->value==LOGIC_D) {
                     Dnum++; 
                  } else if (np->unodes[jj]->value==LOGIC_DBAR) {
                     Dbarnum++; 
                  }
               }
               if(has_controlvalue==false && unknown_num==0) {
                  if(Dnum==0 || Dbarnum==0) { 
                     return false;
                  }
               } else if(has_controlvalue==false && unknown_num ==1) {     
                  if(!(Dnum>0 && Dbarnum>0)) {
                  //else
                  np->unodes[oneXind]->value = LOGIC_0;   
                  imply.push_back(np->unodes[oneXind]->indx); }
               } else if(has_controlvalue==false && unknown_num>1 && !((Dnum>0 && Dbarnum>0))) { 
                  Jfront.push_back(np->indx);
               }
            }
         } // and
		}


	   //forward imply								 
      for(int jj=0;jj<(np->fout);jj++){
         NSTRUC *np_out;		
         np_out = &Node[np->dnodes[jj]->indx];	
         if(np->dnodes[jj]->num == (check_fault.first) ) continue;
         int Xnum=0;  
         int Dnum=0;
         int Dbarnum=0;
         int Onenum=0;
         int Zeronum=0;
         int expect_outvalue= 0;
         for(int jj=0;jj<(np_out->fin);jj++) {
               if      (np_out->unodes[jj]->value==LOGIC_X) Xnum++;
               else if (np_out->unodes[jj]->value==LOGIC_D) Dnum++;
               else if (np_out->unodes[jj]->value==LOGIC_DBAR) Dbarnum++;
               else if (np_out->unodes[jj]->value==LOGIC_1) Onenum++; 
               else if (np_out->unodes[jj]->value==LOGIC_0) Zeronum++;
         }

         //branch
         if(np_out->type==GATE_BRANCH){
            if(np->value==LOGIC_0) { 
               if(np_out->value==LOGIC_X) {
                  np_out->value = LOGIC_0;
                  imply.push_back(np_out->indx);
               } else if (np_out->num!=(int)(check_fault.first) && !(np_out->value==LOGIC_0) && ! (np_out->value==LOGIC_DBAR)) {
                  return false;
               }
            }
            if(np->value==LOGIC_1) {
               if(np_out->value==LOGIC_X) {
                  np_out->value = LOGIC_1;
                  imply.push_back(np_out->indx);
               } else if (np_out->num!=(int)(check_fault.first) &&!(np_out->value==LOGIC_1) && ! (np_out->value==LOGIC_D)) {
                  return false;
               }
            }
            if(np->value==LOGIC_D || np->value==LOGIC_DBAR ) {
               if(np_out->value==LOGIC_X) {
                  np_out->value = np->value;   
                  imply.push_back(np_out->indx);
               }
            }
         }

         else if(np_out->type==GATE_NOT){
            if(np->value==LOGIC_0) {
               if(np_out->value==LOGIC_X) {
                  np_out->value = LOGIC_1;   
                  imply.push_back(np_out->indx);
               } else if (np_out->num!=(int)(check_fault.first) && !(np_out->value==LOGIC_1)  && ! (np_out->value==LOGIC_D) ) {
                  return false;
               }
            }
            if(np->value==LOGIC_1) { 
               if(np_out->value==LOGIC_X) { 
                  np_out->value = LOGIC_0;   
                  imply.push_back(np_out->indx);
               } else if (np_out->num!=(int)(check_fault.first) && !(np_out->value==LOGIC_0) && ! (np_out->value==LOGIC_DBAR)) {
                  return false;
               }
            } 
            if(np->value==LOGIC_D) { 
               if(np_out->value==LOGIC_X) { 
                  np_out->value = LOGIC_DBAR;   
                  imply.push_back(np_out->indx);
               }       
            }
            if(np->value==LOGIC_DBAR) { 
               if(np_out->value==LOGIC_X) {
                  np_out->value = LOGIC_D;   
                  imply.push_back(np_out->indx);
               }
            }
         }//not

         else if(np_out->type==GATE_XOR){
         //try to imply
            if(np_out->value ==LOGIC_X && Xnum==0 ) { 
               if(Dnum%2==0 && Dbarnum%2==0) {
                  if(Onenum%2==1) {
                     np_out->value=LOGIC_1;
                  } else {
                     np_out->value=LOGIC_0;
                  } 
                  imply.push_back(np_out->indx);
               } else if (Dnum==Dbarnum && Onenum%2==1) {
                  np_out->value=LOGIC_0; 
                  imply.push_back(np_out->indx);
               } else if(Dnum==Dbarnum && Onenum%2==0) {
                  np_out->value=LOGIC_1; 
                  imply.push_back(np_out->indx);
               }
            }
            
            // check
            else if(np_out->value==LOGIC_1 && Xnum==0) { 
               if(Dnum%2==0 && Dbarnum%2==0) {
                  if(Onenum%2==1) {
                     if(np_out->value==LOGIC_0) {
                        return false;
                     }
                  }
               } else if(Dnum==Dbarnum && Onenum%2==0) { 
                  if(np_out->value==LOGIC_0) {
                     return false;
                  }
               }
            } else if(np_out->value==LOGIC_0 && Xnum==0) { 
               if(Dnum%2==0 && Dbarnum%2==0) {
                  if(Onenum%2==0) {
                     if(np_out->value==LOGIC_1) {
                        return false;
                     }
                  }
               } else if(Dnum==Dbarnum && Onenum%2==1) {
                  if(np_out->value==LOGIC_1) {
                     return false;
                  }
               }
            }
         }//xor
            
         else if(np_out->type==GATE_OR){
            //try to imply
            if(np_out->value ==LOGIC_X ) { 
               if(Onenum>0 ||  (Dnum>0&& Dbarnum>0)) {
                  np_out->value=LOGIC_1; 
                  imply.push_back(np_out->indx);
               } else if (Xnum==0 && Zeronum==np_out->fin) {
                  np_out->value=LOGIC_0; 
                  imply.push_back(np_out->indx);
               } else if( (Zeronum+Dnum) == np_out->fin) {
                  np_out->value=LOGIC_D; 
                  imply.push_back(np_out->indx);
               } else if( (Zeronum+Dbarnum) == np_out->fin) {
                  np_out->value=LOGIC_DBAR; 
                  imply.push_back(np_out->indx);
               }
            }
            // check
            else if(np_out->value==LOGIC_1) {
               if(!(Onenum>0 ||  (Dnum>0&& Dbarnum>0))) {
                  return false;
               }
            } else if (np_out->value==LOGIC_0) {
               if(!(Xnum==0 && Zeronum==np_out->fin)) {
                  return false;
               }
            }
         }//or
      
         else if(np_out->type==GATE_NOR) {
            //try to imply
            if(np_out->value ==LOGIC_X ) { 
               if(Onenum>0 ||  (Dnum>0&& Dbarnum>0)) {
                  np_out->value=LOGIC_0; 
                  imply.push_back(np_out->indx);
               } else if (Xnum==0 && Zeronum==np_out->fin) {
                  np_out->value=LOGIC_1; 
                  imply.push_back(np_out->indx);
               } else if ((Zeronum+Dnum) == np_out->fin) {
                  np_out->value=LOGIC_DBAR; 
                  imply.push_back(np_out->indx);
               } else if ((Zeronum+Dbarnum) == np_out->fin) {
                  np_out->value=LOGIC_D; 
                  imply.push_back(np_out->indx);
               }
            }
            // check
            else if(np_out->value==LOGIC_0) {
               if (!(Onenum>0 ||  (Dnum>0&& Dbarnum>0))) {
                  return false;
               }
            } else if (np_out->value==LOGIC_1) {
               if (!(Xnum==0 && Zeronum==np_out->fin)) {
                  return false;
               }
            }
         }//nor

         else if(np_out->type==GATE_NAND){
            //try to imply
            if(np_out->value ==LOGIC_X ) {
               if(Zeronum>0 ||  (Dnum>0&& Dbarnum>0)) {
                  np_out->value=LOGIC_1; 
                  imply.push_back(np_out->indx);
               } else if(Xnum==0 && Onenum==np_out->fin) {
                  np_out->value=LOGIC_0; 
                  imply.push_back(np_out->indx);
               } else if((Onenum+Dnum) == np_out->fin) {
                  np_out->value=LOGIC_DBAR; 
                  imply.push_back(np_out->indx);
               } else if((Onenum+Dbarnum) == np_out->fin) {
                  np_out->value=LOGIC_D; 
                  imply.push_back(np_out->indx);
               }
            }
            // check
            else if (np_out->value==LOGIC_1) { 
               if(!(Zeronum>0 ||  (Dnum>0&& Dbarnum>0))) return false;
            } else if (np_out->value==LOGIC_0) {
               if(!(Xnum==0 && Onenum==np_out->fin)) return false;
            }
         }//nand
      
         else if(np_out->type==GATE_AND){
            //try to imply
               if(np_out->value ==LOGIC_X ) { 
                     if(Zeronum>0 ||  (Dnum>0&& Dbarnum>0))    {np_out->value=LOGIC_0; imply.push_back(np_out->indx);}
                     else if(Xnum==0 && Onenum==np_out->fin)   {np_out->value=LOGIC_1; imply.push_back(np_out->indx);}
                     else if( (Onenum+Dnum) == np_out->fin)    {np_out->value=LOGIC_D; imply.push_back(np_out->indx);}
                     else if( (Onenum+Dbarnum) == np_out->fin) {np_out->value=LOGIC_DBAR; imply.push_back(np_out->indx);}
               }
               // check
               else if(np_out->value==LOGIC_0) { if(!(Zeronum>0 ||  (Dnum>0&& Dbarnum>0))) return false;}
               else if(np_out->value==LOGIC_1) {if(!(Xnum==0 && Onenum==np_out->fin)) return false;}
         }//and
      }  								
	}
   
   return true;
}


bool Dalg(int level) {
	
   int time_elapsed = (clock() - dalg_recursion_tStart)/(CLOCKS_PER_SEC);
   if (time_elapsed > 1) {
      return false;
   }


	// Dalg_count++;
	// if(Dalg_count> 20000) return false;
	for ( int kk=0;kk< Nnodes;kk++) {
	   if( Node[kk].assign_level > level) {
         Node[kk].value=LOGIC_X; 
         Node[kk].assign_level=-1;
      }
	}

	if(imply_and_check()) {
		for(int kk=0; kk< imply.size(); kk++) { 
         Node[imply[kk]].assign_level=level;  
      }  
		//getDfront();
	} else {
		for(int i=0; i<imply.size(); i++ ){  
         Node[imply[i]].value=LOGIC_X; 
         Node[imply[i]].assign_level=-1; 
      }
	   imply.clear(); 
      return false;
   }

   imply.clear();
   getDfront();
   bool DatOut=false;
   NSTRUC *np;
   for(int i = 0; i<Npo; i++) {  // check D or D' at output
      if(Poutput[i]->value==LOGIC_D||Poutput[i]->value==LOGIC_DBAR) {
         DatOut=true;
      }
   }
   // propagate D frontier to output
   if(!DatOut) {
      if(Dfront.size()==0) {
         unassign(level-1); 
         return false;
      } else {
         int iter_while= 0;
         for(int i = 0; i<Dfront.size(); i++) {
         
         np = &Node[Dfront[i]];
         
         int Dnum=0; 
         int Dbarnum=0, Onenum=0, Zeronum=0;
         for(int j=0;j<(np->fin); j++ ) {
            if(np->unodes[j]->value == LOGIC_D) Dnum++;
            else if(np->unodes[j]->value == LOGIC_DBAR) Dbarnum++;
            else if(np->unodes[j]->value == LOGIC_1) Onenum++;
            else if(np->unodes[j]->value == LOGIC_0) Zeronum++;
         }
         
         if ( (Dnum==0 && Dbarnum==0)|| np->value!=LOGIC_X ||(Dnum>0 && Dbarnum>0) ) {
            continue;
         }
         
         if(Dnum>0) {
            if(np->type == GATE_OR || np->type== GATE_AND || np->type==GATE_BRANCH || np->type==GATE_XOR)      {   np->value=LOGIC_D; } 
            else if(np->type ==GATE_NOR || np->type==GATE_NAND || np->type==GATE_NOT) {   np->value=LOGIC_DBAR; }
            
            imply.push_back(np->indx);
            for(int j=0;j<(np->fin); j++ ){
               if( np->unodes[j]->value == LOGIC_X) {
                  if(np->type == GATE_OR || np->type==GATE_NOR || np->type==GATE_XOR) { np->unodes[j]->value=LOGIC_0; } 
            else if(np->type ==GATE_NAND || np->type==GATE_AND) { np->unodes[j]->value=LOGIC_1; }
            imply.push_back(np->unodes[j]->indx);
            }
         }	

            
         }
         else if(Dbarnum>0) {
            if(np->type == GATE_OR || np->type==GATE_AND  || np->type==GATE_BRANCH || np->type==GATE_XOR)      {   np->value=LOGIC_DBAR; } 
            else if(np->type ==GATE_NOR || np->type==GATE_NAND || np->type==GATE_NOT) {   np->value=LOGIC_D; }
            
            imply.push_back(np->indx);
            for(int j=0;j<(np->fin); j++ ){
            if( np->unodes[j]->value ==LOGIC_X){
            
                              if(np->type ==GATE_OR || np->type==GATE_NOR || np->type==GATE_XOR) { np->unodes[j]->value=LOGIC_0; } 
            else if(np->type == GATE_NAND || np->type==GATE_AND) { np->unodes[j]->value=LOGIC_1; }
            imply.push_back(np->unodes[j]->indx);
            }
         }
         
         }
         int pop_ind=Dfront[0];
         
            if(Dalg(level+1)) {
               return true;
            } else {  
               unassign(level); 
               getDfront();
            }
         }
         unassign(level-1);
         return false;
      }
   }
	//check Jfront
	vector<int> temp2; temp2.clear();
	for(int i=0;i <Jfront.size();i++){
	      int Xnum=0;
			int Dnum=0;
			int Dbarnum=0;
			int Onenum=0;
			int Zeronum=0;
			np = &Node[Jfront[i]];
			for(int j=0;j< np->fin ;j++){
													if      (np->unodes[j]->value==LOGIC_X) Xnum++;
													else if (np->unodes[j]->value==LOGIC_D) Dnum++;
													else if (np->unodes[j]->value==LOGIC_DBAR) Dbarnum++;
													else if (np->unodes[j]->value==LOGIC_1) Onenum++; 
													else if (np->unodes[j]->value==LOGIC_0) Zeronum++;
			}
			if(Node[Jfront[i]].value==LOGIC_1 && np->type==GATE_OR) { // or 
				if(Xnum>0 && !(Dnum>0&& Dbarnum>0) && Onenum==0) {temp2.push_back(Jfront[i]);}
			}
			else if(Node[Jfront[i]].value==LOGIC_1 && np->type==GATE_NAND) { //  nand
				if(Xnum>0 && !(Dnum>0&& Dbarnum>0) && Zeronum==0) {temp2.push_back(Jfront[i]);}
			}
			else if(Node[Jfront[i]].value==LOGIC_0 && np->type==GATE_NOR){ // nor
				if(Xnum>0 && !(Dnum>0&& Dbarnum>0) && Onenum==0 ) {temp2.push_back(Jfront[i]);}
			}
			else if(Node[Jfront[i]].value==LOGIC_0 && np->type==GATE_AND){ //and
				if(Xnum>0 && !(Dnum>0&& Dbarnum>0) && Zeronum==0) {temp2.push_back(Jfront[i]);}
			}
	}
	Jfront= temp2;
	if(temp2.size()==0) return true;
	for(int i=0;i <temp2.size();i++){
			int Xnum=0;
			int Dnum=0;
			int Dbarnum=0;
			int Onenum=0;
			int Zeronum=0;
			np = &Node[temp2[i]];
			for(int j=0;j< np->fin ;j++){
			    if(np->unodes[j]->value==LOGIC_X){
					if(np->type==GATE_OR || np->type==GATE_NOR){
					    np->unodes[j]->value =  LOGIC_1;   imply.push_back(np->unodes[j]->indx);	
						 if(Dalg(level+1)) {return true;} else { unassign(level);}
						np->unodes[j]->value =  LOGIC_0;   imply.push_back(np->unodes[j]->indx);
						np->unodes[j]->assign_level= level;

					}
					else if(np->type==GATE_AND || np->type==GATE_NAND){
						 np->unodes[j]->value = LOGIC_0;   imply.push_back(np->unodes[j]->indx);	
						 if(Dalg(level+1)) {return true;} else { unassign(level);}
						 np->unodes[j]->value =  LOGIC_1;   imply.push_back(np->unodes[j]->indx);
						 np->unodes[j]->assign_level= level;
						 
					}
				}
			}
			imply.clear();
			unassign(level-1);
			return false;
	}
}



bool DalgCall(pair<int,int> fault){

	Dfront.clear();
	Jfront.clear(); 
   imply.clear();
   dalg_recursion_tStart = clock();
	Dalg_count=0;
   for(int kk=0;kk< Nnodes; kk++){
      Node[kk].value= LOGIC_X; 
      Node[kk].assign_level = -1;
   }
   NSTRUC *np =&Node[(int) (fault.first)];
	check_fault = fault;
   for(int kk=0;kk< np->fout;kk++){
      Dfront.push_back(np->dnodes[kk]->indx);      // setup d frontier
   }
	if (fault.second == 0) {
      np->value= LOGIC_D; 
      if(np->type==GATE_BRANCH) {
         np->unodes[0]->value=LOGIC_1; 
      }
      imply.push_back(np->indx);
   } else {  
      np->value= LOGIC_DBAR; 
      if(np->type==GATE_BRANCH) {
         np->unodes[0]->value=LOGIC_0; 
      }
      imply.push_back(np->indx);
   }//D 
	
	bool find=Dalg(0);
   
   if(find) { 
      vector<int> temp; 
      temp.clear();
      for(int i=0;i<Npi  ;i++){
         if(Pinput[i]->value==LOGIC_D) {
            temp.push_back(1);
         } else if(Pinput[i]->value==LOGIC_DBAR) {
            temp.push_back(0);
         } else {
            temp.push_back(Pinput[i]->value);
         }
      }
      test_vectors.push_back(temp);
      return true;
   }
   else return false;
}


int dalg (char *cp) {

   char faultNode_buf[MAXLINE], faultValue_buf[MAXLINE];
   sscanf(cp, "%s %s", faultNode_buf, faultValue_buf);

   int faultNode = stoi(faultNode_buf);
   int faultValue = stoi(faultValue_buf);
   int faultNodeIndx;

   lev();

   for (int i = 0; i < Nnodes; i++) {
      if (Node[i].num == faultNode) {
         faultNodeIndx = Node[i].indx;
      }
   }

   DalgCall(make_pair(faultNodeIndx, faultValue));

   int initial;
   if (test_vectors.size() != 0) {
      string output_file = circuitName + "_DALG_" + faultNode_buf + "@" + faultValue_buf + ".txt";
      ofstream output_test_pattern_file;
      output_test_pattern_file.open(output_file);
      if ( output_test_pattern_file ) {
         initial = 0;
         for(int i=0;i<Npi  ;i++){
            if (initial == 1) {
               output_test_pattern_file << ",";
            }
            initial = 1;
            output_test_pattern_file << Pinput[i]->num;
         }
         output_test_pattern_file << endl;
         initial = 0;

         for (int j=0; j<test_vectors.size(); j++){
            vector<int> tmp = test_vectors[j];
            initial = 0;
            for(int m=0;m< tmp.size(); m++){
               if (initial == 1) {
                        output_test_pattern_file << ",";
               }
               initial = 1;
               if (tmp[m] == LOGIC_X) {
                  output_test_pattern_file << "X";
               } else {
                  output_test_pattern_file << tmp[m];
               }
            }
            output_test_pattern_file << endl;
         } 
      }
      output_test_pattern_file.close();
      return 0;
   } else {
      return 1;
   }
}



int atpg_det(char *cp) {
   // time
   // read
   // rfl
   // for all faults
   // --podem/dalg
   // --add pattern
   // print patterns
   // rtg - logic for fc
   // report

   // start clock
   const auto before = std::chrono::system_clock::now();
   //clock_t tStart = clock();

   char circuit_name[MAXLINE], alg_name[MAXLINE];
   sscanf(cp, "%s %s", circuit_name, alg_name);
   
   string alg_name_str = alg_name;
   transform(alg_name_str.begin(), alg_name_str.end(), alg_name_str.begin(), ::toupper);

   // read circuit
   cread((circuit_name));

   // generate fault list
   string rfl_arguments = "atpg_det_rfl.out";
   rfl(strdup(rfl_arguments.c_str()));

   string token;
   int i;
   // read fault list
   vector<pair<int,int> > fault_list;
   fault_list.clear();
   pair<int,int> fault;
   ifstream fault_file;
   fault_file.open(rfl_arguments);
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
      return 1;
   }
   
   vector<vector<int> > test_patterns;
   test_patterns.clear();
   vector<int> test_pattern;
   test_pattern.clear();
   for (int i = 0; i < Npi; i++) {
      test_pattern.push_back(Pinput[i]->num);
   }
   test_patterns.push_back(test_pattern);
   test_pattern.clear();

   int node_num = 1;
   string alg;
   string in_pattern_buf = circuitName + "_DALG_" + to_string(fault_list[i].first) + "@" + to_string(fault_list[i].second) + ".txt";

   for (int i=0; i<fault_list.size(); i++) {
      if (alg_name_str == "DALG") {
         string dalg_arguments = to_string(fault_list[i].first) + " " + to_string(fault_list[i].second);
         int x = dalg(strdup(dalg_arguments.c_str()));
         alg = "DALG";
         if (x == 0) {
            // read test patterns
            vector<int> temp; 
            temp.clear();
            for(int i=0;i<Npi  ;i++){
               if(Pinput[i]->value==LOGIC_D) {
                  temp.push_back(1);
               } else if(Pinput[i]->value==LOGIC_DBAR) {
                  temp.push_back(0);
               } else if (Pinput[i]->value==LOGIC_X) {
                  temp.push_back(rand()%2);
               } else {
                  temp.push_back(Pinput[i]->value);
               }
            }
            test_patterns.push_back(temp);
         }
      } else if (alg_name_str == "PODEM") {
         string podem_arguments = to_string(fault_list[i].first) + " " + to_string(fault_list[i].second);
         int x = podem(strdup(podem_arguments.c_str()));
         alg = "PODEM";
         if (x == 0) {  // if not timeout
            for (int j = 0; j < Nnodes; j++) {

               if (Node[j].fin == 0) {
                  if (Node[j].value == LOGIC_X) {
                     Node[j].value = rand()%2;
                  } else if (Node[j].value == LOGIC_D) {
                     Node[j].value = LOGIC_1;
                  } else if (Node[j].value == LOGIC_DBAR) {
                     Node[j].value = LOGIC_0;
                  }
                  test_pattern.push_back(Node[j].value);
               }
            }
            test_patterns.push_back(test_pattern);
            test_pattern.clear();
         }
      } else {
         cout << "invalid argument" << endl;
         return 1;
      }

   }

   // write patterns to output file
   bool first = true;
   string atpg_det_output_patterns = circuitName + "_" + alg + "_ATPG_patterns.txt";
   ofstream output_file;
   output_file.open(atpg_det_output_patterns);
   if ( output_file ) {
      for (auto const& element : test_patterns) {
         first = true;
         for (auto pattern : element) {
            if (!first){
               output_file << ",";
            }
            first = false;
            output_file << pattern;
         }
         output_file << endl;
      }
   } else {
      cout << "Couldn't create file\n";
      return 1;
   }

   // fault coverage calculation
   set<pair<int,int> > detected_faults;

   string pfs_arguments = atpg_det_output_patterns + " " + rfl_arguments + " " + "detected_faults_temp.txt";
   pfs(strdup(pfs_arguments.c_str()));

   // read fault list
   fault_file.open("detected_faults_temp.txt");
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
   } else {
      cout << "Couldn't open file\n";
   }

   const sec duration = std::chrono::system_clock::now() - before;
   string atpg_det_output_report = circuitName + "_" + alg + "_ATPG_report.txt";
   ofstream output_report;
   output_report.open(atpg_det_output_report);
   if ( output_report ) {
      output_report << "Algorithm: " << alg << endl;
      output_report << "Circuit: " << circuitName << endl;
      output_report << "Fault Coverage: " << fixed << setprecision(2) << detected_faults.size()*100.0/fault_list.size() << "%" << endl;
      output_report << "Time: " << duration.count() << " seconds" << endl;
      output_report.close();
   } else {
      cout << "Couldn't create file\n";
      return 1;
   }

   cout << "OK" << endl;

   printf("Time taken: %.2fs\n", duration.count());
   return 0;
}


int atpg(char *cp) {
   // time
   // read
   // lev
   // rfl
   // random test generation
   // calculate fc after every Nnodes/10 patterns
   // if difference in fc is less than 10%
   // -- switch to podem
      // for all faults
      // --podem/dalg
      // --add pattern
   // print patterns
   // logic for fc
   // report

   // start clock
   const auto before = std::chrono::system_clock::now();

   char circuit_name[MAXLINE], alg_name[MAXLINE];
   sscanf(cp, "%s %s", circuit_name, alg_name);
   
   // read circuit
   cread((circuit_name));

   lev();
   
   // random test generation
   vector<pair<int, int> > fault_list;     // dictionary to hold PO values
   fault_list.clear();
   vector<pair<int,int> > fault_list_drop;
   fault_list_drop.clear();
   pair<int,int> fault;    // temporarily holds the faults

   vector<int> PI;
   PI.clear();
   vector<int> input_values;
   input_values.clear();
   vector<vector<int> > test_patterns;
   test_patterns.clear();

   NSTRUC *np;

   // get all faults
   for (int i = 0; i < Nnodes; i++){    // iterate over all the nodes
      np = &Node[i];

      // add to PI vector
      if (np->fin == 0) {
         PI.push_back(np->num);
      }

      // add faults
      fault.first = np->num;
      for (int j = 0; j < 2; j++) {     // assign value of 0 and 1 to the faulty node
         fault.second = j; 
         fault_list.push_back(fault);     // add the fault to the fault_list
         fault_list_drop.push_back(fault);   // add fault to the fault list after dropping - we will use this later
      }
   }

   string test_pattern_buf = circuitName + "_ATPG_patterns.txt";

   ofstream output_test_pattern_file;
   output_test_pattern_file.open(test_pattern_buf);
   // print test patterns to a file
   int flag = 0;
   if ( output_test_pattern_file ) {
      for (int i = 0; i < PI.size(); i++) {
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

   set<pair<int,int> > detected_faults;
   int fc=0, fc_old=0;

   int test_patterns_generated = 0;
   srand(time(0));
   while (((fc==0)&(fc_old==0)) | ((fc-fc_old > 5)&(fault_list_drop.size() != 0))) {
      test_patterns.clear();
      test_patterns.push_back(PI);
      for (int i = 0; i < Nnodes/10; i++) {     // todo change number of test_patterns generated in each iteration
         for (int j = 0; j < PI.size(); j++) {
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
         for (int i = 0; i < fault_list_drop.size(); i++) {
            output_file << fault_list_drop[i].first << "@" << fault_list_drop[i].second << endl;
         }
      } else {
         cout << "Couldn't create file\n";
      }
      output_file.close();
      
      int flag = 0;
      output_file.open("test_pattern_temp.txt");
      if ( output_file ) {
         for (int i = 0; i < test_patterns.size(); i++) {
            for (int j = 0; j < test_patterns[0].size(); j++) {
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
      int i;
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
            for (int j = 0; j < test_patterns[0].size(); j++) {
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

      // calculate FC and update old FC
      fc_old = fc;
      fc = detected_faults.size()*100.0/fault_list.size();

      // update fault_list_drop
      for (auto element : detected_faults) {
         fault_list_drop.erase(remove(fault_list_drop.begin(), fault_list_drop.end(), element), fault_list_drop.end());
      }

   }
   cout << "Fault List size (after dropping): " << fault_list_drop.size() << endl;
   cout<< "FC: " << fc << "%" << endl;
   cout << "done with Random, starting ATPG_DET" << endl;
   test_patterns.clear();
   test_patterns.push_back(PI);
   // podem
   vector<int> test_pattern;
   test_pattern.clear();
   for (int i=0; i<fault_list_drop.size(); i++) {
      string podem_arguments = to_string(fault_list_drop[i].first) + " " + to_string(fault_list_drop[i].second);
      int x = podem(strdup(podem_arguments.c_str()));
      if (x == 0) {  // if not timeout
         for (int i = 0; i < Nnodes; i++) {

            if (Node[i].fin == 0) {
               if (Node[i].value == LOGIC_X) {
                  Node[i].value = rand()%2;
               } else if (Node[i].value == LOGIC_D) {
                  Node[i].value = LOGIC_1;
               } else if (Node[i].value == LOGIC_DBAR) {
                  Node[i].value = LOGIC_0;
               }
               test_pattern.push_back(Node[i].value);
            }
         }
         test_patterns.push_back(test_pattern);
         test_pattern.clear();
      }
   }

// -------------------------------------------------
      // write the fault list and test patterns to temp files inorder to pass them to PFS
      ofstream output_file;
      output_file.open("fault_list_temp.txt");
      if ( output_file ) {
         for (int i = 0; i < fault_list_drop.size(); i++) {
            output_file << fault_list_drop[i].first << "@" << fault_list_drop[i].second << endl;
         }
      } else {
         cout << "Couldn't create file\n";
      }
      output_file.close();
      
      flag = 0;
      output_file.open("test_pattern_temp.txt");
      if ( output_file ) {
         for (int i = 0; i < test_patterns.size(); i++) {
            for (int j = 0; j < test_patterns[0].size(); j++) {
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
      int i;
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
            for (int j = 0; j < test_patterns[0].size(); j++) {
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

      // done with atpg -report
   const sec duration = std::chrono::system_clock::now() - before;

   string atpg_det_output_report = circuitName + "_ATPG_report.txt";
   ofstream output_report;
   output_report.open(atpg_det_output_report);
   if ( output_report ) {
      output_report << "Circuit: " << circuitName << endl;
      output_report << "Fault Coverage: " << fixed << setprecision(2) << detected_faults.size()*100.0/fault_list.size() << "%" << endl;
      output_report << "Time: " << duration.count()  << " seconds" << endl;
      output_report.close();
   } else {
      cout << "Couldn't create file\n";
      return 1;
   }

   cout << "OK" << endl;

   printf("Time taken: %.2fs\n", duration.count()) ;
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
