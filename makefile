EXE_FILE=sub_client

OBJS=mosquitto.o \
	logging_mosq.o\
	messages_mosq.o\
	memory_mosq.o\
	net_mosq.o\
	read_handle.o\
	read_handle_client.o\
	read_handle_shared.o\
	send_mosq.o\
	send_client_mosq.o\
	thread_mosq.o\
	time_mosq.o\
	tls_mosq.o\
	util_mosq.o\
	will_mosq.o\
	test_client.o\
	cfgfile.o
	
$(EXE_FILE):$(OBJS)
	gcc $(OBJS) -o $(EXE_FILE)  -lpthread -ldl
	
test_client.o : test_client.c test_client.h  
	gcc -c test_client.c 
	
mosquitto.o : mosquitto.c mosquitto.h net_mosq.h
	gcc  -c mosquitto.c
	
logging_mosq.o : logging_mosq.c logging_mosq.h
	gcc  -c logging_mosq.c
	
messages_mosq.o : messages_mosq.c messages_mosq.h
	gcc -c messages_mosq.c	

memory_mosq.o : memory_mosq.c memory_mosq.h
	gcc -c memory_mosq.c

net_mosq.o : net_mosq.c net_mosq.h
	gcc  -c net_mosq.c

read_handle.o : read_handle.c read_handle.h
	gcc -c read_handle.c

read_handle_client.o : read_handle_client.c read_handle.h
	gcc -c read_handle_client.c

read_handle_shared.o : read_handle_shared.c read_handle.h
	gcc -c read_handle_shared.c

send_mosq.o : send_mosq.c send_mosq.h
	gcc -c send_mosq.c

send_client_mosq.o : send_client_mosq.c send_mosq.h
	gcc -c send_client_mosq.c

thread_mosq.o : thread_mosq.c
	gcc -c thread_mosq.c

time_mosq.o : time_mosq.c
	gcc -c time_mosq.c

tls_mosq.o : tls_mosq.c
	gcc -c tls_mosq.c

util_mosq.o : util_mosq.c util_mosq.h
	gcc -c util_mosq.c

will_mosq.o : will_mosq.c will_mosq.h
	gcc -c will_mosq.c
	
cfgfile.o : cfgfile.c cfgfile.h
	gcc -c cfgfile.c
	
.PHONY:clean
clean:
	-rm $(EXE_FILE) $(OBJS)
