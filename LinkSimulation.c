/*
0 2 5 1 -1
2 0 8 7  9
5 8 0 -1 4
1 7 -1 0 2
-1 9 4 2 0
*/

#include <stdio.h>
#include <unistd.h>
#include <termios.h>


#define MAX_NODECOUNT 100
#define Color_Red "\x1b[31;1m" // Color Start
#define Color_Blue "\x1b[34;1m" // Color Start
#define Color_end "\x1b[39;49m" // To flush out prev settings

#define LOG_RED(X) printf("%s%c%s",Color_Red,X,Color_end)

#define LOG_BLUE(X) printf("%s%s%s",Color_Blue,X,Color_end)
#define LOG_BLUE_ARG(X) printf X

//#define DEBUG_INI 0;
//#define DEBUG 0;

#ifdef DEBUG_INI
# define DEBUG_PRINT_INI(x) printf x
#else
# define DEBUG_PRINT_INI(x) do {} while (0)
#endif

#ifdef DEBUG
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

/*
 * Used in the shortes path tree. pathnode is used to represent each node 
 * 
 */
struct pathnode
{
   //Indicates the cumulative weight till the node form source
   int weight_cumulative;
   //Indicates that the node has been marked and its minimum distance from source found
   int flag_inpath;
   //Indicates the index of the previous node in the path
   int prev_index;

};

/*
 * node represents a router node
 * 
 */
struct node
{
  //Indicates the index of the router
  int node_index;
  //Indicates the graph topology
  int topology[MAX_NODECOUNT][MAX_NODECOUNT];
  //Can be used to keep track of LSP received. Not used in this project as LSP is sent only once
  int max_seq_recvd[MAX_NODECOUNT];
  //Represents the shortest path tree with the router as the source
  struct pathnode path[MAX_NODECOUNT];
};

//Represents all routers/nodes in the graph. Used to set each one
struct node all_nodes[MAX_NODECOUNT];

//Represents the entire node topology
int original_topology[MAX_NODECOUNT][MAX_NODECOUNT];

//Filename used for loading the data
char filename[30] ;
int nodeCount = 0;

/*Function to do the dos getch()
 * http://stackoverflow.com/questions/6856635/hide-password-input-on-terminal
 */
int getch() {
    struct termios oldtc, newtc;
    int ch;
    tcgetattr(STDIN_FILENO, &oldtc);
    newtc = oldtc;
    newtc.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newtc);
    ch=getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldtc);
    return ch;
}

/*
 * Function used for updating the LSP message information at a router indicated by destindex
 */
void updateLSP(int senderIndex, int srcindex, int destindex, int * LSPArray, int seqNumber)
{
  DEBUG_PRINT_INI(("\nSending LSPArray from src %d through sender %d to dest %d",srcindex,senderIndex,destindex));
  
  
  //get the destindex node
  struct node * dst_node = &all_nodes[destindex];
  
  //Check the seqid of dest to check if this message has already been received at the dest from this 	//source
  if(dst_node->max_seq_recvd[srcindex] >= seqNumber)
    return;

  dst_node->max_seq_recvd[srcindex] = seqNumber;
  
  //update the destindex nodes topology matrix with the the LSPArray
  int neighbour_index=0;
  for(;neighbour_index<nodeCount;neighbour_index++)
  {
      dst_node->topology[srcindex][neighbour_index] = LSPArray[neighbour_index];
  }

  //Send the LSPArray to all neighbours other than the one it came from
  neighbour_index=0;
  for(;neighbour_index<nodeCount;neighbour_index++)
  {
    
    //Check if a neighbour exists
    if(dst_node->topology[destindex][neighbour_index] > 0)
    {
      if(neighbour_index != srcindex && neighbour_index != senderIndex)
	updateLSP(destindex, srcindex,neighbour_index,LSPArray,1); 
    }
  }
   //
}

/*
 * Function reads from the specified file and automatically counts the number of nodes
 * It loads the original_topology matrix with integer values from the file
 */
int loadOriginalTopology()
{
   //read the network.txt to form a matrix

   //Initialize the nodecount variable
   int n, m;
  
  FILE *inp; 
  if((inp = fopen(filename,"r"))==NULL)
    return -1;
  
  //Read number of nodes
  char fchar;
  int prevIsChar = 0;
  nodeCount = 0;
  while((fchar = fgetc(inp)) != '\n')
  {
    if(fchar>='0' && fchar<='9' && !prevIsChar)
    {
      prevIsChar = 1;
      nodeCount++;
    }
    else
    {
      prevIsChar =0;
    }
  }
  
  //Reset the input to start of the file
  fseek(inp, 0, SEEK_SET);
  for (n = 0; n < nodeCount; ++n) {
  for (m = 0; m < nodeCount; ++m)
    fscanf (inp, "%d", &original_topology[n][m]);
  }
  fclose (inp); 
  
  return 1;
}

