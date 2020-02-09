/*test_client.c*/
#include <sys/epoll.h>
#include <pthread.h>
#include "test_client.h"
#include "mosquitto_internal.h"
#include "mosquitto.h"
#include <stdio.h>

struct t_client_database g_clients_db;
pthread_mutex_t g_epoll_id_lock = PTHREAD_MUTEX_INITIALIZER;
static long int g_total_msg_counter = 0;
static char g_SendMsgBuf[MAX_MSG_LEN];
static int g_recv_print_speed = 10;

static double g_max_delay = 0;
static double g_min_delay = 0;
static double g_total_delay = 0;

long int get_time(char* msg)
{
	if(NULL == msg)
		return 0;
	char* _b_str = "<t:";
	char _e_str = '>';
	long int _value = 0;
	char* _start_pos= NULL;
	char* _end_pos = NULL;
	char _tmp_buf[DEFAULT_COMMON_VALUE];
	int i = 0;
	_start_pos = strstr(msg,_b_str);
	_end_pos = strrchr(msg,_e_str);
	if(NULL==_start_pos || NULL==_end_pos)
		return 0;
	_start_pos += 3;
	if(_start_pos > _end_pos)
		return 0;
	memset(_tmp_buf, 0, DEFAULT_COMMON_VALUE);
	i = 0;
	while(_start_pos < _end_pos){
		*(_tmp_buf+i) = *_start_pos;
		++i;
		++_start_pos;
	}	
	_value = atol(_tmp_buf);
	return _value;
}

void my_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	struct userdata *ud;
	long int msg_send_time = 0;
	long int cur_time_l = 0;
	double _time_interval = 0;
	struct	timeval cur_time;

	if(NULL == obj){
		printf("[my_message_callback] error:NULL == obj \n");
		return;
	}
	ud = (struct userdata *)obj;

	if(message->retain && ud->no_retain) return;
	if(ud->verbose){
		if(message->payloadlen){
			printf("[recv]%s %s\n", message->topic, (const char *)message->payload);
		}else{
			printf("%s (null)\n", message->topic);
		}
		fflush(stdout);
	}else{
		if(message->payloadlen){
			g_total_msg_counter++;
			ud->recv_msg_counter++;
			msg_send_time = get_time(message->payload);
			gettimeofday(&cur_time,NULL);
			cur_time_l = cur_time.tv_sec*1000000+cur_time.tv_usec;
			_time_interval = ((double)cur_time_l - msg_send_time)/1000;
			g_total_delay = g_total_delay + _time_interval;

			if(g_max_delay < _time_interval)
				g_max_delay =  _time_interval;

			if(g_min_delay > _time_interval || g_min_delay <0.000001)
				g_min_delay =  _time_interval;
			
			if(g_total_msg_counter%g_recv_print_speed == 0){
				printf("[<-R]id:[%s];msg:[%s];counter[%ld];delay[%.2f ms];max_delay[%.2f];min_delay[%.2f]\n",
					ud->usr_id, (const char *)message->payload,ud->recv_msg_counter,g_total_delay/g_recv_print_speed,g_max_delay,g_min_delay);
				fflush(stdout);
				g_total_delay = 0;
				g_max_delay = 0;
				g_min_delay;
				if(g_total_msg_counter>99999999999 || g_total_msg_counter <=0)
					g_total_msg_counter = 0;
			}
		}
	}
}

void my_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	if(NULL==mosq || NULL == obj){
		printf("[my_connect_callback] * error:NULL == obj \n");
		return;
	}

	int i;
	struct userdata *ud;

	ud = (struct userdata *)obj;
	int res = 0;
	if(!result){
		for(i=0; i<ud->topic_count; i++){
			res = mosquitto_subscribe(mosq, NULL, ud->topics[i], ud->topic_qos);
			printf("[my_connect_callback] client:%s subscribe topic: %s, res:%d\n",ud->usr_id,ud->topics[i],res);
		}
	}else{
		if(result && !ud->quiet){
			fprintf(stderr, "%s\n", mosquitto_connack_string(result));
		}
	}
}

void my_subscribe_callback(struct mosquitto *mosq, void *obj, int mid, int qos_count, const int *granted_qos)
{
	int i;
	struct userdata *ud;
	if(NULL == obj){
		printf("[my_subscribe_callback] error:NULL == obj \n");
		return;
	}
	ud = (struct userdata *)obj;
	ud->is_sub = true;
	if(!ud->quiet) printf("[my_subscribe_callback] client %s, socket %d Subscribes (mid: %d): %d;qos_count:%d",mosq->id,mosq->sock, mid, granted_qos[0],qos_count);
	for(i=1; i<qos_count; i++){
		if(!ud->quiet) printf(", %d", granted_qos[i]);
	}
	if(!ud->quiet) printf("\n");
}

void my_log_callback(struct mosquitto *mosq, void *obj, int level, const char *str)
{
	printf("[my_log_callback] : %s\n", str);
}

void print_usage(void)
{
	int major, minor, revision;

	mosquitto_lib_version(&major, &minor, &revision);
	
}

/*
*
*ret: 
*	-1: pointer is NULL
*	-2: client num is not bigger than 0   
*	-3: cannot get enough memory
*	-5: register socket into epoll fail
*	-8: parameters error
*	-9: connect user fail
*/

