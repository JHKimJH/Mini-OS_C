#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#define MAX_NAME 20
#define MAX_DIR 64
#define MAX_ARGV 256
#define MAX_CMD 64
#define MAX_MKDIR_PATH 10

typedef struct rtime{
    int year,month,day,hour,min,sec;
}rtime;

typedef struct DirectoryLCRSTreeNodeType{
    char name[20];
    char path[20];
    int mode;
    char type;         // d : directory , f : file
    rtime recent_time;
    int uid;
    int gid;
    int visible;
    int size;
    char content[MAX_ARGV];
    char permission[9];
    struct DirectoryLCRSTreeNodeType* Parent;
    struct DirectoryLCRSTreeNodeType* LChild;
    struct DirectoryLCRSTreeNodeType* RSibling;
}DirNode;


typedef struct DirectoryLCRSTreeType{
    char headerNodeName[20];
    char moveNodename[20];
    DirNode* headerNode;
    DirNode* root;
    DirNode* currentNode;
}DirTree;

typedef struct UserNode{
    char name[MAX_NAME];
    char dir[MAX_DIR];
    DirNode* user_root;
    int uid;
    int gid;
    rtime recent_time;
    struct UserNode* LinkNode;
}UNode;

typedef struct UserList{
    int uid;
    int gid;
    UNode* head;
    UNode* tail;
    UNode* current;
}UList;


typedef struct snode{
    DirNode *data;
    struct snode *link;
}snode;

typedef struct stackType{
    snode* top;
    int num;
}stacktype;




/*mutex 변수*/
int max_thread = 0;
pthread_mutex_t mutex;
pthread_mutexattr_t mutex_attr;
typedef struct thread_info{
    DirNode *t;
    UNode *user;
    char *dname;
    int pathnum;
    int p;
    int mode;
}thrinfo;
/*mutex 변수*/

DirTree* dtree;
UList* ulist;
char* stack[20];
int top = -1;
int bottom = 0;

DirTree* CreateRootDirectory();
void *thrfunc(void* arg);
void mkdir_(char *aname, int mode,UNode *user);
void mkdir_p(char *aname, int mode, UNode *user);
int chkmode(char c[]);
int make_dir(DirNode *t, UNode *user, char* str);
int change_directory (DirNode *t, char dname[]);
void preorder(DirNode* node);
void preorderprint();
int search(DirNode *t, char *dname);
rtime update_time();
void mode_to_permission(DirNode *node,int mode);
int check_permission(DirNode *node, UNode *user,char access_permission);
DirNode* apath(DirNode* mynode);
DirNode *lastpath(DirNode *nodetmp);
DirNode* exist(char* filename, char type);
void remove_(char* name);
void remove_f(char* name);
DirNode* a2path(DirNode* mynode);
void option(DirNode* mynode,char* str, char type, int dot);
void _remove_(char* name);


stacktype make_stack()
{
    stacktype stack;
    stack.top=NULL;
    stack.num=0;
    return stack;
}

void push(stacktype *stack,DirNode *item)
{
    snode *newp;
    newp =(snode*)malloc(sizeof(snode));
    newp->data=item; newp->link=NULL;
    newp->link=stack->top;
    stack->top=newp;
    stack->num++;
}

DirNode *pop(stacktype *stack)
{
    if(stack->top==NULL) return NULL;
    snode *delete_node=stack->top;
    DirNode *return_val=delete_node->data;
    stack->top=stack->top->link;
    stack->num--;
    free(delete_node);
    return return_val;
}

void free_stack(stacktype *stack)
{
    while(pop(stack)!=NULL){

    }
}

rtime update_time()
{
    rtime recent_time;
    time_t make_time=time(NULL);
    struct tm *t=localtime(&make_time);
    recent_time.year=t->tm_year+1900;
    recent_time.month=t->tm_mon+1;
    recent_time.day=t->tm_mday;
    recent_time.hour=t->tm_hour;
    recent_time.min=t->tm_min;
    recent_time.sec=t->tm_sec;
    return recent_time;
}


void *thrfunc2(void* arg)
{
    pthread_mutex_lock(&mutex);
    thrinfo *info1 = arg;   /// -r == 1 , -f == 2 -rf ==3 , -fr ==3
    /// -r == 1 , -f == 2 -rf ==3 , -fr ==3

//        if(info1->p == 1){ // r
//                remove_(info1->dname);  /// remove_        remove_f                 rf : remove_
//          }
//        else if((info1->p==2)||(info1->p==0)){  // r
//                remove_f(info1->dname);
//            }
//        else{
//            remove_(info1->dname);
//        }
    
        if(info1->p == 0)
            _remove_(info1->dname);
        else if(info1->p == 1 || info1->p ==3)
            remove_(info1->dname);
        else
            remove_f(info1->dname);


        pthread_mutex_unlock(&mutex);
        sleep(0.5);
        max_thread = 0;
    return NULL;
}


DirTree* CreateRootDirectory() {
   dtree = (DirTree*)malloc(sizeof(DirTree));
    DirNode *newnode = (DirNode*) malloc (sizeof (DirNode));
   if(dtree != NULL ){
      dtree->root=NULL;
      strncpy(dtree->headerNodeName,"/",20);
      newnode->mode = 755;
    mode_to_permission(newnode, 755);
    newnode->type = 'd';
    newnode->size = 4096;
    newnode->recent_time = update_time();
    strcpy(newnode->name, "/");
    newnode->LChild = newnode->RSibling = NULL;
    // 최상위 디렉토리 "/" 생성을 위해서 실행.
      strcpy(newnode->path,"/");
        newnode->uid = newnode->gid = 0;
        newnode->Parent = newnode;
        dtree->headerNode = dtree->root = dtree->currentNode = newnode;

   }
   return dtree;
}

UList *user_reset_ulist()
{
    ulist = (UList*)malloc(sizeof(UList));

    ulist->gid=ulist->uid=0;

    ulist->head = ulist->current = NULL;

    return ulist;
}


int pwd(DirNode *node)
{
    DirNode *t;
    t =  dtree->headerNode;
    stacktype stack=make_stack();

    if(t->Parent==t){
        printf("/");
        return 1;
    }

    while(t!=dtree->root){
        push(&stack, t);
        t=t->Parent;
    } // 1.root가 아니고 user인 경우 2.stack.top->link->name==home && stack.link->link->name==user_name인 경우? ~로 대체 가능


    push(&stack,t); // It's means push(root)
    while((t=pop(&stack))!=NULL){
        if(t==dtree->root) printf("/");
        else if(t==dtree->headerNode && t->Parent ==dtree->headerNode->Parent) printf("%s",t->name);
    else printf("%s/",t->name);
    }

    free_stack(&stack);
    return 1;
}

char *getpath(DirNode *node)
{
    DirNode *t = (DirNode*)malloc(sizeof(DirNode));
    t = node;

    char *p = malloc(sizeof(char) * 64);


    stacktype stack=make_stack();
    if(t->Parent==t){
        strcpy(p, "/");
        return p;
    }

    while(t!=dtree->root){
        t=t->Parent;
        push(&stack,t);
    } // 1.root가 아니고 user인 경우 2.stack.top->link->name==home && stack.link->link->name==user_name인 경우? ~로 대체 가능


    push(&stack,t); // It's means push(root)
    while((t=pop(&stack))!=NULL){

        if(t==dtree->root){
                strcpy(p, "/");
        }
        else {
                strcat(p,t->name);
                strcat(p, "/");
        }

    }


    if(strcmp(p,"/")!=0) p[strlen(p)-1] = '\0';


    return p;
}


DirNode *finddir(char *higher, char *lower)
{
    if(!strncmp(higher,"~",1)) return dtree->root;
    else if(!strncmp(higher,".",1)) return dtree->headerNode;

    if(dtree->headerNode->LChild==NULL){
        if(lower==NULL) return dtree->headerNode;
        else {
            return NULL;}
    }

    DirNode *t=dtree->headerNode->LChild;
    while(t!=NULL&&strcmp(t->name,higher)){
        t=t->RSibling;
    }
    if(t==NULL){ //upper dir is not exist.
        if(lower==NULL) return dtree->headerNode;
        else return NULL;
    }
    else{ // higher is already exist.
        if(lower==NULL) return 0;
        else return t;
    }
}