void DebugPrintOriginalTopology()
{
  int n,m;
  LOG_BLUE("\n\n\tOriginal routing table is as follows \n\t");
  for (n = 0; n < nodeCount; ++n) 
  {
    for (m = 0; m < nodeCount; ++m)
    {
      LOG_BLUE_ARG(("%s%d%s  ",Color_Blue,original_topology[n][m],Color_end));
      //printf("%d  ",original_topology[n][m]);
    }
     LOG_BLUE(("\n\t"));
  }
  
}

void DebugPrintAllNodeToppology()
{
  int n,m;
  for (n = 0; n < nodeCount; ++n) 
  {
    struct node * current_node = &all_nodes[n];
    DEBUG_PRINT_INI(("\n\n\tTopology for Currentnode Index :%d\n",current_node->node_index));
    
    int k;
    for (k = 0; k < nodeCount; ++k) 
    {
      for (m = 0; m < nodeCount; ++m)
      {
	DEBUG_PRINT_INI(("%d  ",current_node->topology[k][m]));
      }
      DEBUG_PRINT_INI(("\n"));
    }
     
  }
}

int InitializeNodes()
{
   //Load the topology
   if(loadOriginalTopology()<0)
   {
     return -1;
   }

   //Initialize node.topology from the original_topology matrix
   //So that each node only konws its own topology
   int node_index=0;
   struct node * current_node;
   for(;node_index<nodeCount;node_index++)
   {
     //Get the node to be initialized
     current_node =&all_nodes[node_index];
     
     //Initialize the nodeindex
     
      
     current_node->node_index = node_index;
     DEBUG_PRINT_INI(("Current node initialized : %d \n",current_node->node_index));
     int neighbour_index=0;
     for(;neighbour_index<nodeCount;neighbour_index++)
     {
       current_node->topology[node_index][neighbour_index] = original_topology[node_index][neighbour_index];
       DEBUG_PRINT_INI((" %d ",  current_node->topology[node_index][neighbour_index]));
     }
     DEBUG_PRINT_INI(("\n"));
   }
   
  //for each node in the n/w, create and send LSP packages with apt seq numbers
  //Create LSP packet
  DEBUG_PRINT_INI(("Current node initialized : %d \n",all_nodes[2].node_index));
  
  node_index=0;
  for(;node_index<nodeCount;node_index++)
  {
    //Get the node to be initialized
    current_node =&all_nodes[node_index];
    DEBUG_PRINT_INI(("Currentnode.node_index : %d node_index : %d \n",current_node->node_index,node_index));

    //Send LSP packet to neighbours
    int neighbour_index=0;
    for(;neighbour_index<nodeCount;neighbour_index++)
    {
      //Check if a neighbour exists
      if(current_node->topology[node_index][neighbour_index] > 0)
      {
	int src = current_node->node_index;
	int dst = neighbour_index;
	int * LSPArray = current_node->topology[src];
	updateLSP(src,src,dst,LSPArray,1); 
      }
    }
  } 
  DebugPrintOriginalTopology();
  return 1;
}

void SetPath(struct pathnode * pnode, int weight_cumulative, int prev_index,int flag_inpath)
{
  //pnode->index = index;
  pnode->weight_cumulative = weight_cumulative;
  pnode->prev_index = prev_index;
  pnode->flag_inpath = flag_inpath;
}

