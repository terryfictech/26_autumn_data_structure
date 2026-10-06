#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 表头（下标 0 或头结点之后）表示最近使用；表尾表示最久未使用。 */
typedef struct{
    int key;
    int value;
}Item;

typedef struct Node{
    Item item;
    struct Node *next;
}Node;

typedef struct{
    int hits;//get找到key的次数
    int misses;//get没找到key的次数
    int evictions;//缓存满时，插入新key导致淘汰的key次数
}Stats;

typedef struct{
    int kind;              // 1: 顺序表；2: 带头结点的单链表
    int capacity;
    int size;
    Item *array;
    Node *head;
    Stats stats;
}Cache;

typedef struct{
    int type;              // 1: GET；2: PUT 
    int key;
    int value;
}Operation;

/* 初始化和销毁缓存。切换实现或容量时重新初始化。 */
int cache_init(Cache *cache,int kind,int capacity){
    memset(cache,0,sizeof(*cache));
    if(capacity<=0||(kind!=1&&kind!=2)) return 0;
    cache->kind=kind;
    cache->capacity=capacity;
    if(kind==1){
        cache->array=(Item*)malloc((size_t)capacity*sizeof(Item));
        return cache->array!=NULL;
    }
    cache->head=(Node *)malloc(sizeof(Node));
    if(cache->head==NULL) return 0;
    cache->head->next=NULL;
    return 1;
}

void cache_destroy(Cache *cache){
    Node *p=cache->head;
    while(p!=NULL){
        Node *next=p->next;
        free(p);
        p=next;
    }
    free(cache->array);
    memset(cache,0,sizeof(*cache));
}

/* 顺序表：找到元素后，把前面的元素右移一格，再放到下标 0。 */
void array_move_to_front(Cache *cache,int index){
    Item selected=cache->array[index];
    int i;
    for(i=index;i>0;i--) cache->array[i]=cache->array[i-1];
    cache->array[0]=selected;
}

int array_get(Cache *cache,int key,int *value){
    int i;
    for(i=0;i<cache->size;i++){
        if(cache->array[i].key==key){
            *value=cache->array[i].value;
            array_move_to_front(cache,i);
            ++cache->stats.hits;
            return 1;
        }
    }
    ++cache->stats.misses;
    return 0;
}

void array_put(Cache *cache,int key,int value){
    int i;
    for(i=0;i<cache->size;i++){
        if(cache->array[i].key==key){
            cache->array[i].value=value;
            array_move_to_front(cache,i);
            return;
        }
    }
    if(cache->size==cache->capacity){
        --cache->size;  /* 直接舍弃数组末尾的最久未使用元素。 */
        ++cache->stats.evictions;
    }
    for(i=cache->size;i>0;i--) cache->array[i]=cache->array[i-1];
    cache->array[0].key=key;
    cache->array[0].value=value;
    ++cache->size;
}

/* 单链表查找时返回前驱，便于摘下命中的结点。 */
Node *list_find_previous(Cache *cache,int key){
    Node *previous=cache->head;
    while(previous->next!=NULL){
        if(previous->next->item.key==key) return previous;
        previous=previous->next;
    }
    return NULL;
}

void list_move_to_front(Cache *cache,Node *previous){
    Node *selected;
    if(previous==cache->head) return;
    selected=previous->next;
    previous->next=selected->next;
    selected->next=cache->head->next;
    cache->head->next=selected;
}

int list_get(Cache *cache,int key,int *value){
    Node *previous=list_find_previous(cache,key);
    if(previous==NULL){
        ++cache->stats.misses;
        return 0;
    }
    *value=previous->next->item.value;
    list_move_to_front(cache,previous);
    ++cache->stats.hits;
    return 1;
}

int list_put(Cache *cache,int key,int value){
    Node *previous=list_find_previous(cache,key);
    Node *new_node;
    if(previous!=NULL){
        previous->next->item.value=value;
        list_move_to_front(cache,previous);
        return 1;
    }
    new_node=(Node *)malloc(sizeof(Node));
    if(new_node==NULL) return 0;
    if(cache->size==cache->capacity){
        previous=cache->head;
        while(previous->next->next!=NULL) previous=previous->next;
        free(previous->next);
        previous->next=NULL;
        --cache->size;
        ++cache->stats.evictions;
    }
    new_node->item.key=key;
    new_node->item.value=value;
    new_node->next=cache->head->next;
    cache->head->next=new_node;
    ++cache->size;
    return 1;
}