int check_permission(DirNode *node, UNode *user,char access_permission)
{
    if(user->uid==0&&user->gid==0) return 1;
    if(access_permission=='r'){ // 접근하려는 권한이 읽기 권한인 경우(ex:cat(read only), vi(read only)...)
        if(node->uid==user->uid){ // 대상이 유저 소유라면 rwxrwxrwx 중 제일 앞의 3자리를 본다.
            if(node->permission[0]=='r') return 1;
            else return 0;
        }
        else{ // 유저 소유의 폴더가 아니므로 rwxrwxrwx 중 맨 뒤 3자리를 본다.
            if(node->permission[6]=='r') return 1;
            else return 0;
        }
    }
    else if(access_permission=='w'){ // 접근하려는 권한이 쓰기 권한인 경우(ex:mkdir, cat(read and write), chmod ...)

        if(node->uid==user->uid){ // 대상이 유저 소유라면 rwxrwxrwx 중 제일 앞의 3자리를 본다.
            if(node->permission[1]=='w') return 1;
            else return 0;
        }
        else{ // 유저 소유의 폴더가 아니므로 rwxrwxrwx 중 맨 뒤 3자리를 본다.
            if(node->permission[7]=='w') return 1;
            else return 0;
        }
    }
    else if(access_permission=='x'){ // 접근하려는 권한이 접근 권한인 경우(ex:ls, cd)
        if(node->uid==user->uid){ // 대상이 유저 소유라면 rwxrwxrwx 중 제일 앞의 3자리를 본다.
            if(node->permission[2]=='x') return 1;
            else return 0;
        }
        else{ // 유저 소유의 폴더가 아니므로 rwxrwxrwx 중 맨 뒤 3자리를 본다.
            if(node->permission[8]=='x') return 1;
            else return 0;
        }
    }
    else return 0;
}

DirNode* check_directory(DirNode *t, char *dname, UNode *user,int mode)
{
    DirNode *curtemp=t;
    char *str, *next_str;
    if(!strncmp(dname,"/",1)) curtemp=dtree->root;


    str=strtok(dname,"/");
    next_str=strtok(NULL,"/");


    while(next_str!=NULL){
        curtemp=finddir(str,next_str);

    if(curtemp==NULL){
            printf("ERROR: cannot create directory \'%s\': No such file or directory\n",str);
            return NULL;
        }
        else if(curtemp==0){
            printf("ERROR: cannot create directory \'%s\': File or directory exists.\n",str);
            return NULL;
        }
        str=next_str;
        next_str=strtok(NULL,"/");
    }

    if(!check_permission(curtemp,user,'w')){
        printf("mkdir: cannot open directory \'%s\': Permission denied\n",curtemp->name);
        return NULL;
    }

    if(next_str==NULL){
        mkdir_(str, mode, user);
    } // str=='TT', curtemp=test2
    else {
        mkdir_(next_str, mode, user);
    }
    return curtemp;
}

void mkdir_p(char dname[], int mode, UNode *user)
{
    DirNode *curtemp=dtree->headerNode;
    if(!strncmp(dname,"/",1)) curtemp=dtree->root;
    char *str=strtok(dname,"/");
    while(str!=NULL){
        if(exist(str,'d')==NULL)
        {
            mkdir_(str, mode, user);
            change_directory(curtemp,str);
        }
    else
    {
        change_directory(curtemp,str);
    }
        str=strtok(NULL,"/");
    }
    change_directory(dtree->root, curtemp->name);
}

void *thrfunc(void* arg)
{
    thrinfo *info = arg;

    pthread_mutex_lock(&mutex);
   // printf("%ld thread\n", pthread_self());


    if(info->p == 0){
            if(check_directory(info->t, info->dname, info->user, info->mode)==NULL){
                return NULL;
                }
        }
  else if(info->p==1){
                mkdir_p(info->dname, info->mode, info->user);
    }
    pthread_mutex_unlock(&mutex);
        sleep(0.5);

    return NULL;
}


void mode_to_permission(DirNode *node,int mode)
{

    int i=0,j=0,searchset[]={4,2,1},step=0,len=0;
    char mset[4],pset[]={'r','w','x'},temp[]="000\0";
    //itoa(mode,mset,10); -> 리눅스에서 안됨
    sprintf(mset, "%d", mode); //mode 의 값을 %d 의 형태로 mset 에 문자열로 넣었으니 mset 에는 i 의 값이 문자열의 형태로 변환된다. 마찬가지 방법으로 i 의 값을 16 진수나 8 진수 형태로 (%x, %o) 넣을 수 도 있다.
    //ASCII 코드로 전환
    len=strlen(mset); //자릿수

    if(len==1){
        temp[2]=mset[0];
        strcpy(mset,temp);
    }
    else if(len==2){
        temp[1]=mset[0]; temp[2]=mset[1];
        strcpy(mset,temp);
    }
    for(i=0;i<3;i++){
        int mode_i=mset[i]-48;  //ASCII코드(0이 48) 따라서 755면 55-48 53-48 53-48
        for(j=0;j<3;j++,step++){
            if(mode_i<searchset[j]){
                node->permission[step]='-';
            }
            else{
                mode_i%=searchset[j];
                node->permission[step]=pset[j];
            }
        }
    }
    node->permission[step]='\0';
}

void mkdir_(char *aname,int mode, UNode *user)
{
    DirNode* newnode, *temp;
    newnode = (DirNode*) malloc (sizeof (DirNode));
    newnode->type = 'd';

    newnode->visible = 1;

    newnode->mode = mode;
    mode_to_permission(newnode, mode);

    newnode->uid = user->uid;
    newnode->gid = user->gid;

    newnode->recent_time = update_time();
    strcpy(newnode->name, aname);
    newnode->LChild = newnode->RSibling = NULL;
    newnode->size = 4096;

    if(exist(aname, 'd') != NULL || exist(aname,'f') !=NULL )
    {
        printf("ERROR: cannot create file \'%s\': File or directory exists.\n",aname);
        return;
    }


    if(dtree->root == NULL){
        strcpy(newnode->path,"/");
        newnode->uid = newnode->gid = 0;
        newnode->Parent = newnode;
        dtree->headerNode = dtree->root = dtree->currentNode = newnode;
    }


        if (dtree->headerNode->LChild == NULL) //headerNode : current folder
        {

            newnode->Parent = dtree->headerNode;
            strcpy(newnode->path, getpath(newnode));
            dtree->headerNode->LChild = newnode;
        }
        else
        {
            newnode->Parent = dtree->headerNode;
            strcpy(newnode->path, getpath(newnode));
            temp = dtree->headerNode->LChild;
            while (temp->RSibling != NULL)
                temp = temp->RSibling;
            temp->RSibling = newnode;
        }  //rsibling of headerNode


}

int chkmode(char c[])
{
    int i=0;
    while(c[i]!='\0'){
        if(c[i]<48||c[i]>55) return 0; // 48:0, 55:7
        i++;
    }
    return 1; // all is digit!
}

int make_dir(DirNode *t, UNode *user, char *str)
{
    int pathnum =0;
    char *path_list[MAX_MKDIR_PATH];
    int status;
    int i, poption=0, moption = 0;
    int mode = -1;

    if(str == NULL){
        printf("ERROR: Syntax Error\n");
    }
    while(str!=NULL){
        if ((*str)=='-'){
            i=0;//-m --p option processing
            while(*(str+i)=='-'){
                i++;
            } // str[i] == option character

            if(*(str+i)=='\0'){
                printf("ERROR: invalid option -- \'%c\'\n",str[i]);
                return -1;
            }

            while(*(str+i)!='\0'){
                if(*(str+i)=='m')
                {
                    moption=1;
                }
                // option m is on (Need a [mode] such as 700)
                else if(*(str+i)=='p')
                    poption=1; // option p is on
                else{
                    printf("ERROR: invalid option -- \'%c\'\n",str[i]);
                    return -1;
                }
                i++;
            }
        }
        else if(moption==1&&mode==-1){
            if(strlen(str)>3||!chkmode(str)){
                printf("mkdir: invalid mode -- \'%s\'\n",str);
                return -1;
            }
            mode=atoi(str);
        }
        else{
            if(pathnum == MAX_MKDIR_PATH)
            {
            printf("mkdir: too many inputs.\n");
                return -1;
            }
            path_list[pathnum]=(char*)malloc(sizeof(char)*strlen(str));
            strcpy(path_list[pathnum], str);
            pathnum++;
            // path processing
        }
        str=strtok(NULL," ");
    }
    if(pathnum == 0){
        printf("ERROR: Syntax Error\n");
    }
    if(moption==1){
        if(mode==-1){
            printf("ERROR: option requires an argument -- 'm'\n");
            return -1;
        }
    }
    if(moption<=0) mode=755;



    pthread_t tid[pathnum];

    thrinfo *info[pathnum];



    for(int i=0; i<pathnum;i++){


    info[i] = (thrinfo*)malloc(sizeof(thrinfo));
            info[i]->t = t;
            info[i]->user = user;

            info[i]->pathnum = pathnum;
            info[i]->p = poption;
            info[i]->mode = mode;
            info[i]->dname = path_list[i];

        if((status=pthread_create(&tid[i],NULL,&thrfunc,info[i]))!=0){
            printf("pthread_create fail: %s", strerror(status));
            exit(0);
        }
    }

    for(i=0; i<pathnum;i++) pthread_join(tid[i],NULL);
    status = pthread_mutex_destroy(&mutex);

    for(i=0;i<pathnum;i++)
        free(path_list[i]);

    return -1;
}
//}

