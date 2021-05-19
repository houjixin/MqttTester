//cfgfile.h
#ifndef CFGFILE_H
#define CFGFILE_H
#include <stdio.h>  
#include <stdlib.h>
#include "mosquitto.h"
#include <string.h>


/****************************************************************************
*   author: jason.hou  
*   date:  2013-09-30
*   requarement: 
*		   1、配置文件的格式，按照段进行分割，以#作为行注释，配置的形式是key = value，
*          2、如果不指定段名，则所有的操作都在[DEFAULT]段内。
*          3、段的名字不超过50个字符，键的名字不超过50个字符，值的名字不超过128个字符         
*   example
*
*	[common]
*		host = 192.168.4.204
*		port = 3
*	[group1]
*		#id [100,200)
*		startid = 100
*		endid = 200
*		keepalive = 300
*		topic = 100 # if not supply ,topic == id
*		interval = 
*		sleep_time = 1
*
*****************************************************************************/

////////////////////////start :defination area of public function/////////

#ifdef __cplusplus
extern "C" {
#endif

int cfg_Open(char* pFileName/*IN*/);
void cfg_Close();
void SetValue_str(char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*IN*/);
void SetValue_int(char* pSec/*IN*/, char* pKey/*IN*/, int iValue/*IN*/);
void SetValue_double(char* pSec/*IN*/, char* pKey/*IN*/, double dValue/*IN*/);
bool GetValue_str(char* pSec/*IN*/, char* pKey/*IN*/, char* pValue/*OUT*/);
bool GetValue_int(char* pSec/*IN*/, char* pKey/*IN*/, int* pValue/*OUT*/);
bool GetValue_double(char* pSec/*IN*/, char* pKey/*IN*/, double dValue/*OUT*/);
void EnableLog(bool isDebug/*IN*/);

#ifdef __cplusplus
}
#endif

////////////////////////end :defination area of public function/////////
#endif //CFGFILE_H