/* 返回 1 表示命中，0 表示未命中；value=-1 也可以正常存储。 */
int cache_get(Cache *cache,int key,int *value){
    if(cache->kind==1) return array_get(cache,key,value);
    return list_get(cache,key,value);
}

int cache_put(Cache *cache,int key,int value){
    if(cache->kind==1){
        array_put(cache,key,value);
        return 1;
    }
    return list_put(cache,key,value);
}

void print_cache(const Cache *cache){
    int i;
    Node *p;
    printf("最近使用->最久未使用: ");
    if(cache->size==0) printf("(缓存为空)");
    if(cache->kind==1){
        for(i=0;i<cache->size;i++)
            printf("(%d:%d) ",cache->array[i].key,cache->array[i].value);
    } else{
        for(p=cache->head->next;p!=NULL;p=p->next)
            printf("(%d:%d) ",p->item.key,p->item.value);
    }
    putchar('\n');
}

void print_stats(const Stats *stats){
    int accesses=stats->hits+stats->misses;
    double rate=accesses==0?0.0:100.0*stats->hits/accesses;
    printf("Hits: %d, misses: %d, hit rate: %.2f%%, evictions: %d\n",
           stats->hits,stats->misses,rate,stats->evictions);
}

/* 文件格式：每行 GET key 或 PUT key value，允许空行和 # 注释。 */
int load_operations(const char *path,Operation **operations,int *count){
    FILE *file=fopen(path,"r");
    Operation *data=NULL;
    int used=0,capacity=0,line_number=0,ok=1;
    char line[256];
    if(file==NULL){
        printf("无法打开文件：%s\n",path);
        return 0;
    }
    while(fgets(line,sizeof(line),file)!=NULL){
        char command[16],extra;
        char *p,*comment=strchr(line,'#');
        int key,value,fields;
        Operation op;
        ++line_number;
        if(comment!=NULL) *comment='\0';
        p=line;
        while(isspace((unsigned char)*p)) ++p;
        if(*p=='\0') continue;
        fields=sscanf(p,"%15s %d %d %c",command,&key,&value,&extra);
        if(strcmp(command,"GET")==0&&fields==2){
            op.type=1;
            op.key=key;
            op.value=0;
        } else if(strcmp(command,"PUT")==0&&fields==3){
            op.type=2;
            op.key=key;
            op.value=value;
        } else{
            printf("第 %d 行的操作格式不正确。\n",line_number);
            ok=0;
            break;
        }
        if(used==capacity){
            int new_capacity=capacity==0?64:capacity*2;
            Operation *new_data=(Operation *)realloc(data,(size_t)new_capacity*sizeof(Operation));
            if(new_data==NULL){
                puts("内存不足，无法保存操作。");
                ok=0;
                break;
            }
            data=new_data;
            capacity=new_capacity;
        }
        data[used++]=op;
    }
    if(ferror(file)) ok=0;
    fclose(file);
    if(!ok||used==0){
        if(used==0&&ok) puts("操作文件为空。");
        free(data);
        return 0;
    }
    *operations=data;
    *count=used;
    return 1;
}

Operation *random_operations(int count,int largest_key,unsigned int seed){
    Operation *data=(Operation *)malloc((size_t)count*sizeof(Operation));
    int i;
    if(data==NULL) return NULL;
    srand(seed);
    for(i=0;i<count;i++){
        data[i].type=rand()%2+1;
        data[i].key=rand()%(largest_key+1);
        data[i].value=data[i].type==2?rand():0;
    }
    return data;
}

int execute(Cache *cache,Operation op,int *hit,int *value){
    if(op.type==1){
        *hit=cache_get(cache,op.key,value);
        if(!*hit) *value=-1;
        return 1;
    }
    *hit=0;
    *value=0;
    return cache_put(cache,op.key,op.value);
    //返回值表示execute是否成功，不代表是否命中
}

