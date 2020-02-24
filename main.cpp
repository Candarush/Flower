#include <iostream>
#include <vector>
#include <cmath>
#include <ctime>
#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include "SolTrack.h"
#include "SolTrack.c"
#include "MyMQTTWrapper.h"

namespace FlowerLamp
{
    class Vector3
    {
        public:
        double x, y, z;

        Vector3(double ix, double iy, double iz)
        {
            x = ix;
            y = iy;
            z = iz;
        }
        Vector3()
        {
            x = 0;
            y = 0;
            z = 0;
        }

        Vector3 operator -(Vector3 b)
        {
            return Vector3(x - b.x, y - b.y, z - b.z);
        }

        Vector3 operator *(double l)
        {
            return Vector3(x * l, y * l, z * l);
        }

        static double Length(Vector3 v)
        {
            return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }

        static double Dot(Vector3 a, Vector3 b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        static Vector3 Normalize(Vector3 a)
        {
            double length = Length(a);
            return Vector3(a.x / length, a.y / length, a.z / length);
        }

        static double Angle(Vector3 a, Vector3 b)
        {
            return Dot(a, b) / (Length(a) * Length(b));
        }

        static Vector3 FromAltAzi(double altitude, double azimuth)
        {
            const double Deg2Rad = M_PI / 180.0;
            double z = sin(altitude*Deg2Rad);
            double hyp = cos(altitude*Deg2Rad);
            double y = hyp * cos(azimuth*Deg2Rad);
            double x = hyp * sin(azimuth*Deg2Rad);
            return Vector3(x, y, z);
        }
    };

    class Sun
    {
        public:
            double altitude, azimuth;
            Vector3 direction;

        public:
        void CalculateSunPosition(time_t rawtime, double latitude, double longitude)
        {
            Time time;
            tm* tmp = gmtime(&rawtime);
            time.year = tmp->tm_year+1900;
            time.month = tmp->tm_mon+1;
            time.day = tmp->tm_mday+1;
            time.hour = tmp->tm_hour-3;
            time.minute = tmp->tm_min;
            time.second = tmp->tm_sec;
            Location sunLoc;
            sunLoc.latitude = latitude;
            sunLoc.longitude = longitude;
            Position outPos;
            SolTrack(time, sunLoc, &outPos,  1,1,1,0);
            
            altitude = outPos.altitude;
            azimuth = outPos.azimuthRefract;
            direction = Vector3::FromAltAzi(altitude,azimuth);
        }
    };


    class Cloud
    {
        public:
            double latitude;
            double longitude;
            double x_width, y_width, height;

        Cloud()
        {
            latitude = 0;
            longitude = 0;
            height = 0;
            x_width = 0;
            y_width = 0;
        }
        
        Cloud(double ilongitude, double ilatitude, double ix_width, double iy_width, double iheight)
        {
            latitude = ilatitude;
            longitude = ilongitude;
            height = iheight;
            x_width = ix_width;
            y_width = iy_width;
        }

        void Move(double ilongitude, double ilatitude)
        {
            latitude += ilatitude;
            longitude += ilongitude;
        }

        Vector3 GetShadowOffset(Sun* sun)
        {
            double distanceToCloud = (height) / sin(sun->altitude);
            Vector3 vecToCloud = ( Vector3::Normalize(sun->direction)) * distanceToCloud;
            Vector3 offset = Vector3(-vecToCloud.x, -vecToCloud.y,0);
            return offset;
        }
    };

    class Lamp
    {
        private:
            bool isOn;
        
        public:
            double latitude;
            double longitude;
        
        Lamp()
        {
            latitude = 0;
            longitude = 0;
        }
        
        Lamp(double ilatitude, double ilongitude)
        {
            latitude = ilatitude;
            longitude = ilongitude;
        }

        bool IsUnderShadow(Cloud cloud, Sun* sun)
        {
            Vector3 shadowOffset = cloud.GetShadowOffset(sun);

            if (abs(longitude - (cloud.longitude + shadowOffset.x)) <= cloud.x_width / 2 && abs(latitude - (cloud.latitude + shadowOffset.y)) <= cloud.y_width / 2)
            {
                return true;
            }

            return false;
        }

        void Update(time_t dateTime, Sun* sun, std::vector<Cloud*> clouds)
        {
            sun->CalculateSunPosition(dateTime, latitude, longitude);

            for(std::vector<Cloud*>::iterator it = clouds.begin(); it != clouds.end(); ++it)
            {
                if (sun->altitude > 0 && IsUnderShadow(*(*it), sun))
                {
                    isOn = true;
                    return;
                }
            }

            if (sun->altitude > 0)
            {
                isOn = false;
                return;
            }

            isOn = true;
        }

        bool GetState()
        {
            return isOn;
        }

        void PrintState(time_t dateTime, Sun* sun, std::vector<Cloud*> clouds)
        {
            Update(dateTime, sun, clouds);
            std::cout<<"Лампа работает: "<< isOn << std::endl;
        }
    };