int change_directory (DirNode *t, char dname[])
// this function finds the node by the given name and make that node as current node
{
    int found;
    if (t != NULL)
    {
        if (t->type=='d')
        {
            if (strcmp(t->name, dname) == 0) //if dname == t->name
            {
                dtree->headerNode = t;
                return found = 1;
            }
            else  // if dname != t->name
            {
                found = change_directory(t->LChild, dname);
                if (!found)
                    return change_directory( t->RSibling, dname);
                else
                return found;
            }
        }
        else  // if t->id = 'f'
            return change_directory(t->RSibling, dname);
    }
    else
    {// if t == NULL
        return 0;
    }
}

int cd(DirNode *t, char *dname)
{
    int pathnum =0;
    char *str, *next_str;
    char *path_list[MAX_MKDIR_PATH];


    if(dname == NULL){
        printf("ERROR: Syntax Error\n");
    }
    if(strcmp(dname,"..")==0)
    {
        if(t != dtree->root)
        {
            change_directory(dtree->root, dtree->headerNode->Parent->name);
            return 0;
        }
        else{
            printf("ERROR: Syntax Error\n");
            return 0;
        }
    }
    else if(strcmp(dname, ".")==0)
    {
        return 0;
    }
    else if(strcmp(dname, "/")==0){
        dtree->headerNode = dtree->root;
        return 0;
    }


    str = dname;
    str = strtok(str, "/");
    if(str==NULL){
        str = strtok(NULL, "/");
    }
    next_str = strtok(NULL,"/");
    path_list[pathnum]=(char*)malloc(sizeof(char)*strlen(str));
    strcpy(path_list[pathnum], str);
    pathnum++;

    while(next_str!=NULL){
            str = next_str;
        next_str = strtok(NULL, "/");
        path_list[pathnum]=(char*)malloc(sizeof(char)*strlen(str));
            strcpy(path_list[pathnum], str);
        pathnum++;
    }

    if(pathnum == 1 && exist(path_list[0], 'd')!= NULL){
        change_directory(dtree->headerNode->LChild, path_list[0]);
    return 0;
    }
    

    for(int i = 0; i<pathnum;i++){
     if(exist(path_list[i], 'd')== NULL){
            printf("ERROR: \'%s\': No such file or directory\n",str);
        return 0;
        }
        else{
            change_directory(dtree->root, path_list[i]);
        }
    }
}


void free_dir(DirNode *node)
{
    if (node != NULL)
    {
        free_dir(node->LChild);
        free_dir(node->RSibling);
        free(node);
    }
}

void remove_(char* name) {
    DirNode *temp = dtree->headerNode->LChild;
    DirNode *prev;

    if(temp!=NULL) {
        if(strcmp(temp->name, name) == 0){
            dtree->headerNode->LChild = dtree->headerNode->LChild->RSibling;
            free_dir(temp->LChild);
            free(temp);
        }
        else
        {
            while ((temp != NULL) && (strcmp(temp->name, name) != 0))
            {
                prev = temp;
                temp = temp->RSibling;
            }

            if (temp == NULL)
                printf("ERROR: No such file or directory \'%s\'\n", name);
            else
            {
                prev->RSibling = temp->RSibling;
                free_dir(temp->LChild);
                free(temp);
            }
        }
    }
}

void remove_f(char* name) {
    DirNode *temp = dtree->headerNode->LChild;
    DirNode *prev;

    if(temp!=NULL) {
        if(strcmp(temp->name, name) == 0){
            if(temp->LChild!=NULL) printf("ERROR: Cannot Remove file or directory \'%s\'\n", name);
            else
            {
                if(temp->type== 'f')
                {
                dtree->headerNode->LChild = dtree->headerNode->LChild->RSibling;
                free(temp);
                }
                else
                 {
                printf("rm: cannot remove : Is a directory\n");
                 }
            }
        }
        else
        {
            while ((temp != NULL) && (strcmp(temp->name, name) != 0))
            {
                prev = temp;
                temp = temp->RSibling;
            }

            if (temp == NULL)
                printf("ERROR: File or directory does not exist \'%s\'\n", name);
            else
            {
                if(temp->LChild!=NULL){
                        printf("ERROR: Cannot Remove file or directory \'%s\'\n", name);}
                else {
                if(temp->type== 'f')
                {
                prev->RSibling = temp->RSibling;
                free(temp);}
                else
                 {
                 printf("rm: cannot remove : Is a directory\n");
                 }
                }
            }
        }
    }
}

void _remove_(char* name) {
    DirNode *temp = dtree->headerNode->LChild;
    DirNode *prev;

    if(temp!=NULL) {
        if(strcmp(temp->name, name) == 0){
            if(temp->type == 'f')
            {
                dtree->headerNode->LChild = dtree->headerNode->LChild->RSibling;
                free_dir(temp->LChild);
                free(temp);
            }
            else
            {
                printf("rm: cannot remove : Is a directory\n");
            }
        }
        else
        {
            while ((temp != NULL) && (strcmp(temp->name, name) != 0))
            {
                prev = temp;
                temp = temp->RSibling;
            }

            if (temp == NULL)
                printf("ERROR: No such file or directory \'%s\'\n", name);
            else
            {
                if(temp->type == 'f')
                {
                    prev->RSibling = temp->RSibling;
                    free_dir(temp->LChild);
                    free(temp);
                }
                else
                {
                    printf("rm: cannot remove : Is a directory\n");

                }
            }
        }
    }
}

int rm(DirNode *t, UNode *user, char *str)
{
    int pathnum =0;
    char *path_list[MAX_MKDIR_PATH];
    int status;
    int i, opt = 0;


    while(str!=NULL){

        if ((*str)=='-'){
            i=0;
            while(*(str+i)=='-'){
                i++;
            } // str[i] == option character

            if(*(str+i)=='\0'){
                printf("ERROR: invalid option -- \'%c\'\n",str[i]);
                return -1;
            }


            while(*(str+i)!='\0'){  // -r == 1 , -f == 2 -rf ==3 , -fr ==3
                if(*(str+i)=='r')
                {
                    opt+=1;
                }
                // option m is on (Need a [mode] such as 700)
                else if(*(str+i)=='f')
                    opt+=2; // option p is on
                else{
                    printf("ERROR: invalid option -- \'%c\'\n",str[i]);
                    return -1;
                }
                i++;
            }
        }
        else{
            if(pathnum == MAX_MKDIR_PATH)
            {
            printf("rm: too many inputs.\n");
                return -1;
            }

            path_list[pathnum]=(char*)malloc(sizeof(char)*strlen(str));
            strcpy(path_list[pathnum], str);
            pathnum++;
            // path processing
        }
        str=strtok(NULL," ");

    }



    pthread_t tid[pathnum];
    thrinfo *info1[pathnum];



    for(int i=0; i<pathnum;i++){
        info1[i] = (thrinfo*)malloc(sizeof(thrinfo));
            info1[i]->t = t;
            info1[i]->user = user;
            info1[i]->p = opt;
            info1[i]->pathnum = pathnum;
            info1[i]->dname=path_list[i];

        if((status=pthread_create(&tid[i],NULL,&thrfunc2,info1[i]))!=0){
            printf("pthread_create fail: %s", strerror(status));
            exit(0);
        }
    }

    for(i=0; i<pathnum;i++) pthread_join(tid[i],NULL);
    status = pthread_mutex_destroy(&mutex);

    for(i=0;i<pathnum;i++)
        free(path_list[i]);

    return -1;
}


DirNode* exist(char* filename, char type) // 현재 디렉토리의 같은 이름을 가진 하위 디렉토리,파일이 있는지 확인
{

    DirNode* tmpnode = dtree->headerNode->LChild;

    if(type!='a'){
        while (tmpnode != NULL)
        {
            if (strcmp(tmpnode->name, filename) == 0 && tmpnode->type == type)
            {

                return tmpnode;
            }

            tmpnode = tmpnode->RSibling;
        }
    }
    else{
        while (tmpnode != NULL)
        {
            if (strcmp(tmpnode->name, filename) == 0)
            {
                return tmpnode;
            }

            tmpnode = tmpnode->RSibling;
        }
    }

    return NULL;
}