void GenerateRoutingTable(int index)
{
   DEBUG_PRINT("Generating Routing Table \n");
   //Get the node at index
   struct node * router = &all_nodes[index];

   //Initialize the root of shortest path tree 
   //Set the source
   
   SetPath(&router->path[index],0,-1,0);   

   
   //Set the neighbours using the router topology
   int neighbour =0;
   for(;neighbour<nodeCount;neighbour++)
   {
     int weight = router->topology[index][neighbour];
     if(weight > 0 && neighbour != index)
     {   
       //Directly connected neighbours
       SetPath(&router->path[neighbour],weight,index,0);
     }
     else if(weight < 0 && neighbour != index)
     {
       //Neighbours who are not directly connected
       //Set all other nodes weight_cumulative to infinity say 999999 and prev to -1
       SetPath(&router->path[neighbour],999999,-1,0);
     }
   }
   
   //print the path array;
   DEBUG_PRINT(("The shortest path array before Dijkstra : \n index\t\t prev node\t\t cum weight\t\t flag\n"));
   int j=0;
   for(;j<nodeCount;j++)
   {
     DEBUG_PRINT(("\t%d\t\t %d\t\t %d\t\t %d\n", j, router->path[j].prev_index, router->path[j].weight_cumulative, router->path[j].flag_inpath));     
   }
   
   //Dijkstras algorithm to generate the shortest path tree
   int include_count = 0;
   while(include_count < nodeCount+1)
   {
     //Find minimum weighted node for whom flag_inpath = 0
     int minweight = 999999, minnode=-1;
     int i = 0;
     
     //Get index of the least weighted node
     for(; i < nodeCount; i++)
     {
       int cum_weight = router->path[i].weight_cumulative;
       if(!router->path[i].flag_inpath && cum_weight<=minweight)
       {
	 minweight = cum_weight;
	 //Set index of the least weighted node
	 minnode = i;
	 DEBUG_PRINT((" Min node found :%d Include Count : %d\n",minnode,include_count));
       }
     }
     
     //If min node is found, update the shortest path tree
     if(minnode != -1)
     {
       //Add the minimum weight node to the path
       
      router->path[minnode].flag_inpath = 1;
      
      //Update the neighbours of minnode with new weights
      int  * neighbour_array = router->topology[minnode];
      int i = 0;
      for(; i < nodeCount; i++)
      {
	int edgeweight = *(neighbour_array + i);
	//Check and make sure neighbour not flagged already
	if(edgeweight > 0 && !router->path[i].flag_inpath)
	{
	  //Update the weight_cumulative if new weight is less than prev weight
	  int prev_weight = router->path[i].weight_cumulative;
	  int new_weight = router->path[minnode].weight_cumulative + edgeweight;
	  
	  
	  if(new_weight < prev_weight)
	  {
	    //Update the prev
	    router->path[i].prev_index = minnode;
	    
	    //Update the weight
	    router->path[i].weight_cumulative = new_weight;
	    
	  }
	}
      }
     }
      include_count++;
     //safe++;
     
     
   }
   
   //print the path array;
   DEBUG_PRINT(("The shortest path array : \n index\t\t prev node\t\t cum weight\t\t flag\n"));
   j=0;
   for(;j<nodeCount;j++)
   {
     DEBUG_PRINT(("\t%d\t\t %d\t\t %d\t\t %d\n", j, router->path[j].prev_index, router->path[j].weight_cumulative, router->path[j].flag_inpath));
     
   }
   //Generate the routing table for the node. Use DFS to traverse the tree
   /*Sample shortest path array : 
    index           prev node               cum weight              flag
    0                -1              		0               1
    1                0               		2               1
    2                0              		 5               1
    3                0              		 1               1
    4                3              		 3               1*/
   LOG_BLUE_ARG(("\n\n\t%sThe Routing Table for Router R%d : \n\t Destination\t\tCost\t\t Next Router%s\n",Color_Blue,index+1,Color_end));
   j=0;
   for(;j<nodeCount;j++)
   {
     
     //In case of source or prev index = source, there is no next router
     if(router->path[j].prev_index == -1 || router->path[j].prev_index == index)
       LOG_BLUE_ARG(("%s\t\tR%d\t\t %d\t\t --%s\n",Color_Blue, j+1, router->path[j].weight_cumulative,Color_end));
     else
     {
       //We need to find the router next to source which leads to the this vertex
       int nextrouter = -1, safe=0;;
       while(router->path[j].prev_index != index && safe < nodeCount)
       {
	 nextrouter = router->path[j].prev_index;
	 safe++;
       }
       LOG_BLUE_ARG(("%s\t\tR%d\t\t %d\t\t %d%s\n",Color_Blue, j+1, router->path[j].weight_cumulative, nextrouter+1,Color_end));
     }
   }
}

/*
 * Function to recursively print the route
 */
void PrintShortestPath_Rec(struct node * srcRouter,  int pnode_index, int pathlength)
{
    //Get the node at index pnode_index
  int prev_index = srcRouter->path[pnode_index].prev_index;
  
   if(prev_index != srcRouter->node_index && pathlength > 0)
  {
    //If there is another router in the path to the source, go to it
    PrintShortestPath_Rec(srcRouter,  prev_index,  pathlength -1);
  }
  //If there is no other router in the path to the source, print
  if(pathlength != nodeCount - 1)
    LOG_BLUE_ARG(("%sR%d ->%s",Color_Blue,prev_index + 1, Color_end));
  else
    LOG_BLUE_ARG(("%sR%d%s ",Color_Blue,prev_index + 1, Color_end));
}