int reg_clients(struct userdata* _tmp_usr_data, char *pStrLocalVirtualIP)
{
	if(!_tmp_usr_data)
		return -1;
		
	struct mosquitto * _cur_context = NULL;
	_cur_context = mosquitto_new(_tmp_usr_data->usr_id, true, _tmp_usr_data);
	if(NULL == _cur_context ){
		printf("[reg_clients]: mosquitto_new fail...\n");
		free(_tmp_usr_data);
		return -1;
	}
	_cur_context->username = strdup(_tmp_usr_data->username);
	_cur_context->password = strdup(_tmp_usr_data->password);
	_tmp_usr_data->context = _cur_context;
	mosquitto_connect_callback_set(_cur_context, my_connect_callback);
	mosquitto_message_callback_set(_cur_context, my_message_callback);
	mosquitto_subscribe_callback_set(_cur_context, my_subscribe_callback);
	
	if(mosquitto_connect_bind(_cur_context, g_clients_db.server_host, g_clients_db.server_port, g_clients_db.keep_alive, pStrLocalVirtualIP)){//最后一个参数NULL->pStrLocalVirtualIP
		printf("[reg_clients] fail from function:mosquitto_connect_bind\n");
		mosquitto_lib_cleanup();
		return -1;
		
	}
	g_clients_db.usr_info_array[g_clients_db.used_buf_len] = _tmp_usr_data;
	g_clients_db.used_buf_len++;
	if(TC_SUC != connect_user(_tmp_usr_data))
		return -9;
	set_fd2c(_tmp_usr_data->context->sock,g_clients_db.used_buf_len-1);
	return TC_SUC;

}

int connect_user(struct userdata* _tmp_usr_data)
{
	if(_tmp_usr_data->context->state == mosq_cs_connect_async)
		mosquitto_reconnect(_tmp_usr_data->context);
	if(TC_SUC != reg_socket(_tmp_usr_data))
		return -5;
	//mosquitto_loop_write(_tmp_usr_data->context,1);
	return TC_SUC;
}

static void* handle_thread(void* _param)
{
	int event_num = 0;
	struct epoll_event* epoll_events = NULL;
	int event_buf_len = DEFAULT_COMMON_VALUE;

	epoll_events = (struct epoll_event*)malloc(sizeof(struct epoll_event)*event_buf_len);
	if(!epoll_events){
		printf("[handle_thread] Error: Out of memory. ---epoll_events\n");
		return (void*)0;
	}
	long last_t = time(NULL);
	long curtime_t =time(NULL);
	sleep(3);
	while(!g_clients_db.stop_handle_thread){
		curtime_t =time(NULL);
		if(event_num > event_buf_len - 100 ){
			event_buf_len = event_num > event_buf_len ? event_num*2 : event_buf_len*2;
			epoll_events = realloc(epoll_events, sizeof(struct epoll_event)*event_buf_len);
			if(!epoll_events){
				printf("[handle_thread]Error: _mosquitto_realloc. ---epoll_events\n");
				return (void*)0;
			}
		}
		pthread_mutex_lock (&g_epoll_id_lock);
		event_num = epoll_wait(g_clients_db.epoll_fd,epoll_events,event_buf_len,100);
		pthread_mutex_unlock(&g_epoll_id_lock);
		if(event_num == -1)
			printf("[handle_thread]error ,after epoll_wait %d\n",event_num);
		else
			handle_events_result(epoll_events, event_num);
		last_t = curtime_t;
		check_clients();
//		sleep(1);
	}
	printf("[handle_thread] return from handle_thread\n");
	return (void*)0;

}

void* thread_func(void* _param)
{
	printf("[thread_func]:will call func1()\n");

}

int join_handle_thread()
{
	int _ret = 0; 
	void* tret;
	_ret = pthread_join(g_clients_db.handle_thread_id,NULL); 

	if(_ret != 0){
		printf("[join_handle_thread]:fail pthread_create:handle_thread()\n");
		return -4;
	}
	return TC_SUC;
}

int create_handle_thread()
{
	int _ret = 0;
	_ret = pthread_create(&g_clients_db.handle_thread_id, NULL, handle_thread,NULL);
	if(_ret != 0){
		printf("[create_handle_thread]:fail pthread_create:handle_thread()\n");
		return -40;
	}
	g_clients_db.stop_handle_thread = false;

	return TC_SUC;
	
}


/*
* -7
*/
int reg_socket(struct userdata* _tmp_usr_data)
{
	if(!_tmp_usr_data || !_tmp_usr_data->context|| _tmp_usr_data->context->sock<=0 || _tmp_usr_data->reg_epoll){
		printf("[reg_socket] :parameters error(_tmp_usr_data:%p,_tmp_usr_data->context:%p,_tmp_usr_data->context->sock:%d,_tmp_usr_data->reg_epoll=%d)\n",_tmp_usr_data,_tmp_usr_data->context,_tmp_usr_data->context->sock,_tmp_usr_data->reg_epoll);
		return -8;
	}
//	printf("[reg_socket] reg socket:%d\n",_tmp_usr_data->context->sock);

	struct epoll_event _new_events;
	_new_events.data.fd = _tmp_usr_data->context->sock;
	_new_events.events = EPOLLIN ;

	pthread_mutex_lock (&g_epoll_id_lock);
	if(epoll_ctl(g_clients_db.epoll_fd, EPOLL_CTL_ADD,_new_events.data.fd,&_new_events) < 0){
		printf("[reg_socket] :error  socket(fd = %d) into epoll(fd = %d)\n",_tmp_usr_data->context->sock, g_clients_db.epoll_fd);
		pthread_mutex_unlock(&g_epoll_id_lock);
		return -5;
	}
	else{	
		pthread_mutex_unlock(&g_epoll_id_lock);
		_tmp_usr_data->reg_epoll = true;
		return TC_SUC;				
	}
}

/*
* we do not need this function, if socket is unuseful,it won't work in epoll
*/

