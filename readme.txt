**************************
*修改日期：2021-05-19
*修改者：houjix
**************************

一、功能说明：
	测试工具mosq_tester可以对mosquitto服务器进行如下测试：
	1）连接测试，该测试模式下测试工具可向mosquitto发起多个连接，连接的个数可由参数配置；
	2）自收发消息测试，该模式下，测试工具的每个连接都可以自己给自己发送消息，
	测试工具与mosquitto的连接数可以由参数配置，并且消息发送的速度也可由参数控制；
	3）一对多测试，该模式下，测试工具可以选用其中一个连接给其他连接发送消息，
	测试工具与mosquitto的连接数可由参数配置，且消息发送速度也可由参数控制；发送连接的id可以由参数指定，也可由测试工具自己选择；
	
二、编译说明：
	直接进入mosq_tester目录下，使用命令：make 即可直接编译，另外还可使用make clean清楚编译生成的所有目标文件。
	
三、使用方法及参数说明：
		mosq_tester的测试参数都存放在mosq_tester目录下的mosq_tester.conf文件里，配置参数以分段方式组合，共有COMMON、GROUP、RECV、SEND四个段，在实际使用过程中，不能改变参数所属的段
	否则程序将无法找到，在程序中获取参数也必须提供参数所属的段名和参数名，才能获取到对应参数的值。
		配置文件中，各参数的含义如下：
	1、COMMON段
		1）host表示mosquitto服务器的ip地址
		2）port表示mosquitto服务器的port地址
	2、GROUP段
		1）startid 客户端连接所使用的起始id
		2）endid 客户端连接所使用的结束id
		[备注]目前该测试工具所用的id都是纯数据，id使用的范围是[startid,endid)，客户端的连接数也是由这两个参数进行控制，为：endid-startid。
		例如，如果要向mosquitto压100000个连接，则可以如下配置：
			startid = 0
			endid = 100000
		3）keepalive客户端所使用的keepalive时间。
		4）针对每个client，内置了接收topic组织方式为：{topic_suffix}/{clientId}/{topic_suffix},其中：{clientId}将会替换为实际的clientId;topic_suffix和topic_suffix由配置文件中对应参数配置,表示客户端所订阅的topic前缀和后缀
		5）msg表示消息发送内容：测试工具内部会给所配置的内容加上前缀<t:{发送时间}>，该时间用于统计消息收发之间的延时。
		
		6）下列参数主要用于控制连接建立的速度：
		a）sleep_time_max = 3
		b）sleep_time_mid = 2
		c）sleep_time_min = 1
		d）sleep_level1 = 20000
		e）sleep_level2 = 40000
		f）sleep_level3 = 50000
		[备注] 上述三个参数需满足条件：sleep_level1 <= sleep_level2 <= sleep_level3 <= (endid - startid)
		g）sleep_interval_max = 1500
		h）sleep_interval_mid = 1000
		i）sleep_interval_min = 50
		参数5）~13）按照下列方式控制连接建立的速度：
		[1]当客户端连接数小于sleep_level1时，每建立sleep_interval_max个连接休息sleep_time_min秒，否则转[2];
		[2]当客户端连接数小于sleep_level2时，每建立sleep_interval_mid个连接休息sleep_time_min秒，否则转[3];
		[3]当客户端连接数小于sleep_level3时，每建立sleep_interval_mid个连接休息sleep_time_mid秒，否则转[4];
		[4]每建立sleep_interval_max个连接休息sleep_time_max秒。
	
	3、RECV段
		1）recv_print_speed 该参数表示测试工具每收到recv_print_speed条消息就打印一条消息日志；
		
	4、SEND段
		1）test_type 测试类型，0:只接受不发送; 1: 每个连接都发给自己; 2:一发多收，发送的连接给除自己之外的其他连接进行发送；
		2）send_print_speed 表示打印发送日志的频率，即每发送send_print_speed条消息打印一条消息发送的日志；
		3）sleepAfterBatch  表示每批消息发送完毕后，线程睡眠的时间,单位：毫秒；
		4）send_context_id	表示一对多发送时，发送连接的id，如果该参数不在[startid,endid)内，则测试工具自动选择第一个作为发送连接。