void run_on_active(Cache *cache,const Operation *data,int count,int show_each){
    int hit,value;
    for(int i=0;i<count;i++){
        if(!execute(cache,data[i],&hit,&value)){
            puts("内存不足，已停止执行。");
            return;
        }
        if(show_each){
            if(data[i].type==1) printf("GET %d -> %d (%s)\n",data[i].key,
                                         value,hit?"hit":"miss");
            else printf("PUT %d %d\n",data[i].key,data[i].value);
            print_cache(cache);
        }
    }
    printf("已执行 %d 次操作。\n",count);
    print_stats(&cache->stats);
}

/* 两种实现分别从空缓存开始，用同一操作序列；计时不含文件读取和打印。 */
int compare(const Operation *data,int count,int capacity){
    Cache caches[2];
    int *values[2]={NULL,NULL};
    unsigned char *hits[2]={NULL,NULL};
    double milliseconds[2];
    int i,k,equal=1,ready=1;
    memset(caches,0,sizeof(caches));
    for(k=0;k<2;k++){
        clock_t start,finish;
        if(!cache_init(&caches[k],k+1,capacity)){
            ready=0;
            break;
        }
        values[k]=(int *)malloc((size_t)count*sizeof(int));
        hits[k]=(unsigned char *)malloc((size_t)count);
        if(values[k]==NULL||hits[k]==NULL){
            ready=0;
            break;
        }
        start=clock();
        for(i=0;i<count;i++){
            int hit,value;
            if(!execute(&caches[k],data[i],&hit,&value)){
                ready=0;
                break;
            }
            hits[k][i]=(unsigned char)hit;
            values[k][i]=value;
        }
        if(!ready) break;
        finish=clock();
        milliseconds[k]=1000.0*(finish-start)/CLOCKS_PER_SEC;
    }
    if(ready){
        for(i=0;i<count;i++){
            if(data[i].type==1&&
                (hits[0][i]!=hits[1][i]||values[0][i]!=values[1][i])) equal=0;
        }
        if(caches[0].size!=caches[1].size||
            caches[0].stats.hits!=caches[1].stats.hits||
            caches[0].stats.misses!=caches[1].stats.misses||
            caches[0].stats.evictions!=caches[1].stats.evictions) equal=0;
        {
            Node *p=caches[1].head->next;
            for(i=0;i<caches[0].size;i++){
                if(p==NULL||caches[0].array[i].key!=p->item.key||
                    caches[0].array[i].value!=p->item.value) equal=0;
                if(p!=NULL) p=p->next;
            }
        }
        printf("Operations: %d, capacity: %d\n",count,capacity);
        puts("         总耗时(ms)   平均耗时(us/次)   命中率   Evictions");
        for(k=0;k<2;k++){
            Stats s=caches[k].stats;
            int accesses=s.hits+s.misses;
            double rate=accesses==0?0.0:100.0*s.hits/accesses;
            printf("%-7s  %8.3f   %12.3f   %7.2f%%   %d\n",k==0?"Array":"List",
                   milliseconds[k],milliseconds[k]*1000.0/count,rate,s.evictions);
        }
        printf("结果一致：%s\n",equal?"是":"否");
    } else{
        puts("内存不足，无法进行对比。");
    }
    for(k=0;k<2;k++){
        free(values[k]);
        free(hits[k]);
        cache_destroy(&caches[k]);
    }
    return ready&&equal;
}

void page_experiment(int capacity){
    int pages[]={7,0,1,2,0,3,0,4,2,3,
                   0,3,2,1,2,0,1,7,0,1};
    int kind,i,value;
    for(kind=1;kind<=2;kind++){
        Cache cache;
        if(!cache_init(&cache,kind,capacity)){
            puts("内存不足。");
            return;
        }
        for(i=0;i<(int)(sizeof(pages)/sizeof(pages[0]));i++){
            if(!cache_get(&cache,pages[i],&value)&&
                !cache_put(&cache,pages[i],pages[i])){
                puts("内存不足。");
                cache_destroy(&cache);
                return;
            }
        }
        printf("%s: ",kind==1?"Array":"List");
        print_stats(&cache.stats);
        print_cache(&cache);
        cache_destroy(&cache);
    }
}