    static void ManualInput(Lamp* lamp, Cloud* cloud)
    {
        std::cout<< "Широта лампы: " << std::endl;
        std::cin>>lamp->latitude;
        std::cout<< "Долгота лампы: " << std::endl;
        std::cin>>lamp->longitude;
        std::cout<< "Широта облака: " << std::endl;
        std::cin>>cloud->latitude;
        std::cout<< "Долгота облака: " << std::endl;
        std::cin>>cloud->longitude;
        std::cout<< "Ширина облака по широте: " << std::endl;
        std::cin>>cloud->x_width;
        std::cout<< "Ширина облака по долготе: " << std::endl;
        std::cin>>cloud->y_width;
        std::cout<< "Высота облака: " << std::endl;
        std::cin>>cloud->height;
    }

    static void HardCodedInput(Lamp* lamp, Cloud* cloud)
    {
        lamp->longitude = 56;
        lamp->latitude = 37;
        cloud->longitude = 50;
        cloud->latitude = -10;
        cloud->x_width = 20;
        cloud->y_width = 30;
        cloud->height = 10;
    }

    void WriteGraphicsData(Lamp* lamp, std::vector<Cloud*> clouds, Sun* sun)
    {
        std::ofstream fs;
        fs.open("graphicsdata.d");
        fs << "#X  Y  Z" << std::endl;
        fs << "#Flower" << std::endl;
        fs << lamp->longitude << " " << lamp->latitude << " 0.0"<<std::endl;
        fs << lamp->longitude << " " << lamp->latitude << " 2.0"<<std::endl;
        fs << std::endl;
        
        fs << "#SunVector" << std::endl;
        fs << lamp->longitude << " " << lamp->latitude << " 0.0"<<std::endl;
        fs << lamp->longitude + sun->direction.x*50 << " " << lamp->latitude+sun->direction.y*50 << " " << sun->direction.z*50 <<std::endl;
        fs << std::endl;
        
        for(std::vector<Cloud*>::iterator it = clouds.begin(); it != clouds.end(); ++it) {
            fs << "#Cloud" << std::endl;
            fs << (*it)->longitude - (*it)->x_width/2 << " " << (*it)->latitude - (*it)->y_width/2 << " " << (*it)-> height <<std::endl;
            fs << (*it)->longitude + (*it)->x_width/2 << " " << (*it)->latitude - (*it)->y_width/2 << " " << (*it)-> height <<std::endl;
            fs << (*it)->longitude + (*it)->x_width/2 << " " << (*it)->latitude + (*it)->y_width/2 << " " << (*it)-> height <<std::endl;
            fs << (*it)->longitude - (*it)->x_width/2 << " " << (*it)->latitude + (*it)->y_width/2 << " " << (*it)-> height <<std::endl;
            fs << (*it)->longitude - (*it)->x_width/2 << " " << (*it)->latitude - (*it)->y_width/2 << " " << (*it)-> height <<std::endl;
            fs << std::endl;
        }
        
        fs.close();
    }

    void MQTTloop1()
    {
        while (1)
        {
            if (mymessages.size()>0)
            {
                std::cout<<mymessages.back()<<std::endl;
                mymessages.pop_back();
            }
                
        }
    }
    
    void MQTTloop2(MyMqttWrapper* MyMQTT)
    {
        MyMQTT->loop_forever();
    }

    void InitMQTT(MyMqttWrapper* MyMQTT)
    {
        mosqpp::lib_init();
        std::thread t1 = std::thread(MQTTloop1);
        std::thread t2 = std::thread(MQTTloop2, MyMQTT);
        t1.detach();
        t2.detach();
    }

    void SendData(Lamp* lamp, Sun* sun, MyMqttWrapper * MQTT )
    {
        std::string sunAlt = "Высота столнца: " + std::to_string(sun->altitude);
        std::string sunAzi = "Азимут солнца: " + std::to_string(sun->azimuth);
        std::string lampState = "Лампа работает: " + std::to_string(lamp->GetState());
        MQTT->send_message(sunAlt.c_str());
        MQTT->send_message(sunAzi.c_str());
        MQTT->send_message(lampState.c_str());
        
    }

    void Exit()
    {
        std::string line;
        while (std::cin >> line && line != "exit")
        {
            //repeat forever
        }
    }
}

using namespace FlowerLamp;

int main()
{
    time_t time = std::time(0);
    
    Sun* sun = new Sun();
    Lamp* lamp = new Lamp;
    std::vector<Cloud*> clouds;
    
    clouds.push_back(new Cloud);
    
    HardCodedInput(lamp, clouds[0]);
    
    lamp->Update(time, sun, clouds);
    
    WriteGraphicsData(lamp, clouds, sun);
    
    MyMqttWrapper* MyMQTT = new MyMqttWrapper("flower1", "test.mosquitto.org", 1883);
    InitMQTT(MyMQTT);
    
    SendData(lamp, sun, MyMQTT);
    
    Exit();
    
    delete MyMQTT;
    delete lamp;
    clouds.clear();
}