int unreg_socket(struct userdata* _tmp_usr_data)
{

	/*printf("[unreg_socket] --------->\n");

	if(!_tmp_usr_data || !_tmp_usr_data->context|| _tmp_usr_data->context->sock<=0 || !_tmp_usr_data->reg_epoll){
		printf("[unreg_socket] :parameters error(_tmp_usr_data:%p,_tmp_usr_data->context:%p,_tmp_usr_data->context->sock:%d,_tmp_usr_data->reg_epoll=%d)\n",_tmp_usr_data,_tmp_usr_data->context,_tmp_usr_data->context->sock,_tmp_usr_data->reg_epoll);
		return -8;
	}
	printf("[unreg_socket] --p2\n");
	struct epoll_event _new_events;
	_new_events.data.fd = _tmp_usr_data->context->sock;
	_new_events.events = EPOLLIN | EPOLLOUT | EPOLLET;
	pthread_mutex_lock (&g_epoll_id_lock);
	printf("[unreg_socket] --p2\n");
	if(epoll_ctl(g_clients_db.epoll_fd,EPOLL_CTL_DEL,_new_events.data.fd,&_new_events) < 0){
		printf("[unreg_socket] :error  socket(fd = %d) into epoll(fd = %d)\n",_tmp_usr_data->context->sock, g_clients_db.epoll_fd);
		pthread_mutex_unlock(&g_epoll_id_lock);
		return -6;
	}
	else{
		pthread_mutex_unlock(&g_epoll_id_lock);
		_tmp_usr_data->reg_epoll = false;
		return TC_SUC;
	}*/
}


int init_db(struct t_client_config* p_config_data)
{
	if(NULL == p_config_data)
		return -1;
	memset(&g_clients_db, 0, sizeof(struct t_client_database));
	int client_num = p_config_data->end_id - p_config_data->start_id;
	if(client_num <=0){
		printf("[init_db] client_num <=0\n");
		return -10;
	}
	g_clients_db.start_id = p_config_data->start_id;
	g_clients_db.end_id = p_config_data->end_id;
	g_clients_db.handle_thread_id = -1;
	g_clients_db.stop_handle_thread = false;
	g_clients_db.used_buf_len = 0;
	g_clients_db.max_buf_len = client_num;
	
	g_clients_db.usr_info_array = malloc(client_num*sizeof(struct userdata *));
	if(NULL == g_clients_db.usr_info_array)
		return -3;
	memset(g_clients_db.usr_info_array, 0, client_num*sizeof(struct userdata *));

	g_clients_db.server_host = strdup(p_config_data->host);
	g_clients_db.server_port = p_config_data->port;
	g_clients_db.epoll_fd = epoll_create(1024);
	g_clients_db.handle_thread_id = -1;
	g_clients_db.is_inited = true;
	g_clients_db.keep_alive = p_config_data->keep_alive;

	return TC_SUC;
}

int uninit_data(struct t_client_config* p_config_data)
{
	if(g_clients_db.server_host){
		free(g_clients_db.server_host);
		g_clients_db.server_host = NULL;
	}
	if(g_clients_db.usr_info_array){
		free(g_clients_db.usr_info_array);
		g_clients_db.usr_info_array = NULL;
	}
	g_clients_db.is_inited = false;	
	return TC_SUC;
}

void handle_events_result(struct epoll_event* epoll_events, int _event_num)
{
	int max_packets = 1;
	int i;
	struct t_fd2client * _tmp_fd2c = NULL;
	struct mosquitto *_cur_context;
	int ires = 0;
	for(i=0; i<_event_num; i++)
	{
		if(-1 == epoll_events[i].data.fd) continue;
	
		_tmp_fd2c = get_fd2c(epoll_events[i].data.fd);
		if(!_tmp_fd2c || _tmp_fd2c->client_index<0
			|| _tmp_fd2c->client_index>=g_clients_db.max_buf_len
			|| !g_clients_db.usr_info_array[_tmp_fd2c->client_index])
			continue;
		_cur_context = g_clients_db.usr_info_array[_tmp_fd2c->client_index]->context;
		if(!_cur_context) continue;

	///handle read
		if(epoll_events[i].events & EPOLLIN){
			ires = mosquitto_loop_read(_cur_context,max_packets);
			if(ires){
				if(_cur_context->state != mosq_cs_disconnecting)
					printf("[handle_events_result]Socket read error on client %s, disconnecting.(socket=%d,ires=%d)\n", _cur_context->id,_cur_context->sock,ires);
				else
					printf("[handle_events_result]Socket read errorClient %s disconnected,ires=%d.\n",_cur_context->id,ires);
				g_clients_db.usr_info_array[_tmp_fd2c->client_index]->is_valid	= false;
			}
		}
	}
}

void check_clients()
{
	int i = 0;
	for(i=0; i<g_clients_db.max_buf_len; i++){
		if(NULL == g_clients_db.usr_info_array[i]){
			//printf("[check_clients] NULL == g_clients_db.usr_info_array[%d]\n",i);
			continue;
		}
		
		if(!g_clients_db.usr_info_array[i]->is_valid){
			printf("[check_clients] g_clients_db.usr_info_array[%d]->is_valid == false\n",i);
			unreg_socket(g_clients_db.usr_info_array[i]);
					
			clear_usr_data(g_clients_db.usr_info_array[i]);
			g_clients_db.usr_info_array[i] = NULL;			
		}
		else
			mosquitto_loop_misc(g_clients_db.usr_info_array[i]->context);
		
	}
}