DirNode* makefile( char* filename)
{
    DirNode* newfile = (DirNode*)malloc(sizeof(DirNode));
    DirNode* tmpnode = dtree->headerNode->LChild;
    strcpy(newfile->name, filename);
    newfile->uid = ulist->current->uid;
    newfile->gid = ulist->current->gid;
    newfile->recent_time = update_time();
    newfile->content[0] = '\0';
    newfile->size = 0;


    newfile->type = 'f';
    newfile->mode = 755;
    mode_to_permission(newfile, newfile->mode);


    newfile->LChild = NULL;

    if (tmpnode == NULL) // 현재디렉토리에 아무것도 없을 때
    {
        newfile->Parent = dtree->headerNode;
        newfile->RSibling = NULL;
        dtree->headerNode->LChild = newfile;
        strcpy(newfile->path,getpath(newfile));

        return newfile;
    }
    else
    {
        newfile->Parent = dtree->headerNode;

        newfile->RSibling = NULL;

        while (tmpnode->RSibling != NULL)
        {
            tmpnode = tmpnode->RSibling;
        }

        tmpnode->RSibling = newfile;

        strcpy(newfile->path,getpath(newfile));

        return newfile;
    }

}

void cat(char *str)
{
    char* curstr;
    DirNode* tmpnode;
    char input[MAX_ARGV];
    int i = 0;
    int key = 0;
    int q = 1;
    char *cpy;

    curstr = strtok(NULL, " ");

    if(str == NULL)
    {
        printf("잘못된 명령어 입니다\n");
        return ;
    }

    if (strcmp(str, ">") == 0) // cat > 파일 생성후 내용 입력
    {
        if (curstr == NULL)// 파일명 미입력
        {
            printf("ERROR: Syntax Error\n");

        }
        else
        {
            if (exist(curstr, 'd') != NULL)//같은 이름 dir 존재
            {
                printf("ERROR: cannot create file \'%s\': File or directory exists.\n",curstr);
            }
            else // 현재 dir에 파일 생성, 내용 입력
            {

                if ((tmpnode=exist(curstr, 'f')) == NULL) // 파일이 없으면 새로 만들어주고, 원래 있는 파일이면 그위에 덮어쓴다ㅏ
                {
                    tmpnode = makefile(curstr);
                }
                while(1)
                {
                    key = getchar();
                    if(key == EOF) break;

                    input[i] = key;
                    i++;
                }
                input[i] = 0;
                strcpy(tmpnode->content, input);

                int j=0;
                while(tmpnode->content[j]!='\0')
                    j++;
                tmpnode->size =j;
            }
        }
    }
    else if (strncmp(str, "-", 1) == 0) // cat -
    {

        if(curstr == NULL)
        {
            printf("ERROR: Syntax Error\n");
            return ;
        }

        if (strcmp(str, "-n") != 0) //
            printf("ERROR: Syntax Error\n");
        else // cat -n  줄 번호 + 내용 출력
        {
            if (exist(curstr, 'd') != NULL) // 파일이 아니라 디렉토리 일때
            {
                printf("ERROR: cannot create file \'%s\': File or directory exists.\n",curstr);
            }
            else
            {
                if ((tmpnode = exist(curstr, 'f')) == NULL) // 파일이 없으면
                    printf("ERROR: No such file or directory \'%s\'\n", curstr);
                else // 파일이 있을 때
                {
                    strcpy (input,tmpnode->content);

            cpy = strtok(input,"\n");

                    while(cpy != NULL)
                    {
                        printf("%d     %s\n",q,cpy);
                        cpy = strtok(NULL,"\n");
                        q++;
                    }

                }
            }
        }

    }
    else // cat (  )          파일 내용 출력
    {
        if (exist(str, 'd') != NULL) // 파일이 아니라 디렉토리 일때
                printf("ERROR: cannot open file \'%s\': File doesn't exists.\n",str);
        else
        {
            if ((tmpnode=exist(str, 'f')) != NULL) // 파일이 있으면 출력
            {
                printf("%s", tmpnode->content);

            }
            else
            {
                printf("ERROR: No such file or directory \'%s\'\n", str);
            }
        }
    }
    return;
}


void find(char *str)
{
    char k[100];
    char* j;
    DirNode* nodetmp;
    char* str1, *str2, *str3, *str4;

    str1 = strtok(NULL," ");
    str2 = strtok(NULL," ");
    str3 = strtok(NULL," ");
    str4 = strtok(NULL," ");


    if(str == NULL)
    {
        nodetmp=dtree->headerNode;

        printf(".\n");

        if(nodetmp->LChild == NULL)
            return;

        nodetmp = nodetmp->LChild;
        a2path(nodetmp);

        return ;
    }

    if (strncmp(str, "/", 1) != 0 && strcmp(str,".")!=0)
    {
        printf("잘못된 명령어입니다.\n");
        return;
    }

    if(strncmp(str,"/",1)==0)
    {
        strcpy(k, str);
        j = strtok(k, "/");
        stack[++top] = "/";

        while (j != NULL)
        {
            top++;
            stack[top]=j;
                j = strtok(NULL, "/");
        }

        if (strcmp(dtree->root->name, stack[bottom]) != 0)
        {
            printf("잘못된 경로입니다.\n");
            return;
        }

        if (strcmp(stack[top],dtree->root->name)!=0) //
        {
            bottom++;
            nodetmp = dtree->root->LChild;
            nodetmp = lastpath(nodetmp);
        }
        else
            {nodetmp = dtree->root;}
    }
    else
        nodetmp = dtree ->headerNode;

    top = -1;
    bottom = 0;

    if (nodetmp == NULL)
    {
        printf("find: '/%s': 그런 파일이나 디렉토리가 없습니다\n",str);
        return;
    }

    if (str1 == NULL) // find /~
    {

        if(strncmp(str,"/",1)==0)
        {
            printf("%s", str);
            printf("\n");

            if (nodetmp->LChild == NULL) // 디렉토리에 아무것도 없을 경우 자기 자신만 출력하고 종료
                return;

            nodetmp = nodetmp->LChild;
            apath(nodetmp);
        }

        else
        {
            printf(".\n");

            if(nodetmp->LChild == NULL)
                return;

            nodetmp = nodetmp->LChild;
            a2path(nodetmp);

        }

        top == -1;
        bottom = 0;
    }
    else if (strcmp(str1, "-name") == 0) // find /~ -name
    {
        if (str2 == NULL || strncmp(str2, "\"", 1) != 0)
        {
            printf("잘못된 명령어 입니다\n");
            return;
        }

        if (str3 == NULL) // find /~ -name " "   " " 의 조건에 맞는 파일, 디렉토리 출력
        {
            if(strcmp(str,".")==0)
                option(nodetmp,str2,'a', 1);
            else
                option(nodetmp,str2,'a', 0);
        }
        else if (strcmp(str3, "-type") == 0)
        {
            if(str4 == NULL)
            {
                printf("find: '%s'에 인수가 빠졌습니다.\n",str3);
                return ;
            }

            if (strcmp(str4, "d") == 0) // " " 이름을 갖는 디렉토리만 출력
            {
                if(strcmp(str,".")==0)
                    option(nodetmp,str2,'d', 1);
                else
                    option(nodetmp,str2 , 'd',0);
            }
            else if (strcmp(str4, "f") == 0)// 파일만 출력
            {
                if(strcmp(str,".")==0)
                    option(nodetmp,str2,'f', 1);
                else
                    option(nodetmp,str2 , 'f',0);
            }
            else
            {
                printf("find: Unknown argument to -type: %s \n",str4);
            }
        }
        else
        {
            printf("find: unknown predicate '%s' \n",str3);
        }
    }
    else
    {
        printf("find: unknown predicate '%s' \n",str1);
    }

    return ;
}

DirNode* apath(DirNode* mynode)
{
    DirNode* tmpnode = mynode;

    while (strcmp(tmpnode->name,"/")!=0)
    {
        top++;
        stack[top] = tmpnode->name;
        tmpnode = tmpnode->Parent;
    }

    tmpnode = mynode; // tmpnode를 mynode로 다시 초기화

    while (top != -1)
    {
        printf("/%s", stack[top]);
        top--;
    }
    printf("\n");

    if (mynode->RSibling != NULL) //파일,디렉토리가 더 있으면
    {
        tmpnode = mynode->RSibling;
        apath(tmpnode);
        tmpnode = mynode;
    }

    if (mynode->LChild != NULL) // 현재 디렉토리에서 파일,디렉토리를 가지고 있을 때
    {
        tmpnode = mynode->LChild;
        apath(tmpnode);
        tmpnode = mynode;
    }

    return mynode;
}

