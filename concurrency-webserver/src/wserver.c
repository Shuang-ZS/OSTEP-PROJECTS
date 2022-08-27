#include <stdio.h>
#include "request.h"
#include "io_helper.h"
#include <pthread.h>

#define MAXBUF 8921

char default_root[] = ".";
int count = 0;
int fill = 0;
int use = 0;
int numb = 1;
int numt = 1;

pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t fills = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;



void put(int value, int max, int buffer[]){
	buffer[fill] = value;
	fill = (fill + 1)% max;
	count++;
}

int get(int buffer[], int max){
	int res = buffer[use];
	use = (use + 1) % max;
	count--;
	return res;
}

void* consummer(void* buffer){
	while(1){
	pthread_mutex_lock(&mutex);
	while(count == 0){
		pthread_cond_wait(&empty, &mutex);
	}
	int res = get((int*)buffer, numb);
	pthread_cond_signal(&fills);
	pthread_mutex_unlock(&mutex);
	request_handle(res);
	close_or_die(res);
	}
	return NULL;
}

typedef struct info_struc{
	int fd;
	char* method;
	int size;
	char* version;
	char* uri;
}infos;

typedef struct link{
	infos info;
	struct link *next;
}linklist;

linklist *head;

linklist* put_SFF(linklist* head, infos info_s){
	linklist *tmp = head;
	//printf(" %d %d\n",info_s.size,(head->info).size );
	if(info_s.size <= (head->info).size){
		linklist *new = (linklist*)malloc(200);
		//(head->info).fd = 999;
		new->info = info_s;
		new->next = head;
		head = new;
		count++;
	}
	else{
		if(tmp->next == NULL){
			linklist *new = (linklist*)malloc(200);
			new->info = info_s;
			new->next = NULL;
			tmp->next = new;
			count++;
		}
		else{
			while(tmp->next != NULL){
				//tmp = tmp->next;
				if(info_s.size <= (tmp->next->info).size){
					linklist *new = (linklist*)malloc(200);
					new->info = info_s;
					new->next = tmp->next;
					tmp->next = new;
					count++;
					break;
				}
				tmp = tmp->next;
			}
		}
	}
	return head;
}

infos get_SFF(){
	printf("test %lld\n", (long long int)head);
	//printf("uri%s %d\n", (head->info).uri,(head->info).size );
	//printf("%s\n", (head->info).uri);
	infos tmp = head->info;
	head = head->next;
	count--;
	return tmp;
}

void* consummer_SFF(){
	while(1){
	pthread_mutex_lock(&mutex);
	while(count == 0){
		pthread_cond_wait(&empty, &mutex);
	}
	//printf("%d\n", count);
	infos res = get_SFF((linklist*)head);
	pthread_cond_signal(&fills);
	pthread_mutex_unlock(&mutex);
	request_handle_SFF(res.fd, res.method, res.uri, res.version);
	close_or_die(res.fd);
	}
	return NULL;
}

void change(linklist *head){
	(head->info).fd = 1000;
}

//
// ./wserver [-d <basedir>] [-p <portnum>] 
// 
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = default_root;
	char *policy = (char*)malloc(10);
    int port = 10000;

    
    while ((c = getopt(argc, argv, "d:p:t:b:s:")) != -1)
	switch (c) {
	case 'd':
	    root_dir = optarg;
	    break;
	case 'p':
	    port = atoi(optarg);
	    break;
	case 'b':{
	    numb = atoi(optarg);
		//int buffer[numb];
	    break;
	}
	case 't':{
	    numt = atoi(optarg);
	    break;
	}
	case 's':{
		strcpy(policy, optarg);
		break;
	}
	default:
	    fprintf(stderr, "usage: wserver [-d basedir] [-p port]\n");
	    exit(1);
	}

    // run out of this directory
    chdir_or_die(root_dir);
	
	//buffer
	int buffer[numb];
	pthread_t p[numt];
	infos info_s[numb];

	if(strcmp(policy, "SFF") == 0){

		head = (linklist*)malloc(200);
		infos fake;
		fake.fd = 0;
		fake.method = "";
		fake.size = 100000;
		fake.uri = "";
		fake.version = "";
		head->info = fake;
		head->next = NULL;
		
		
		for(int m = 0; m < numt; m++){
			pthread_create(&p[m], NULL, consummer_SFF, NULL);
		}
		// now, get to work
		int listen_fd = open_listen_fd_or_die(port);//创建一个socket端点等待请求
		while (1) {

			struct sockaddr_in client_addr;

			int client_len = sizeof(client_addr);
			int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
			char buff[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];
    		readline_or_die(conn_fd, buff, MAXBUF);
			//移动文件的指针
			//FILE *fpt = fdopen(conn_fd,"r");
			//fseek(fpt, 0, SEEK_SET );
			//rewind(conn_fd);
    		sscanf(buff, "%s %s %s", method, uri, version);
			//printf("method:%s uri:%s version:%s\n", method, uri, version);//从buf写入他们
			struct stat sbuf;
			stat(uri, &sbuf);
			infos tmp;
			tmp.fd = conn_fd;
			tmp.method = method;
			tmp.size = sbuf.st_size;
			tmp.uri = uri;
			tmp.version = version;
			// printf("%lld\n", sbuf.st_size);

			pthread_mutex_lock(&mutex);

			while(count == numb){
				pthread_cond_wait(&fills, &mutex);//线程会在wait的时候自动释放锁，并在wait结束的时候自动持
			}
			
			head = put_SFF(head, tmp);
			
			//printf("test %d\n", (head->info).fd);
			
			//printf("uri%s %d\n", (head->info).uri,(head->info).size );
			pthread_cond_signal(&empty);
			pthread_mutex_unlock(&mutex);
				
		}
	}
	else{
	
		for(int m = 0; m < numt; m++){
			pthread_create(&p[m], NULL, consummer, buffer);
		}
		// now, get to work
		int listen_fd = open_listen_fd_or_die(port);//创建一个socket端点等待请求
		while (1) {

			struct sockaddr_in client_addr;

			int client_len = sizeof(client_addr);
			int conn_fd = accept_or_die(listen_fd, (sockaddr_t *) &client_addr, (socklen_t *) &client_len);
			char buff[MAXBUF], method[MAXBUF], uri[MAXBUF], version[MAXBUF];

			pthread_mutex_lock(&mutex);

			while(count == numb){
				pthread_cond_wait(&fills, &mutex);//线程会在wait的时候自动释放锁，并在wait结束的时候自动持
			}
			
			put(conn_fd, numb, buffer);
			pthread_cond_signal(&empty);
			pthread_mutex_unlock(&mutex);
				
		}
	}
    return 0;
}


    


 