void PrintShortestPath(int src, int dst)
{
  //Get the node at index src
  struct node * srcRouter = &all_nodes[src];



  //Check if the routing table has been generated, if not generate it
  if(!(srcRouter->path[src].prev_index == -1 && srcRouter->path[src].flag_inpath == 1))
    GenerateRoutingTable(src);
  
  //traverse the tree and print the path to the dst node
   
  //We need to find all the routers in the path destination to the source

  int nextrouter = dst, safe=0;;
  LOG_BLUE_ARG(("\n\n\t%sThe shortest path from %d to %d is :%s",Color_Blue, src+1, dst+1, Color_end));
  /*while(nextrouter != src && safe < nodeCount)
  {
    printf("R%d ->",nextrouter+1);
    nextrouter = srcRouter->path[nextrouter].prev_index;
    safe++;
  }
  if(nextrouter == src )
  */
  PrintShortestPath_Rec(srcRouter, dst, nodeCount - 1);
  LOG_BLUE_ARG(("%s->R%d \t,the total cost (value) :%d %s",Color_Blue,dst+1,srcRouter->path[dst].weight_cumulative, Color_end));
 

}

/*
 * Function to disply the entrered string in red
 */
void cScanf(char * inputString, int maxlength )
{
  int i =0;
  while((inputString[i] = getch())!='\n' && i < maxlength-1)
  {
    LOG_RED(inputString[i]);
    i++;
  }
  inputString[i]='\0';
}

int main()
{
   //Program prompts in blue. User input in red.
   char input_buffer[30];

  //Display the Menu screen
   char menu_input = 'a', routerIp[3], srcIp[3],destIp[3],menu_input_str[2];
  while(menu_input!='4')
  {
     LOG_BLUE("\n\n\n\t 1-Load File\n\t 2-Build Routing Table for each Router \n\t 3-Out Optimal Path and Minimum Cost\n\t 4-Exit Program\n\n\t ");
     //scanf(" %c",&menu_input);
     //menu_input = getch();
     cScanf(menu_input_str,sizeof(menu_input_str));
     menu_input = menu_input_str[0];
    // LOG_RED(menu_input);
  
     

      switch(menu_input)
      {
	case '1' :
	  //Get the file 
	  LOG_BLUE("\n\t Please load original routing table data file : ");
	  //scanf("%s", filename);
	 cScanf(filename,sizeof(filename));
	 
	  DEBUG_PRINT(("File %s \n", filename));
	  if(InitializeNodes()<0)
	  {
	    printf("\nFile %s could not be loaded. Please check location and try again\n", filename);
	  }
	  break;
	case '2' :
	  LOG_BLUE("\n\t Please select a router : ");
          //scanf(" %c",&routerIp);
	  cScanf(routerIp,sizeof(routerIp));
	  int routerNumber = atoi(routerIp);
          if(routerNumber >= 1 && routerNumber <= nodeCount)
	  {
	      GenerateRoutingTable(routerNumber-1);
	  }
	  else
	  {
	    //printf("%d\n",routerNumber);
	    LOG_BLUE("\n\tWrong router index entered. Try again : ");
	  }
	  break;
	case '3' :
	  LOG_BLUE("\n\t Please select a source : ");
          //scanf(" %c",&srcIp);
	  cScanf(srcIp,sizeof(srcIp));
	  LOG_BLUE("\n\t Please select a destination : ");
          //scanf(" %c",&destIp);
	  cScanf(destIp,sizeof(destIp));
	  int srcrouterNumber = atoi(srcIp);
	  int dstrouterNumber = atoi(destIp);
          if(srcrouterNumber >= 1 && srcrouterNumber <= nodeCount && dstrouterNumber >= 1 && dstrouterNumber <= nodeCount)
	  {
	    PrintShortestPath(srcrouterNumber-1 , dstrouterNumber-1);
	  }
	  else
	  {
	    LOG_BLUE("\tWrong source or destination key entered. Try again : ");
	  }
	  //Print the best path between the two routers
	  break;
	case '4' :
	  return(0);
	default :  LOG_BLUE("\tWrong menu key entered. Try again : ");
      }

    fflush(stdin);
    
  }
}


/*
 * 
1) Fix network.txt hardcoding : File name is currently hardcoded  -- done
2) Fix reversed printing of shortest path                         -- done
3) Fix colured input from user                                    -- done
4) Find the max node count which will be tested                   -- done
5) Additional tests : Test on nodes of size 10 20 and 30
6) Documentation
7) Change array implementation to linked list implementation(only if time available)
8) Fix node counting while loading files                          -- done
9) Display original routing table after loading                   -- done

Use the below code in unix/ubuntu/linux to run the code.
gcc LinkSimulation.c -o linkSim
/lib/ld-linux.so.2 ./linkSim 

*/