DirNode* a2path(DirNode* mynode)
{
    DirNode* tmpnode = mynode;

    while (tmpnode != dtree->headerNode)
    {
        top++;
        stack[top] = tmpnode->name;
        tmpnode = tmpnode->Parent;
    }

    tmpnode = mynode; // tmpnode를 mynode로 다시 초기화
    printf(".");
    while (top != -1)
    {
        printf("/%s", stack[top]);
        top--;
    }
    printf("\n");

    if (mynode->RSibling != NULL) //파일,디렉토리가 더 있으면
    {
        tmpnode = mynode->RSibling;
        a2path(tmpnode);
        tmpnode = mynode;
    }

    if (mynode->LChild != NULL) // 현재 디렉토리에서 파일,디렉토리를 가지고 있을 때
    {
        tmpnode = mynode->LChild;
        a2path(tmpnode);
        tmpnode = mynode;
    }

    return mynode;
}

DirNode* optionprint(DirNode* mynode,char type,char*name,int option)
{
    int a=0;
    DirNode* tmpnode = mynode;
    int b = strlen(tmpnode->name);
    int c = strlen(name);


    if (option == 1 || option == 11)
    {
        if ((tmpnode->type == type) && (strcmp(tmpnode->name, name) == 0) || (strcmp(tmpnode->name, name) == 0) && (type == 'a'))
            a = 1;
    }
    else if (option == 2  || option == 12 )
    {
        if ((tmpnode->type == type && strncmp(tmpnode->name, name,c) == 0) || (strncmp(tmpnode->name, name,c) == 0 && type == 'a'))
            a = 1 ;
    }
    else if (option == 3  || option == 13 )
    {
        if ((tmpnode->type == type && strncmp(tmpnode->name + b -c, name, c) == 0) || (strncmp(tmpnode->name + b - c, name, c) == 0 && type == 'a'))
            a = 1;
    }
    else if (option == 4  || option == 14 )
    {
        if ((tmpnode->type == type && strstr(tmpnode->name,name) !=NULL) || (strstr(tmpnode->name, name) != NULL && type == 'a'))
            a = 1;
    }
    else
        a = 0;


    if (a == 1)
    {
        if (option > 10)
        {
            while (tmpnode != dtree->headerNode)
            {
                top++;
                stack[top] = tmpnode->name;
                tmpnode = tmpnode->Parent;
            }
            printf(".");
        }
        else
        {
            while (strcmp(tmpnode->name, "/") != 0)
            {
                top++;
                stack[top] = tmpnode->name;
                tmpnode = tmpnode->Parent;
            }

        }

        while (top != -1)
        {
            printf("/%s", stack[top]);
            top--;
        }
        printf("\n");
    }

    tmpnode = mynode; // tmpnode를 mynode로 다시 초기화

    if (mynode->RSibling != NULL)
    {
        tmpnode = mynode->RSibling;
        optionprint(tmpnode,type,name,option);
        tmpnode = mynode;
    }

    if (mynode->LChild != NULL)
    {
        tmpnode = mynode->LChild;
        optionprint(tmpnode,type,name,option);
        tmpnode = mynode;
    }

    return mynode;
}

DirNode *lastpath(DirNode *nodetmp)
{
    if (nodetmp == NULL && strcmp(nodetmp->Parent->name, "/") != 0)
    {
        return NULL;
    }

    while (strcmp(stack[bottom], nodetmp->name) != 0 || nodetmp->type != 'd')//이름이같고 디렉토리일때까지
    {
        if (nodetmp->RSibling == NULL) // 못찾았을때
        {
            return NULL;
        }
        else
            nodetmp = nodetmp->RSibling;
    }

    if (top == bottom)//마지막 경로이면
    {
        return nodetmp;
    }
    else // 더 아래 디렉토리로 가야하면
    {
        bottom++;
        nodetmp = nodetmp->LChild;
        return lastpath(nodetmp);
    }

}

void option(DirNode* mynode,char* str, char type, int dot)
{
    DirNode* nodetmp = mynode;
    char a[20];
    char* b;
    int len;
    int option1 =0 ;
    strcpy(a, str);

    if(dot == 1)
        option1 = option1+10;

    b = strtok(a, "\"");
    len = strlen(b);

    if (strstr(b, "*") == NULL) // "   " 이름과 똑같은 것만 출력
    {
        option1 = option1 + 1;
        if (nodetmp->LChild != NULL);
        { optionprint(nodetmp->LChild, type, b, option1); }
    }
    else
    {
        if (strncmp(b, "*", 1) == 0) // "*    " or  "*   *"
        {
            if (strncmp(b + len - 1, "*", 1) == 0)  //"*    *" 포함하는 파일 디렉토리 출력
            {
                option1 = option1 + 4;
                b = strtok(b, "*"); // b = 파일, 디렉토리 이름
                if (nodetmp->LChild != NULL);
                {   optionprint(nodetmp->LChild, type, b, option1); }
            }
            else // "*    " 로 끝나는 것 출력
            {
                option1 = option1 + 3;
                b = b + 1;
                if (nodetmp->LChild != NULL);
                { optionprint(nodetmp->LChild, type, b, option1); }
            }
        }
        else if (strncmp(b + len - 1, "*", 1) == 0)// "     *" 로 시작하는 것만 출력
        {
            option1 = option1 + 2;
            b = strtok(b, "*");
            if (nodetmp->LChild != NULL);
            { optionprint(nodetmp->LChild, type, b, option1); }
        }
        else
        {
            printf("Error \n");
            return;
        }
    }

    return;
}



void my_cp(DirTree *dtree,char *filename, char *ForDname)
{
    DirNode *currentdir=dtree->headerNode;
    DirNode *copyfile;
    DirNode *tempfile;
    DirNode *searchfile;



    if(filename == NULL || ForDname==NULL){
        printf("ERROR: Syntax Error\n");
        return;
    }


    if(strchr(ForDname,'/')==NULL)
    {
        if(exist(ForDname, 'd')==NULL)
        {
            copyfile=exist(filename, 'a');
            if(copyfile==NULL)
            {
                printf("ERROR:That file not exist\n");
                return;
            }
            tempfile=makefile(ForDname);
            strcpy(tempfile->content, copyfile->content);
            int j=0;
            while(tempfile->content[j]!='\0')
                j++;
            tempfile->size =j;

            return;
        }
        else
        {
            change_directory(dtree->root,ForDname);
            tempfile=dtree->headerNode;
            if (tempfile == NULL)
                {
                    tempfile = makefile(filename);
                    strcpy(tempfile->content, copyfile->content);
                    int j=0;
                    while(tempfile->content[j]!='\0')
                        j++;
                    tempfile->size =j;
                }
            else
                {
                    if (tempfile->RSibling == NULL)
                        {
                            tempfile->RSibling = makefile(filename);
                            strcpy(tempfile->RSibling->content, copyfile->content);
                            int j=0;
                            while(tempfile->content[j]!='\0')
                                j++;
                            tempfile->size =j;
                        }
                    else
                        {
                            searchfile = tempfile->RSibling;
                            while (searchfile->RSibling != NULL)
                            {
                                searchfile = searchfile->RSibling;
                            }
                            tempfile = makefile(filename);
                            strcpy(tempfile->content, copyfile->content);
                            int j=0;
                            while(tempfile->content[j]!='\0')
                                j++;
                            tempfile->size =j;
                            searchfile->RSibling=tempfile;
                        }
                }
        }
    }
    else
    {
        char *fname, *dname;
        tempfile=dtree->headerNode->LChild;

        filename = strtok(filename,"/");
        fname = strtok(NULL,"/");
        while (fname!=NULL){
            filename = fname;
            fname = strtok(NULL,"/");
        }

        copyfile = exist(filename, 'f');
        if(copyfile==NULL)
            {
                printf("ERROR:That file not exist\n");
                return;
            }

        ForDname=strtok(ForDname,"/");
        dname = strtok(NULL,"/");
        if (search(dtree->root,ForDname)==1){
            change_directory(dtree->root,ForDname);
            }
        while (dname!=NULL){
            ForDname = dname;
            dname = strtok(NULL,"/");
            if (search(dtree->root,ForDname)==1){
            change_directory(dtree->root,ForDname);
            }
            else if(search(dtree->root,ForDname)!=1 && dname != NULL){
                printf("ERROR: No such directory \'%s\'\n",ForDname);
                return;
            }

        }

        if(search(dtree->root,ForDname)==1){
            ForDname=filename;
        }




        if (tempfile == NULL)
            {
                tempfile = makefile(ForDname);
                strcpy(tempfile->content, copyfile->content);
                int j=0;
                while(tempfile->content[j]!='\0')
                    j++;
                tempfile->size =j;
            }
        else
            {
                if (tempfile->RSibling == NULL)
                    {
                        tempfile->RSibling = makefile(ForDname);
                        strcpy(tempfile->RSibling->content, copyfile->content);
                        int j=0;
                        while(tempfile->content[j]!='\0')
                            j++;
                        tempfile->size =j;
                    }
                else
                    {
                        tempfile = makefile(ForDname);
                        strcpy(tempfile->content, copyfile->content);
                        int j=0;
                        while(tempfile->content[j]!='\0')
                            j++;
                        tempfile->size =j;
                    }
            }
        change_directory(dtree->root, currentdir->name);
    }
}



