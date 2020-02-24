#include "/usr/local/include/mosquittopp.h"
#include "MyMQTTWrapper.h"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <locale>
#include <vector>
#include <string>


using namespace std;

vector<string> mymessages;

MyMqttWrapper::MyMqttWrapper(const char *id, const char *host, int port) : mosquittopp(id)
{
    int keepalive = 60;
    connect(host, port, keepalive);
    printf("Подключение успешно.\n");
};

void MyMqttWrapper::on_connect(int rc)
{
    if (rc == 0)
    {
        subscribe(NULL, "M30-212B-18/FlowerLamp");
    }
}

void MyMqttWrapper::on_message(const struct mosquitto_message *message){
    setlocale(LC_CTYPE, "rus");
    if(!strcmp(message->topic, "M30-212B-18/FlowerLamp")){
        char buffer[50];
        sprintf(buffer,"%s",message->payload);
        string msg = buffer;
        mymessages.push_back(msg);
    }
};

bool MyMqttWrapper::send_message(const  char * message)
{
    int ret = publish(NULL,"M30-212B-18/FlowerLamp",(int)strlen(message),message,2,false);
    return ( ret == MOSQ_ERR_SUCCESS );
}