int read_int(const char *prompt,int minimum,int maximum,int *result){
    char line[128],extra;
    int value;
    for(;;){
        printf("%s",prompt);
        if(fgets(line,sizeof(line),stdin)==NULL) return 0;
        if(sscanf(line,"%d %c",&value,&extra)==1&&
            value>=minimum&&value<=maximum){
            *result=value;
            return 1;
        }
        printf("请输入 %d 到 %d 之间的整数。\n",minimum,maximum);
    }
}

int read_path(char *path,size_t size){
    size_t length;
    printf("操作文件路径：");
    if(fgets(path,(int)size,stdin)==NULL) return 0;
    length=strlen(path);
    if(length>0&&path[length-1]=='\n') path[length-1]='\0';
    return path[0]!='\0';
}

int main(void){
    Cache cache;
    int choice,kind=1,capacity=3,show_each=0;
    if(!cache_init(&cache,kind,capacity)) return 1;
    while(1){
        Operation *data=NULL;
        int count=0,key,value,seed,largest_key,hit;
        char path[512];
        printf("\nLRU (%s, capacity %d)\n",kind==1?"array":"list",capacity);
        puts("1 选择顺序表/链表（重置缓存）   2 设置容量（重置缓存）");
        puts("3 GET   4 PUT   5 查看缓存   6 查看统计");
        puts("7 从文件执行   8 随机生成并执行   9 文件数据对比   10 随机数据对比");
        printf("11 切换逐步输出（%s）   12 页面置换实验   0 退出\n",
               show_each?"开启":"关闭");
        if(!read_int("Choice: ",0,12,&choice)||choice==0) break;
        if(choice==1||choice==2){
            Cache replacement;
            int new_kind=kind,new_capacity=capacity;
            if(choice==1){
                if(!read_int("1 Array, 2 list: ",1,2,&new_kind)) break;
            } else{
                if(!read_int("Capacity (1..1000000): ",1,1000000,&new_capacity)) break;
            }
            if(cache_init(&replacement,new_kind,new_capacity)){
                cache_destroy(&cache);
                cache=replacement;
                kind=new_kind;
                capacity=new_capacity;
            } else puts("内存不足，仍使用原来的缓存。");
        } else if(choice==3){
            if(!read_int("Key: ",INT_MIN,INT_MAX,&key)) break;
            hit=cache_get(&cache,key,&value);
            printf("GET %d -> %d (%s)\n",key,hit?value:-1,hit?"hit":"miss");
            if(show_each) print_cache(&cache);
        } else if(choice==4){
            if(!read_int("Key: ",INT_MIN,INT_MAX,&key)||
                !read_int("Value: ",INT_MIN,INT_MAX,&value)) break;
            if(!cache_put(&cache,key,value)) puts("内存不足。");
            if(show_each) print_cache(&cache);
        } else if(choice==5) print_cache(&cache);
        else if(choice==6) print_stats(&cache.stats);
        else if(choice==11) show_each=!show_each;
        else if(choice==12){
            if(!read_int("页面容量：",1,1000000,&value)) break;
            page_experiment(value);
        } else{
            if(choice==7||choice==9){
                if(!read_path(path,sizeof(path))){
                    puts("文件路径不能为空。");
                    continue;
                }
                if(!load_operations(path,&data,&count)) continue;
            } else{
                if(!read_int("操作次数 (1..1000000)：",1,1000000,&count)||
                    !read_int("最大 key (0..10000)：",0,10000,&largest_key)||
                    !read_int("随机种子：",0,INT_MAX,&seed)) break;
                data=random_operations(count,largest_key,(unsigned int)seed);
                if(data==NULL){
                    puts("内存不足，无法保存操作。");
                    continue;
                }
            }
            if(choice==7||choice==8) run_on_active(&cache,data,count,show_each);
            else compare(data,count,capacity);
            free(data);
        }
    }
    cache_destroy(&cache);
    return 0;
}