void my_mv(DirTree *dtree,char *filename, char *ForDname)
{
    DirNode *currentdir=dtree->headerNode;
    DirNode *mvfile;
    DirNode *tempfile;
    DirNode *searchfile;


    if(filename == NULL || ForDname==NULL){
        printf("ERROR: Syntax Error\n");
        return;
    }


    if(strchr(ForDname,'/')==NULL)
    {
        if(exist(ForDname, 'd')==NULL)
        {
            mvfile=exist(filename, 'a');
            if(mvfile==NULL)
            {
                printf("ERROR:That file not exist\n");
                return;
            }
            tempfile=makefile(ForDname);
            strcpy(tempfile->content, mvfile->content);
            int j=0;
            while(tempfile->content[j]!='\0')
                j++;
            tempfile->size =j;
            remove_(mvfile->name);
            return;
        }
        else
        {
            remove_(mvfile->name);
            change_directory(dtree->root,ForDname);
            tempfile=dtree->headerNode;
            if (tempfile == NULL)
                {
                    tempfile = makefile(filename);
                    strcpy(tempfile->content, mvfile->content);
                    int j=0;
                    while(tempfile->content[j]!='\0')
                        j++;
                    tempfile->size =j;
                }
            else
                {
                    if (tempfile->RSibling == NULL)
                        {
                            tempfile->RSibling = makefile(filename);
                            strcpy(tempfile->RSibling->content, mvfile->content);
                            int j=0;
                            while(tempfile->content[j]!='\0')
                                j++;
                            tempfile->size =j;
                        }
                    else
                        {
                            searchfile = tempfile->RSibling;
                            while (searchfile->RSibling != NULL)
                            {
                                searchfile = searchfile->RSibling;
                            }
                            tempfile = makefile(filename);
                            strcpy(tempfile->content, mvfile->content);
                            int j=0;
                            while(tempfile->content[j]!='\0')
                                j++;
                            tempfile->size =j;
                            searchfile->RSibling=tempfile;
                        }
                }
        }
    }
    else
    {
        char *fname, *dname;
        tempfile=dtree->headerNode->LChild;

        filename = strtok(filename,"/");
        fname = strtok(NULL,"/");
        while (fname!=NULL){
            filename = fname;
            fname = strtok(NULL,"/");
        }

        mvfile = exist(filename, 'f');
        remove_(mvfile->name);
        if(mvfile==NULL)
            {
                printf("ERROR:That file not exist\n");
                return;
            }

        ForDname=strtok(ForDname,"/");

        dname = strtok(NULL,"/");
        if (search(dtree->root,ForDname)==1){
            change_directory(dtree->root,ForDname);
            }
    while (dname!=NULL){
            ForDname = dname;
            dname = strtok(NULL,"/");
            if (search(dtree->root,ForDname)==1){
            change_directory(dtree->root,ForDname);
            }
            else if(search(dtree->root,ForDname)!=1 && dname != NULL){
                printf("ERROR: No such directory \'%s\'\n",ForDname);
                return;
            }

        }


        if(search(dtree->root,ForDname)==1){
            ForDname=filename;
        }


        if (tempfile == NULL)
            {
                tempfile = makefile(ForDname);
                strcpy(tempfile->content, mvfile->content);
            }
        else
            {
                if (tempfile->RSibling == NULL)
                    {
                        tempfile->RSibling = makefile(ForDname);
                        strcpy(tempfile->RSibling->content, mvfile->content);
                        int j=0;
                        while(tempfile->content[j]!='\0')
                            j++;
                        tempfile->size =j;
                    }
                else
                    {
                        tempfile = makefile(ForDname);
                        strcpy(tempfile->content, mvfile->content);
                        int j=0;
                        while(tempfile->content[j]!='\0')
                            j++;
                        tempfile->size =j;
                    }
            }
        change_directory(dtree->root, currentdir->name);
    }
}




void ls() { // 기본 명령어 ls 함수
    DirNode* t = dtree->headerNode; // 현재 디렉토리 를 t라고 함
    //printf("%s",t->LChild->name);
    top = -1;

    if(t->LChild != NULL) { // t->lchild 가 있으면  t 안에 폴더와 파일들이 있음
        stack[++top] = t->LChild->name; // dir 배열에 lchild 넣고
        t = t->LChild;  // t->lchild의 rsibling을 탐색하기위해서 t = t->lchild 라고함

        while(t->RSibling != NULL) { // rsibling이 있으면 t 안에 두개이상의 폴더, 파일있음
            stack[++top] = t->RSibling->name; // rsibling 넣어주고
            t = t->RSibling; // 계속해서 rsibling 탐색
        }

        while(top != -1) {
// 숨김폴더가 아닌 내용물들만
                printf("%s    ", stack[top]); // name 출력
 // 숨김폴더들도 일단 dir 배열에 들어있기 때문에 index는 계속 증가해줘야함
                top--;
        }
        printf("\n");
    }
    else // t->lchild == NULL 이면 안에 폴더와 파일이 하나도없음
        printf("\n");
}


void preorder(DirNode* node)
{
    if (node != NULL)
    {
        printf("%s ", node->name);
        preorder(node->RSibling);
        preorder(node->LChild);
    }
}



void preorderprint()
{
    preorder(dtree->root);
    printf("\n");
}

DirNode* freeall(DirNode* t)
{
    if(t!=NULL){
        freeall(t->LChild);
        freeall(t->RSibling);
        free(t);
    }
}


int search(DirNode *t,char *dname)
{
    int found = 0;
    if (t != NULL)
    {
        if (t->type=='d')
        {
            if (strcmp(t->name, dname) == 0) //if dname == t->name
            {
                return found = 1;
            }
            else  // if dname != t->name
            {
               // printf("%s",dname);
                found = search(t->LChild, dname);
                if (!found)
                    return search( t->RSibling, dname);
                return found;
            }
        }
        else  // if t->id = 'f'
            return search( t->RSibling, dname);
    }
    else// if t == NULL
        return 0;
}




int readuser(char tmp[])
{
    UNode* NewNode = (UNode*)malloc(sizeof(UNode));
    char* str;

    NewNode->LinkNode = NULL;

    str = strtok(tmp, " ");
    strcpy(NewNode->name, str);
    str = strtok(NULL, " ");
    NewNode->uid = atoi(str);
    str = strtok(NULL, " ");
    NewNode->gid = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.year = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.month = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.day = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.hour= atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.min = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.sec = atoi(str);

    str = strtok(NULL, " ");
    str[strlen(str)-1] = '\0';
    strcpy(NewNode->dir, str);

    if(strcmp(NewNode->name, "root") == 0){
        ulist->head = NewNode;
        ulist->tail = NewNode;
    }
    else{
        ulist->tail->LinkNode = NewNode;
        ulist->tail = NewNode;
    }
    return 0;
}

void load_users()
{
    FILE *fp;
    char buffer[512];



    if ((fp = fopen("user.txt", "r")) != NULL) {
        while(fgets(buffer, 512, fp) != NULL){
            readuser(buffer);

    }
    fclose(fp);
    }
    else{
        printf("fail to load user list!\n");
        return;
    }

}

void print_ulist( UNode *unode)
{
    if(unode!=NULL){
        printf("%s %d %d %d/%d/%d %d:%d:%d\n",unode->name, unode->uid, unode->gid, unode->recent_time.year, unode->recent_time.month, unode->recent_time.day, unode->recent_time.hour, unode->recent_time.min, unode->recent_time.sec);
        print_ulist(unode->LinkNode);
    }
}



void write_df(FILE *fp,DirNode *t)
{
    if(t!=NULL){
        fwrite(t,sizeof(DirNode),1,fp);
        write_df(fp,t->RSibling);
        write_df(fp,t->LChild);
    }
}

void save_df(DirNode *t, char* str)
{
    FILE *fp;
    if((fp=fopen(str,"wb"))==NULL){
        printf("File open error!\n"); return;
    }
    write_df(fp,t);
    fclose(fp);
}