void clear_usr_data(struct userdata* _usr_data)
{
	printf("[clear_usr_data]-->\n");
	if(NULL == _usr_data)
		return;
	if(_usr_data->context)
		mosquitto_destroy(_usr_data->context);
	int i;
	if(_usr_data->topics){
		for(i=0; i<_usr_data->topic_count; ++i)
			free(_usr_data->topics[i]);
	}
	if(_usr_data->password)
		free(_usr_data->password);
	if(_usr_data->username)
		free(_usr_data->username);
	if(_usr_data->usr_id)
		free(_usr_data->usr_id);
	free(_usr_data);
		printf("[clear_usr_data]<--\n");
}

void set_fd2c(int key_fd,int _client_index)
{

	if(key_fd <= 0 || _client_index <= 0)
		return;

	struct t_fd2client * _tmp_fd2c = NULL;
	HASH_FIND_INT(g_clients_db.ht_fd2c,&key_fd,_tmp_fd2c);

	if(!_tmp_fd2c){
		_tmp_fd2c = (struct t_fd2client *)malloc(sizeof(struct t_fd2client));	
		if(!_tmp_fd2c)
		return ;
		_tmp_fd2c->client_index= _client_index;
		_tmp_fd2c->fd = key_fd;
		HASH_ADD_INT(g_clients_db.ht_fd2c, fd, _tmp_fd2c);	
	}
	_tmp_fd2c->client_index= _client_index;

}

struct t_fd2client * get_fd2c(int key_fd)
{
	if(key_fd <= 0)
		return NULL;

	struct t_fd2client * _tmp_fd2c = NULL;
	HASH_FIND_INT(g_clients_db.ht_fd2c, &key_fd, _tmp_fd2c);
	return _tmp_fd2c;
}

void del_fd2c(int key_fd)
{
	if(key_fd <= 0)
			return ;

	struct t_fd2client * _tmp_fd2c = NULL;
	_tmp_fd2c = get_fd2c(key_fd);
	if(_tmp_fd2c){
		HASH_DEL(g_clients_db.ht_fd2c,_tmp_fd2c);
		free(_tmp_fd2c);
	}

}

void clear_fd2c() 
{
	struct t_fd2client *cur_f2c, *tmp;
	HASH_ITER(hh, g_clients_db.ht_fd2c, cur_f2c, tmp) {
		HASH_DEL(g_clients_db.ht_fd2c,cur_f2c);  
		free(cur_f2c);         
	}
}


struct userdata * create_usr(int _usr_id, struct t_client_config* p_client_cfg)
{
	if(NULL == p_client_cfg)	return NULL;
	struct userdata* _usr_data = NULL;
	_usr_data = malloc(sizeof(struct userdata));
	if(NULL == _usr_data ) return NULL;
	_usr_data->usr_id = malloc(sizeof(char)*125);//strdup(_usr_id);
	if(!_usr_data->usr_id){
		free(_usr_data);
		return NULL;
	}
	static int pwd_counter = PWD_USR_BEGIN;
	char pwd_usr_buf[128];
	memset(pwd_usr_buf,'\0',128);
	sprintf(pwd_usr_buf,"%d",pwd_counter);
	pwd_counter++;
	if(pwd_counter>PWD_USR_END)
		pwd_counter = PWD_USR_BEGIN;
	
	sprintf(_usr_data->usr_id, "%d", _usr_id);

	
	int i = 0;//data from p_client_cfg->_usr_data
	for (i = 0; i < MAX_VIRTUALIP_NUM; i++)
	{
		if (NULL == p_client_cfg->virtual_ip[i])
		{
			break;
		}
		_usr_data->virtual_ip[i] = strdup(p_client_cfg->virtual_ip[i]);
	}
	_usr_data->message_counter = 0;
	_usr_data->username = strdup(pwd_usr_buf);
	_usr_data->password = strdup(pwd_usr_buf);
	_usr_data->is_valid = true;
	_usr_data->reg_epoll = false;
	_usr_data->topics = NULL;
	_usr_data->topic_qos = 0;
	_usr_data->quiet = false;
	_usr_data->no_retain = true;
	_usr_data->topic_count = 1;
	_usr_data->verbose = 0;
	_usr_data->send_msg_counter = 0;
	_usr_data->recv_msg_counter = 0;
	_usr_data->is_sub = false;
	_usr_data->keepalive = p_client_cfg->keep_alive;
	_usr_data->topics = (char **)malloc(_usr_data->topic_count*sizeof(char **));
	if(NULL == _usr_data->topics){
		printf("[create_usr] 1. malloc fail for:id=%d \n",_usr_id);
		return NULL;
	}

	_usr_data->topics[0] = (char *)malloc( TC_MAX_TOPIC_LEN*sizeof(char *));
	if(NULL == _usr_data->topics){
		printf("[create_usr] 2. malloc fail for:id=%d \n",_usr_id);
		return NULL;
	}
	if(NULL != p_client_cfg->topic_prefix)
		sprintf(_usr_data->topics[0],"%s%d",p_client_cfg->topic_prefix,_usr_id);
	else
		sprintf(_usr_data->topics[0],"%d",_usr_id);
	return _usr_data;
}

