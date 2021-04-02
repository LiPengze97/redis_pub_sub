#include "redis_publisher.h"
#include <fstream>
#include <iostream>
#include <string.h>
#include <vector>
using namespace std;

string chatting_split(string str)
{
    string res = "";
    if (str == "")
        return res;
    size_t pos = str.find(" ", 0);
    if (pos != str.npos)
    {
        string tmp = str.substr(pos + 1, str.size());
        pos = tmp.find(" ", 0);
        if (pos != tmp.npos)
        {
            res = tmp.substr(pos + 1, str.size());
        }
    }
    return res;
}

int main(int argc, char *argv[])
{
    CRedisPublisher publisher;

    bool ret = publisher.init();
    if (!ret)
    {
        printf("Init failed.\n");
        return 0;
    }

    ret = publisher.connect();
    if (!ret)
    {
        printf("connect failed.");
        return 0;
    }
    printf("Press enter to start publish message!");
    getchar();
    ifstream infile;
    infile.open("/users/pzl97/angularjs.txt");

    string s;
    int a = 0;
    while (getline(infile, s))
    {
        // cout << chatting_split(s) << endl;
        publisher.publish("test-channel", s);
        sleep(1);
        if (a++ > 50)
        {
            
            break;
        }
    }
    infile.close();
    // while (true)
    // {
    //     publisher.publish("test-channel", "Hello shiyanlou!");
    //     sleep(1);
    // }

    publisher.disconnect();
    publisher.uninit();
    return 0;
}