void df_treegen(DirNode* *curr,DirNode* *n)
{
   char *str, *next_str = NULL;
   str =(*n)->path;

    char path[MAX_ARGV];
    strcpy(path,(*n)->path);


    if(strcmp(str,"/") != 0)
    {
        str = strtok(str,"/");
        next_str = strtok(NULL,"/");
    }


    while(next_str!=NULL){
        str = next_str;
        next_str = strtok(NULL,"/");
    }
    strcpy((*n)->path, path);


    if((*curr)!=NULL){
            if(str!=NULL){


                if(strcmp((*curr)->name,str)==0&&(*curr)->type=='d'){

                    DirNode* t=(*curr)->LChild;

                    while(t!=NULL&&t->RSibling!=NULL){
                        t=t->RSibling;
                    }
                    if(t==NULL){
                        //mode_to_permission((*n), (*n)->mode);

                        (*curr)->LChild=*n;
                    }
                    else {
                        //mode_to_permission((*n), (*n)->mode);
                        t->RSibling=*n;
                    }
                    (*n)->Parent=*curr; // Refresh parent
                }
                else{
                    df_treegen(&((*curr)->RSibling),n);
                    df_treegen(&((*curr)->LChild),n);
                }
            }
    }
}

void df_loadcheck(DirNode* *curr,FILE *fp)
{
    while(!feof(fp)){
        DirNode* n=(DirNode*)malloc(sizeof(DirNode));
        fread(n,sizeof(DirNode),1,fp);


        n->LChild=NULL; n->RSibling=NULL;

    if(strcmp(n->name, "/")!=0){
    if(!strlen(n->name)){
            free(n);
            return;
        }
        else if(dtree->root==NULL){
            dtree->root= dtree->headerNode =*curr=n;
            n->Parent=dtree->root;
            mode_to_permission(dtree->root,dtree->root->mode);
        }
        else{
            df_treegen(curr,&n);
        }
    }
    }
}

int load_df(DirNode* *curr,char *dir)
{
    FILE *fp;
    if ((fp=fopen(dir,"rb"))==NULL){
        printf("Directory file does not exist!\n");
        return 0;
    }
    df_loadcheck(curr,fp);
    fclose(fp);
    return 1;
}


UNode *intro( UList *ulist)
{
    char account[MAX_NAME];
    UNode *login_user=NULL;

    printf("mkdir: create directory, options -m -p\ncd: change directory\nEnter exit to finish the programme\n\n");

    while(1){
        printf("localhost login: ");
        fgets(account,sizeof(account),stdin);
        account[strlen(account)-1] = '\0';
        UNode *lead= ulist->head;
        while(lead!=NULL&&strcmp(lead->name,account)){
            lead=lead->LinkNode;
        }
        login_user = lead;
        if((login_user!=NULL)) {
            change_directory(dtree->headerNode, login_user->name);
            break;
        }
        else printf("Login incorrect\n\n");
    } // month day year
/*    printf("Login Success! User home path: %s\n",login_user->home_path);
    printf("Last login: %s %d %d %d:%d:%d\n",get_month(login_user->recent_time.month),login_user->recent_time.day,login_user->recent_time.year+1900,login_user->recent_time.hour,login_user->recent_time.min,login_user->recent_time.sec);
    login_user->recent_time=make_fd_refresh_time();
    cd(cur,login_user,login_user->home_path);
*/
    return login_user;
}



int mv_cur(DirTree* t,char* path) // current를 바꿔주는 함수
{
    DirNode* tmp = NULL;
    if(!strcmp(path,".")) // cd .
    {
    }
    else if(!strcmp(path,"..")) // cd ..
    {
        if(t->headerNode != t->root)
            t->headerNode = t->headerNode->Parent;
    }
    else // 경로 지정했을때
    {
        tmp = exist(path, 'd');
        if(tmp != NULL)
            t->headerNode = tmp;
        else
            return -1;
    }
    return 0;
}
int mv_path(DirTree* t, char* path)
{
    DirNode* tmp = t->headerNode;
    char tmp_path[30];
    char* str = NULL;
    int val = 0;

    strncpy(tmp_path, path, 30);
    if(!strcmp(path, "/")) // path가 / 이면 root
        t->headerNode = t->root;
    else
    {
        if(!strncmp(path, "/", 1)) // 절대경로 /시작
        {
            if(strtok(path, "/") == NULL)
                return -1;
            t->headerNode = t->root;
        }
        str = strtok(tmp_path, "/");
        while(str != NULL)
        {
            val = mv_cur(t,str);

            if(val != 0)
            {
                t->headerNode = tmp;
                return -1;
            }
            str = strtok(NULL, "/");
        }
    }
    return 0;
}
int ls_func(DirTree* t, int a, int l)
{
    DirNode* tmp1;
    DirNode* tmp2;
    char type;
    int cnt; // 하드링크

    tmp1 = t->headerNode->LChild; // 현재 디렉토리내의 첫번째

    if(l == 0) // l 없고
    {
        if(a == 0) // a 도없는 그냥 ls
        {
            if(tmp1 == NULL) // 빈 디렉토리면 -1 리턴
                return -1;
        }
        else if(a == 1) // ls -a 옵션
        {
            printf(".\t"); // . directory
            if(t->headerNode!= t->root) // root는 상위 디렉토리가 없기때문에
                printf("..\t");
            // 여기까지 일단 . 이랑 ..을 출력
        }
        while(tmp1 != NULL) // 현재 리렉토리에 dir이나 file이 있을때
        {
            if(a == 0)
            {
                if(strncmp(tmp1->name,".",1) == 0) // 파일 이름이 .으로 시작하면 숨김파일
                {
                    tmp1 = tmp1->RSibling; // 숨김파일이기때문에 출력안하고 넘어감
                    continue;
                }
            }
            printf("%s\t", tmp1->name);
            tmp1 = tmp1->RSibling;
        }
        printf("\n");
    }
    else // l 옵션 있다
    {
        if(a == 0)
        {
            if(tmp1 == NULL)    // 아무것도 안들어있다.
            {
                printf("total: 0\n");
                return -1;
            }
        }
        if(a == 1)  // a 옵션이있다.
        {
            tmp2 = t->headerNode->LChild;
            if(tmp2 == NULL)
            {
                cnt = 2;    // 2개인 이유는 ( .과 ..은  기본으로 세야함)
            }
            else
            {
                if(tmp2->type == 'd')
                    cnt = 3;
                else
                    cnt = 2;
                while(tmp2->RSibling != NULL)
                {
                    tmp2 = tmp2->RSibling;
                    if(tmp2->type=='d')
                        cnt += 1;
                }
            }
            // 현재 위치 (.폴더)에 대한 정보
            printf("%c", t->headerNode->type); // d or x
            printf("%s", t->headerNode->permission); // rwxr-xr-x
            printf("%3d", cnt); // 하드링크
            printf("   ");
            printf("root  root ");
            printf("%5d ", t->headerNode->size);
            printf("%2d월 %2d일 %2d시%2d분 ", t->headerNode->recent_time.month,t->headerNode->recent_time.day,t->headerNode->recent_time.hour,t->headerNode->recent_time.min);
            printf(".\n");

                tmp2 = t->headerNode->Parent->LChild;
                if(tmp2 == NULL)
                    cnt = 2;
                else
                {
                    if(tmp2->type=='d')
                        cnt = 3;
                    else
                        cnt = 2;

                    while(tmp2->RSibling != NULL)
                    {
                        tmp2 = tmp2->RSibling;
                        if(tmp2->type=='d')
                           cnt += 1;
                    }
                }
                // 상위폴더 (..폴더)에 대한 정보
                printf("%c", t->headerNode->type); // d or x
                printf("%s", t->headerNode->Parent->permission); // rwxr-xr-x
                printf("%3d", cnt); // 하드링크
                printf("   ");
                printf("root  root ");
                printf("%5d ", t->headerNode->Parent->size);
                printf("%2d월 %2d일 %2d시%2d분 ", t->headerNode->recent_time.month,t->headerNode->recent_time.day,t->headerNode->recent_time.hour,t->headerNode->recent_time.min);
                printf("..\n");
        }
        while(tmp1 != NULL)
        {
            if(a == 0)
            {
                if(!strncmp(tmp1->name,".",1))
                {
                    tmp1 = tmp1->RSibling;
                    continue;
                }
            }
            tmp2 = tmp1->LChild;
            if(tmp2==NULL)
            {
                if(tmp1->type == 'd')
                    cnt = 2;
                else
                    cnt = 1;
            }
            else
            {
                if(tmp2->type=='d')
                    cnt=3;
                else
                    cnt=2;
                while(tmp2->RSibling != NULL)
                {
                    tmp2 = tmp2->RSibling;
                    if(tmp2->type=='d')
                        cnt += 1;

                }
            }
            if(tmp1->type=='d')
                type = 'd';
            else
            type = '-';
            printf("%c", type); // d or x
            printf("%s", tmp1->permission); // rwxr-xr-x
            printf("%3d", cnt); // 하드링크
            printf("   ");
            printf("root  root ");
            printf("%5d ", tmp1->size);
            printf("%2d월 %2d일 %2d시%2d분 ", tmp1->recent_time.month,tmp1->recent_time.day,tmp1->recent_time.hour,tmp1->recent_time.min);
            printf("%-20s\n", tmp1->name);

            tmp1 = tmp1->RSibling;
        }
    }
    return 0;
}


