/*test_client.h*/
#ifndef _TEST_CLIENT_H_
#define _TEST_CLIENT_H_

/*head files*/
#include<sys/epoll.h>
#include<pthread.h>
#include "mosquitto.h"
#include "uthash.h"
#include <stdlib.h>
#include <string.h>
#include<sys/time.h>
#include<unistd.h>


#define TC_MAX_CLIENT_NUM 1000
#define TC_MAX_TOPIC_LEN 1024

#define TC_DEFAULT_UN "JASON_HOU"
#define TC_DEFAULT_PWD "JASON_HOU"
#define MAX_HOST_BUF_LEN 125
#define MAX_VIRTUALIP_BUF_LEN 1000//virtual IPs buf len aiden150728
#define MAX_VIRTUALIP_NUM 50//virtual IPs num aiden150728
#define DEFAULT_COMMON_VALUE 1024
#define CONFIG_FILE_NAME "sub_client.conf"
#define MAX_FAIL_NUM 100

#define PWD_USR_BEGIN 10000
#define PWD_USR_END 30000

#define MAX_MSG_LEN 10240


#define TC_SUC 0
#define TC_ERROR -1
#define TC_NOT_SEND -2


/*defination of data type
*/
enum e_test_type{
	TEST_TYPE_ONLY_RECEIVE,
	TEST_TYPE_SEND_TO_ITSELF,
	TEST_TYPE_SEND_TO_OTHERS,
	TEST_TYPE_UNKNOWN_TYPE
};

struct t_fd2client{
	int fd;
	int client_index;
	UT_hash_handle hh;
};

struct userdata {
	char **topics;
	int topic_count;
	int topic_qos;
	char *username;
	char *password;
	int verbose;
	long send_msg_counter;
	long recv_msg_counter;
	bool quiet;/*out put error log or not*/
	bool no_retain;
	/*---added by jason.hou---*/
	int keepalive;
	char* usr_id;
	int message_counter;
	char *virtual_ip[MAX_VIRTUALIP_NUM];//virtual IPs with which we establish the sockets, aiden150728
	struct mosquitto * context;
	bool reg_epoll;/*added by jason.hou*/
	bool is_valid ;
	bool is_sub;
};

struct t_client_database{
	struct userdata **usr_info_array;
	int max_buf_len;
	int used_buf_len;
	int epoll_fd;
	char *server_host;
	int server_port;
	int start_id;
	int end_id;
	int keep_alive;
	bool stop_handle_thread;
	pthread_t handle_thread_id;
	pthread_t send_thread_id;
	bool stop_send_thread;
	bool is_inited;
	struct t_fd2client* ht_fd2c;/*hash table for the map from fd to client int client data base*/
};	

struct t_client_config{
	char *host;
	int port;
	char *virtual_ip[MAX_VIRTUALIP_NUM];//aiden150728
	int keep_alive;
	bool debug;
	int start_id;
	int end_id;
	int sleep_time_max;
	int sleep_time_mid;
	int sleep_time_min;
	int sleep_level1;
	int sleep_level2;
	int sleep_level3;
	int sleep_interval_max;
	int sleep_interval_mid;
	int sleep_interval_min;
	int batchSize;
	int sleepAfterBatch;
	int send_print_speed;
	int recv_print_speed;
	char* topic_prefix;
	enum e_test_type test_type;
	int send_client_id;
	int send_client_index;
};


/*function declaration*/
static void* handle_thread(void* _param);
int reg_socket(struct userdata* _tmp_usr_data);
int unreg_socket(struct userdata* _tmp_usr_data);
void handle_events_result(struct epoll_event* epoll_events, int _event_num);
int connect_user(struct userdata* _tmp_usr_data);
void clear_usr_data(struct userdata* _usr_data);
void check_clients();

void clear_fd2c();
void del_fd2c(int key_fd);
struct t_fd2client * get_fd2c(int key_fd);
void set_fd2c(int key_fd,int _client_index);
long int get_time(char* msg);


int create_send_thread(struct t_client_config* cfg_data);
int join_send_thread();
static void* send_thread(void* _param);
//static void pub(struct userdata *mosq_info,struct t_client_config* p_client_cfg);
static void pub(struct userdata *send_mosq_info, struct userdata *recv_mosq_info,struct t_client_config* p_client_cfg);
static struct userdata *get_send_client(struct t_client_config* p_client_cfg); 


#ifdef __cplusplus
extern "C" {
#endif

libmosq_EXPORT int join_handle_thread();
libmosq_EXPORT int create_handle_thread();
libmosq_EXPORT int init_db(struct t_client_config* p_config_data);
libmosq_EXPORT int uninit_data(struct t_client_config* p_config_data);
libmosq_EXPORT int reg_clients(struct userdata* _tmp_usr_data, char *pStrLocalVirtualIP);


#ifdef __cplusplus
}
#endif


#endif /*_TEST_CLIENT_H_*/