int get_config(struct t_client_config* p_client_cfg)
{
	if(NULL == p_client_cfg)
		return -1;
	EnableLog(false);
	if(TC_SUC != cfg_Open(CONFIG_FILE_NAME)){
		printf("[get_config] open config: *%s *fail",CONFIG_FILE_NAME);
		return -100;
	}
	p_client_cfg->debug = true;

	char host_buf[MAX_HOST_BUF_LEN];
	if(!GetValue_str("COMMON", "host",host_buf)){
		printf("[get_config] get *host* fail from config file \n");
		return -100;
	}
	p_client_cfg->host = strdup(host_buf);

	int tmp_value;
	if(!GetValue_int("COMMON", "port",&tmp_value)){
		tmp_value = 1883;
		printf("[get_config] get *port* fail from config file, %d will be used as default port\n",tmp_value);
	}
	p_client_cfg->port = tmp_value;

	if(!GetValue_int("GROUP", "startid",&tmp_value)){
		printf("[get_config] get *startid* fail from config file\n");
		return -100;
	}
	p_client_cfg->start_id = tmp_value;

	if(!GetValue_int("GROUP", "endid",&tmp_value)){
		printf("[get_config] get *end_id* fail from config file\n");
		return -100;
	}
	p_client_cfg->end_id = tmp_value;
	
	if(!GetValue_int("GROUP", "keepalive",&tmp_value)){
		tmp_value = 60;
		printf("[get_config] get *keepalive* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->keep_alive = tmp_value;

	//get virtual IPs from config file. aiden150728
	char virtual_IP_buf[MAX_VIRTUALIP_BUF_LEN];
	bzero(virtual_IP_buf, sizeof(virtual_IP_buf));
	if (!GetValue_str("GROUP", "client_virtual_ip_group", virtual_IP_buf)) {
		printf("[get_config] get *client_virtual_ip_group* fail from config file \n");
		return -100;
	}
	char *result = NULL;
	char delims[] = ",";
	result = strtok(virtual_IP_buf, delims);
	int i = 0;
	for (i = 0; i < MAX_VIRTUALIP_NUM; i++)
	{
		if (NULL == result)
		{
			p_client_cfg->virtual_ip[i] = NULL;
			break;
		}
		p_client_cfg->virtual_ip[i] = strdup(result);		
		result = strtok(NULL, delims);
	}

	if(!GetValue_int("GROUP", "sleep_time_max",&tmp_value)){
		tmp_value = 3;
		printf("[get_config] get *sleep_time_max* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_time_max = tmp_value;

	if(!GetValue_int("GROUP", "sleep_time_mid",&tmp_value)){
		tmp_value = 2;
		printf("[get_config] get *sleep_time_mid* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_time_mid = tmp_value;

	if(!GetValue_int("GROUP", "sleep_time_min",&tmp_value)){
		tmp_value = 1;
		printf("[get_config] get *sleep_time_min* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_time_min= tmp_value;

	if(!GetValue_int("GROUP", "sleep_level1",&tmp_value)){
		tmp_value = 2000;
		printf("[get_config] get *sleep_level1* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_level1= tmp_value;

	if(!GetValue_int("GROUP", "sleep_level2",&tmp_value)){
		tmp_value = 5000;
		printf("[get_config] get *sleep_level2* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_level2= tmp_value;

	if(!GetValue_int("GROUP", "sleep_level3",&tmp_value)){
		tmp_value = 10000;
		printf("[get_config] get *sleep_level3* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_level3= tmp_value;

	if(!GetValue_int("GROUP", "sleep_interval_max",&tmp_value)){
		tmp_value = 100;
		printf("[get_config] get *sleep_interval_max* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_interval_max= tmp_value;

	if(!GetValue_int("GROUP", "sleep_interval_mid",&tmp_value)){
		tmp_value = 50;
		printf("[get_config] get *sleep_interval_mid* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_interval_mid= tmp_value;

	if(!GetValue_int("GROUP", "sleep_interval_min",&tmp_value)){
		tmp_value = 10;
		printf("[get_config] get *sleep_interval_min* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleep_interval_min= tmp_value;

	if(!GetValue_int("SEND", "batchSize",&tmp_value)){
		tmp_value = 50;
		printf("[get_config] get *batchSize* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->batchSize= tmp_value;
	
	if(!GetValue_int("SEND", "sleepAfterBatch",&tmp_value)){
		tmp_value = 1000*1000;
		printf("[get_config] get *sleepAfterBatch* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->sleepAfterBatch= tmp_value;//外面配的是us

	if(!GetValue_int("RECV", "recv_print_speed",&tmp_value)){
		tmp_value = 1000;
		printf("[get_config] get *recv_print_speed* fail from config file, will use %d as default value\n",tmp_value);
	}
	g_recv_print_speed= tmp_value;
	p_client_cfg->recv_print_speed = tmp_value;

	if(!GetValue_int("SEND", "send_print_speed",&tmp_value)){
		tmp_value = 1000;
		printf("[get_config] get *send_print_speed* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->send_print_speed = tmp_value;

	char tmp_buf[DEFAULT_COMMON_VALUE];
	memset(tmp_buf, '\0',DEFAULT_COMMON_VALUE);
	if(!GetValue_str("GROUP", "topic_prefix",tmp_buf)){
		printf("[get_config] get *topic_prefix* fail from config file \n");
		p_client_cfg->topic_prefix = NULL;
		return -100;
	}
	else{
		p_client_cfg->topic_prefix = strdup(tmp_buf);
		//printf("get topic prefix:[%s];tmp_buf[%s]\n",p_client_cfg->topic_prefix,tmp_buf);
	}
	
	if(!GetValue_int("SEND", "test_type",&tmp_value)){
		tmp_value = 0;
		printf("[get_config] get *use_send* fail from config file, will use %d as default value\n",tmp_value);
	}
	switch(tmp_value){
		case 0 :
			p_client_cfg->test_type= TEST_TYPE_ONLY_RECEIVE;
			break;
		case 1:
			p_client_cfg->test_type= TEST_TYPE_SEND_TO_ITSELF;
			break;
		case 2:
			p_client_cfg->test_type= TEST_TYPE_SEND_TO_OTHERS;
			break;
		default:
			p_client_cfg->test_type= TEST_TYPE_UNKNOWN_TYPE;
	}
	if(!GetValue_int("SEND", "send_context_id",&tmp_value)){
		tmp_value = p_client_cfg->start_id;
		printf("[get_config] get *use_send* fail from config file, will use %d as default value\n",tmp_value);
	}
	p_client_cfg->send_client_id = tmp_value;

	cfg_Close();
	return TC_SUC;
}
/*
int get_sleep_time(struct t_client_config* p_client_cfg)
{
	if(NULL == p_client_cfg)
		return 0;
	static int sleep_counter = 0;
	int sleep_time = 0;
	if(g_clients_db.used_buf_len <= p_client_cfg->sleep_level1){
		sleep_time = p_client_cfg->sleep_time_min;
		sleep_counter = ((g_clients_db.used_buf_len == p_client_cfg->sleep_level1) ? sleep_counter=0 : sleep_counter+1);
		return (sleep_counter%p_client_cfg->sleep_interval_max ==0) ? sleep_time : 0;
	}
	else
		if(g_clients_db.used_buf_len <= p_client_cfg->sleep_level2){
			sleep_time = p_client_cfg->sleep_time_min;
			sleep_counter = ((g_clients_db.used_buf_len == p_client_cfg->sleep_level2) ? sleep_counter=0 : sleep_counter+1);
			return (sleep_counter%p_client_cfg->sleep_interval_mid ==0) ? sleep_time : 0;
		}
		else
			if(g_clients_db.used_buf_len <= p_client_cfg->sleep_level3){
				sleep_time = p_client_cfg->sleep_time_mid;
				sleep_counter = ((g_clients_db.used_buf_len == p_client_cfg->sleep_level3) ? sleep_counter=0 : sleep_counter+1);
				return (sleep_counter%p_client_cfg->sleep_interval_mid ==0) ? sleep_time : 0;
			}
			else{			
				sleep_time = p_client_cfg->sleep_time_max;
				sleep_counter = sleep_counter+1;
				return (sleep_counter%p_client_cfg->sleep_interval_max ==0) ? sleep_time : 0;
			}

}
*/
int get_sleep_time(struct t_client_config* p_client_cfg)
{
	if (NULL == p_client_cfg)
		return 0;
	//client数是400的整数倍时，才往下走，否则直接返回0
	if (0 != g_clients_db.used_buf_len%400)
	{
		return 0;
	}

	int nSleepTime = 0;//单位us
	if (g_clients_db.used_buf_len <= p_client_cfg->sleep_level1)
	{
		nSleepTime = 1000 * p_client_cfg->sleep_interval_min;
	} 
	else if(g_clients_db.used_buf_len > p_client_cfg->sleep_level1 && g_clients_db.used_buf_len <= p_client_cfg->sleep_level2)
	{
		nSleepTime = 1000 * p_client_cfg->sleep_interval_mid;
	}
	else if (g_clients_db.used_buf_len > p_client_cfg->sleep_level2 && g_clients_db.used_buf_len <= p_client_cfg->sleep_level3)
	{
		nSleepTime = 1000 * p_client_cfg->sleep_interval_max;
	}
	else
	{
		nSleepTime = 1000 * (p_client_cfg->sleep_interval_max + 50);
	}
	return nSleepTime;
}


int join_send_thread()
{
	int _ret = 0; 
	void* tret;
	if(g_clients_db.send_thread_id <= 0)
		return TC_ERROR;
	_ret = pthread_join(g_clients_db.send_thread_id,NULL); 

	if(_ret != 0){
		printf("[join_send_thread]:fail pthread_create:send_thread()\n");
		return -4;
	}
	return TC_SUC;

}

int create_send_thread(struct t_client_config* cfg_data)
{
	if(NULL == cfg_data)
		return TC_ERROR;
	if(TEST_TYPE_ONLY_RECEIVE == cfg_data->test_type)
		return TC_SUC;
	int _ret = 0;
	g_clients_db.stop_send_thread = false;
	_ret = pthread_create(&g_clients_db.send_thread_id, NULL, send_thread,(void*)cfg_data);
	if(_ret != 0){
		printf("[create_send_thread]:fail pthread_create:send_thread()\n");
		g_clients_db.stop_send_thread = true;
		return -40;
	}

	return TC_SUC;
	
}

static void send_to_itself(struct t_client_config* p_client_cfg, int dest_index)
{
	pub(g_clients_db.usr_info_array[dest_index],NULL,p_client_cfg);
}


static void send_to_others(struct t_client_config* p_client_cfg, int dest_index)
{
	static struct userdata *send_mosq = NULL;
	if(NULL == send_mosq || !send_mosq->is_valid){
		send_mosq = get_send_client(p_client_cfg); 
		if(NULL == send_mosq || !send_mosq->is_valid)
			send_mosq = g_clients_db.usr_info_array[dest_index];
	}

	if(NULL != send_mosq && send_mosq->is_valid){
		pub(send_mosq,g_clients_db.usr_info_array[dest_index],p_client_cfg);
	}
	else
		printf("[send_to_others]can't find send client\n");
}

static struct userdata *get_send_client(struct t_client_config* p_client_cfg) 
{
	if(NULL == p_client_cfg){
		printf("[get_send_client]Pointer: p_client_cfg is NULL\n");
		return NULL;
	}
	int client_id = p_client_cfg->send_client_id;

	if(client_id>=p_client_cfg->end_id || client_id<p_client_cfg->start_id){
		printf("[get_send_client](client:%d) id beyond [start_id:%d,end_id:%d)\n",client_id,p_client_cfg->start_id,p_client_cfg->end_id);
		return NULL;
	}
	char buf_id[DEFAULT_COMMON_VALUE];
	sprintf(buf_id, "%d", client_id);
	int i=0;
	for(i=0; i<g_clients_db.used_buf_len; i++){
		if(g_clients_db.usr_info_array[i] && !strcmp(g_clients_db.usr_info_array[i]->context->id,buf_id) && g_clients_db.usr_info_array[i]->is_valid)
			return 	g_clients_db.usr_info_array[i];		
	}
	return NULL;
}

static void* send_thread(void* _param)
{
	struct t_client_config* p_client_cfg = (struct t_client_config*)_param;
	printf("\n[send_thread]  into send_thread()\n");
	struct userdata *cur_send_mosq = NULL;
	int cur_mosq_index = 0;
	long lSendCounter = 0;//用来实现每发送n个消息后，休息m微秒的功能
	//计时器，用来计算发送速度
	long lStartTime = 0;
	long lEndTime = 0;
	struct timeval t_start, t_end;
	bzero(&t_start, sizeof(t_start));
	bzero(&t_end, sizeof(t_end));
	gettimeofday(&t_start, NULL);
	lStartTime = ((long)t_start.tv_sec) * 1000 + (long)t_start.tv_usec / 1000;
	while(!g_clients_db.stop_send_thread){//预先设想为由外部Telnet来改变这个值的状态g_clients_db.stop_send_thread，即用外部命令来终止这个while循环，但是目前没有telnet的功能，所以是死循环
		
		switch(p_client_cfg->test_type){
			case TEST_TYPE_SEND_TO_ITSELF :
				send_to_itself(p_client_cfg,cur_mosq_index);
				break;
			case TEST_TYPE_SEND_TO_OTHERS :
				send_to_others(p_client_cfg,cur_mosq_index);
				break;
			default:
				usleep(1000);				
		}
		//每发送了batchSize个消息后，休息sleepAfterBatch微秒
		lSendCounter++;//发送信息累计计数+1
		if(0 == lSendCounter % p_client_cfg->batchSize)
		{
			usleep(p_client_cfg->sleepAfterBatch);
		}
		if (lSendCounter > 999999999)//防止溢出
		{
			lSendCounter = 0;
		}

		cur_mosq_index++;
		if (cur_mosq_index >= g_clients_db.used_buf_len)
		{
			cur_mosq_index = 0;
		}
		
		//计算发送消息的速度，单位 条/s。不是实时计算，而是每发送p_client_cfg->send_print_speed条消息后，才计算并打印一次log。求余时用lSendCounter比较好，不能用cur_mosq_index，它会周期性被重置为0，不准
		if(0 == lSendCounter % p_client_cfg->send_print_speed)
		{
			//获取结束时间，计算速度
			gettimeofday(&t_end, NULL);
			lEndTime = ((long)t_end.tv_sec) * 1000 + (long)t_end.tv_usec / 1000;
			long ti = lEndTime - lStartTime;
			if(ti > 0)
			{
				printf("\n[send_thread] current send speed = %ld\n", p_client_cfg->send_print_speed * 1000 / ti);
			}

			//更新起始时间
			gettimeofday(&t_start, NULL);
			lStartTime = ((long)t_start.tv_sec) * 1000 + (long)t_start.tv_usec / 1000;
		}
	}
}
/*
static void pub(struct userdata *mosq_info,struct t_client_config* p_client_cfg)
{
	if(NULL == mosq_info || !mosq_info->is_sub || NULL == p_client_cfg)
		return;
	static long send_counter = 0;
	struct	timeval cur_time;	
	gettimeofday(&cur_time,NULL);
	mosq_info->send_msg_counter++;
	int sleep_time = 1000*1000;
	int max_batchSize = 100;
	int print_speed = 100;
	if(NULL != p_client_cfg){
		sleep_time = p_client_cfg->sleepAfterBatch;
		max_batchSize = p_client_cfg->batchSize;
		print_speed = p_client_cfg->send_print_speed;
	}

	memset(g_SendMsgBuf, '\0', MAX_MSG_LEN);
	sprintf(g_SendMsgBuf,"<id:%s><c:%d><t:%ld>",mosq_info->usr_id, mosq_info->send_msg_counter,cur_time.tv_sec*1000000+cur_time.tv_usec);
	mosquitto_publish(mosq_info->context, NULL, mosq_info->topics[0], strlen(g_SendMsgBuf), g_SendMsgBuf, 0, 0);

	send_counter++;
	if(send_counter%max_batchSize == 0){
		usleep(sleep_time);
	}
	if(send_counter%print_speed == 0)
		printf("[S->]:%s\n",g_SendMsgBuf);
	if(send_counter > 999999999)
		send_counter = 0;
}
*/
static void pub(struct userdata *send_mosq_info, struct userdata *recv_mosq_info,struct t_client_config* p_client_cfg)
{
	//if(NULL == send_mosq_info || !send_mosq_info->is_sub || NULL == p_client_cfg){
	if(NULL == send_mosq_info || NULL == p_client_cfg){
		printf("[pub] parameter error:(send_mosq_info:%p)(p_client_cfg:%p)\n",send_mosq_info,p_client_cfg);
		return;
	}
	if(NULL == recv_mosq_info)
		recv_mosq_info = send_mosq_info;
	struct	timeval cur_time;	
	gettimeofday(&cur_time,NULL);
	send_mosq_info->send_msg_counter++;

	memset(g_SendMsgBuf, '\0', MAX_MSG_LEN);
	sprintf(g_SendMsgBuf,"<id:%s><c:%d><t:%ld>",send_mosq_info->usr_id, send_mosq_info->send_msg_counter,cur_time.tv_sec*1000000+cur_time.tv_usec);
	mosquitto_publish(send_mosq_info->context, NULL, recv_mosq_info->topics[0], strlen(g_SendMsgBuf), g_SendMsgBuf, 0, 0);

	//if(send_counter%print_speed == 0)
		//printf("[S->]:%s\n",g_SendMsgBuf);//20150813
}

int main()
{
	printf("****************************************************************\n");
	printf("*                                                              *\n");
	printf("*                         V1.0.3                               *\n");
	printf("*                                                              *\n");
	printf("****************************************************************\n");
	printf("press any key to continue...");
	char chAnyKey = getchar();
	int i;
	int ires = 0;
	static int reg_fail_num = 0;
	static int reg_success_num = 0;
	struct userdata *_cur_usr_data = NULL;
	mosquitto_lib_init();
	
	struct t_client_config client_cfg;
	memset(&client_cfg, 0, sizeof(struct t_client_config));
	printf("\n[main] 1. get configue data:\n");
	if(TC_SUC != get_config(&client_cfg)){
		printf("\n[main] get configue data fail\n");
		goto handle_over_pos;
	}
	i = 0;
	while(client_cfg.virtual_ip[i])
	{
		printf("\[main] ip[%d] = %s\n", i, client_cfg.virtual_ip[i]);
		i++;
	}
	printf("press any key to continue...");
	chAnyKey = getchar();

 	if(TEST_TYPE_UNKNOWN_TYPE == client_cfg.test_type){
		printf("\n[main] your \"test_type\" is unknown;0:only receive; 1: send to itself; 2:send to others;\n");
		goto handle_over_pos;
 	}
	
	printf("\n[main] 2. init_db:\n");
	ires = init_db(&client_cfg);
	if(TC_SUC != ires){
		printf("[main] init_db res =%d\n",ires);
		goto handle_over_pos;
	}

	printf("\n[main] 3. create_handle_thread:\n");
	ires = create_handle_thread();
	if(TC_SUC != ires){
		printf("[main] create_handle_thread res =%d\n",ires);
		goto handle_over_pos;
	}

	//多个虚拟ip，ip[0]~ip[n]，先用ip[0]来不停注册链接，端口是内核自己找空闲的来用。如果连续MAX_FAIL_NUM次都没有正常生成client，说明ip[0]的空闲端口已被压榨完毕。
	//此时，转到ip[1]，继续尝试生成client。如此循环，直到ip[n]也没有空闲port时，打印个log，说无法再生成新client了，然后break，继续运行程序其他步骤。
	printf("\n[main] 4. reg_clients:\n");
	int nStrategy = 2;//1：每个ip生成固定个数的client，随后切换ip；2：每个ip尽可能的压榨port，直到连续MAX_FAIL_NUM次生成失败之后，再切换ip
	int nRes = -1;
	int nLocalVirtualIPIdx = 0;
	char* pStrLocalVirtualIP = client_cfg.virtual_ip[nLocalVirtualIPIdx];
	for(i=client_cfg.start_id; i<client_cfg.end_id; ++i){
		_cur_usr_data = create_usr(i,&client_cfg);
		if(NULL == _cur_usr_data)
			continue;
		nRes = reg_clients(_cur_usr_data, pStrLocalVirtualIP);
		if (1 == nStrategy)
		{
			//策略1
			if (TC_SUC == nRes) {
				reg_success_num++;
				if (reg_success_num > 40000)
				{
					if (NULL == client_cfg.virtual_ip[++nLocalVirtualIPIdx])//是否有下一个备选虚拟ip
					{
						//没有备选ip，打印log，继续往下执行
						printf("[main] insufficient virtual ip, unable to create new user any more\n");
						break;
					}
					else
					{
						//有备选ip，则今后用这个新ip建立client，reg_success_num置零，continue开始下一次for循环
						pStrLocalVirtualIP = client_cfg.virtual_ip[nLocalVirtualIPIdx];
						reg_success_num = 0;
						continue;
					}
				}//判断reg_success_num结束
			}//判断nRes结束
		}//策略1结束
		else
		{
			//策略2
			if (TC_SUC != nRes) {
				printf("[main] fail from reg_clients(),user id = %d,total fail num is %d\n", i, reg_fail_num);
				if (reg_fail_num < MAX_FAIL_NUM) {
					reg_fail_num++;
					continue;
				}
				else
				{
					if (NULL == client_cfg.virtual_ip[++nLocalVirtualIPIdx])//是否有下一个备选虚拟ip
					{						
						//没有备选ip，打印log，继续往下执行
						printf("[main] total fail num is %d, won't create new user\n", reg_fail_num);
						break;
					}
					else
					{
						//有备选ip，则今后用这个新ip建立client，reg_fail_num置零，continue开始下一次for循环
						pStrLocalVirtualIP = client_cfg.virtual_ip[nLocalVirtualIPIdx];
						reg_fail_num = 0;
						continue;
					}
				}//判断reg_fail_num结束
			}//判断 nRes结束
		}//策略2结束
		printf("[main]   register clientid:(%d),usr:(%s) pwd:(%s) localip:(%s) over\n\n",i,_cur_usr_data->username,_cur_usr_data->password, pStrLocalVirtualIP);
		usleep(get_sleep_time(&client_cfg));
	}

	printf("\n[main] 5. create_send_thread:\n");
	ires = create_send_thread(&client_cfg);
	if(TC_SUC != ires){
		printf("[main] create_send_thread res =%d\n",ires);
		goto handle_over_pos;
	}
	
	printf("\n[main] 6. join thread:\n");
	join_handle_thread();
	join_send_thread();

handle_over_pos:
	uninit_data(&client_cfg);
	mosquitto_lib_cleanup();
	printf("[main] 7. test_over\n");
	return 0;
}