int ls_(DirTree* t, char* cmd)
{
    DirNode* tmp = t->headerNode;
    char* str;
    int val;

    if(cmd == NULL) // 그냥 ls
       {
           ls_func(t, 0, 0);
           return 0;
       }
    if(cmd[0]=='-') // ls -
    {
        if(!strcmp(cmd, "-al") || !strcmp(cmd, "-la"))
        {
            str = strtok(NULL, " ");
            if(str != NULL)
            {
                val = mv_path(t,str);
                if(val != 0)
                    return -1;
            }
            ls_func(t,1, 1);
        }
        else if(!strcmp(cmd, "-l"))
        {
            str = strtok(NULL, " ");
            if(str != NULL)
            {
                tmp = t->headerNode;
                val = mv_path(t,str);
                if(val != 0)
                    return -1;
            }
            ls_func(t,0, 1);
        }
        else if(!strcmp(cmd, "-a"))
        {
            str = strtok(NULL, " ");
            if(str != NULL)
            {
                tmp = t->headerNode;
                val = mv_path(t,str);
                if(val != 0)
                    return -1;
            }
            ls_func(t,1,0);
        }
        else
        {
            str = strtok(cmd, "-");
            if(str == NULL)
            {
                printf("잘못된 명령어\n");
                return -1;
            }
            else
            {
                printf("잘못된 옵션 -- %s\n", str);
                return -1;
            }
        }
    }
    else
    {
        tmp = t->headerNode;
        val = mv_path(t,cmd);
        if(val != 0)
            return -1;
        ls_func(t,0, 0);
        t->headerNode = tmp; // 다시 현재경로 바꿔줘야함
    }
    if(str != NULL)
        t->headerNode = tmp;
    return 0;
}







int readdir(DirTree* dtree, char* tmp)
{
    DirNode* NewNode = (DirNode*)malloc(sizeof(DirNode));
    DirNode* temp = (DirNode*)malloc(sizeof(DirNode));

    char* str, *str_next;


    NewNode->LChild = NewNode->RSibling = NULL;

    str = strtok(tmp, " ");
    strcpy(NewNode->name, str);
    str = strtok(NULL, " ");
    NewNode->type = *str;
    str = strtok(NULL, " ");
    NewNode->mode = atoi(str);
    str = strtok(NULL, " ");
    NewNode->uid = atoi(str);
    str = strtok(NULL, " ");
    NewNode->gid = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.year = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.month = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.day = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.hour= atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.min = atoi(str);
    str = strtok(NULL, " ");
    NewNode->recent_time.sec = atoi(str);

    str = strtok(NULL, " ");



    if(str == NULL){
     NewNode = dtree->root;
     return 0;
    }

    str[strlen(str)-1] = '\0';
    strcpy(NewNode->path,str);
    mode_to_permission(NewNode, NewNode->mode);

    if(strcmp(dtree->root->name, str) == 0){

        if(dtree->root->LChild==NULL)
        {
            NewNode->Parent=dtree->root;
            dtree->root->LChild = NewNode;
        }
        else{
            NewNode->Parent = dtree->root;
            temp = dtree->headerNode->LChild;
            while (temp->RSibling != NULL)
                temp = temp->RSibling;
            temp->RSibling = NewNode;
        }
    }
    else{
            int i = 1;
            str = strtok(str,"/");

            str_next = strtok(NULL,"/");



            while(str!=NULL){
                change_directory(dtree->root, str);

                if(str!=NULL&&str_next==NULL){
                            if(dtree->headerNode->LChild==NULL)
                            {

                                NewNode->Parent = dtree->headerNode;
                                dtree->headerNode->LChild = NewNode;

                            }
                            else{
                                NewNode->Parent = dtree->headerNode;
                                temp = dtree->headerNode->LChild;
                                while (temp->RSibling != NULL)
                                    temp = temp->RSibling;
                                temp->RSibling = NewNode;
                            }
                }
                str = str_next;
                str_next = strtok(NULL,"/");
            }



        }
    dtree->headerNode = dtree->root;

    return 0;
}

void load_dir()
{
    FILE *fp;
    char buffer[516];


    int i = 0;
    if ((fp = fopen("Directory1.txt", "r")) != NULL) {
        while(fgets(buffer, 516, fp) != NULL){
            readdir(dtree, buffer);
        }
    }
    else{
        printf("fail to load directory list!\n");
        return;
    }
    fclose(fp);
}

void WriteNode(DirTree* dirTree, DirNode* dirNode, FILE* Dir)
{
    fprintf(Dir, "%s %c %d ", dirNode->name, dirNode->type, dirNode->mode);
    fprintf(Dir, "%d %d %d %d %d %d %d %d %d", dirNode->uid, dirNode->gid, dirNode->recent_time.year, dirNode->recent_time.month, dirNode->recent_time.day, dirNode->recent_time.hour, dirNode->recent_time.min, dirNode->recent_time.sec, dirNode->size);

    if(dirNode == dirTree->root)
        fprintf(Dir, "\n");
    else
        fprintf(Dir, " %s\n" ,getpath(dirNode));

    if(dirNode->RSibling != NULL){
        WriteNode(dirTree, dirNode->RSibling,Dir);
    }
    if(dirNode->LChild != NULL){
        WriteNode(dirTree, dirNode->LChild,Dir);
    }
}

void SaveDir(DirTree* dirTree)
{
    FILE* Dir;

    Dir = fopen("Directory1.txt", "w");

    WriteNode(dirTree, dirTree->root, Dir);

    fclose(Dir);
}



int main()
{
    dtree = CreateRootDirectory();
    //dtree->root = NULL;
    DirNode* rt = NULL; load_df(&dtree->root,"file1.txt");
        //dtree->root->LChild = rt;
    //load_dir();


    preorderprint();

    ulist = user_reset_ulist();
    load_users(ulist);
    ulist->current = intro(ulist);





    char arg[MAX_ARGV] = {'\0'};
    char* str = malloc(MAX_ARGV);
    char* filename, *dname;

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutex_init(&mutex, &mutex_attr);


    while(printf("%s@DguOS:~",ulist->current->name)&& pwd(dtree->root)&& printf("$ ")&&fgets(arg,sizeof(arg),stdin)!=NULL&&strncmp(arg,"exit",4))
    {
        arg[strlen(arg)-1] = '\0';

        if(!strncmp(arg," ",1)||strlen(arg)<=0) continue;

        if(strcmp(arg,"")==0){
              return -1;
        }
        str = strtok(arg, " ");

        if(!strcmp(str,"mkdir"))
        {
            str = strtok(NULL, " ");
            make_dir(dtree->headerNode,ulist->current,str);
        }
        else if(!strcmp(str,"pwd"))
        {
            pwd(dtree->headerNode);
            printf("\n");
        }
        else if(!strcmp(str,"cd"))
        {
            str = strtok(NULL, " ");
            cd(dtree->headerNode, str);
        }
        else if(!strcmp(str,"search"))
        {
            str = strtok(NULL, " ");
            printf("%d", search(dtree->root, str));
        }
        else if(!strcmp(str, "ls"))
        {
            str = strtok(NULL, " ");
            ls_(dtree, str);

        }
        else if(!strcmp(str, "path"))
        {
            printf("%s\n",getpath(dtree->headerNode));
        }
        else if(!strcmp(str, "rm"))
        {
            str = strtok(NULL, " ");
            rm(dtree->headerNode, ulist->current,str);
        }
        else if(!strcmp(str, "cat"))
        {
            str = strtok(NULL, " ");
            cat(str);
        }
        else if(!strcmp(str, "find"))
        {
            str = strtok(NULL, " ");
            find(str);
        }
        else if(!strcmp(str, "cp"))
        {
            str = strtok(NULL, " ");
            filename = strtok(NULL, " ");
            my_cp(dtree, str, filename);
        }
        else if(!strcmp(str, "pre"))
        {
            preorderprint();
        }
    else if(!strcmp(str, "mv"))
        {
            filename = strtok(NULL, " ");
            dname = strtok(NULL, " ");
            my_mv(dtree, filename, dname);
        }
        else{
            printf("ERROR: Syntax Error \n");
        }
    }
   //SaveDir(dtree);
    save_df(dtree->root, "file1.txt");
    freeall(dtree->root);
    return 0;

}





