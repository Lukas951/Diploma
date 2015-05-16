#include <pthread.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include <stdio.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/dmx.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <time.h>
#include <stdint.h>
#include <fstream>

#include <libucsi/mpeg/descriptor.h>
#include <libucsi/mpeg/section.h>
#include <libucsi/dvb/descriptor.h>
#include <libucsi/dvb/section.h>
#include <libucsi/atsc/descriptor.h>
#include <libucsi/atsc/section.h>
#include <libucsi/transport_packet.h>
#include <libucsi/section_buf.h>
#include <libucsi/dvb/types.h>
#include <libdvbapi/dvbdemux.h>
#include <libdvbapi/dvbfe.h>
#include <libdvbcfg/dvbcfg_zapchannel.h>
#include <libdvbsec/dvbsec_api.h>
#include <libdvbsec/dvbsec_cfg.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <iostream>

#include <list>
#include <sstream>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;


class channel{
    int service_id;
    string provider_name;
    string service_name;

public:
    channel();

    void set_service_id(int id);
    void set_provider_name(string name);
    void set_service_name(string name);

    int get_service_id();
    string get_provider_name();
    string get_service_name();

};

channel::channel(){
    service_id = 0;
    provider_name = "";
    service_name = "";
}
void channel::set_service_id(int id){
    service_id = id;
}
void channel::set_provider_name(string name){
    provider_name=name;
}
void channel::set_service_name(string name){
    service_name=name;
}
int channel::get_service_id(){
    return service_id;
}
string channel::get_provider_name(){
    return provider_name;
}
string channel::get_service_name(){
    return service_name;
}

class event_data{
    int service_id;
    int event_id;
    int duration;
    int running_status;
    //int free_ca_mode;
    int start_time;

    string language_code;
    string event_name;
    string short_text;

    string extended_text;

    string extended_text2;

public:
    event_data();

    void set_service_id(int id);
    void set_event_id(int id);
    void set_duration(int d);
    void set_running_status(int status);
    void set_start_time(int time);
    void set_language_code(string code);
    void set_event_name(string name);
    void set_short_text(string text);
    void set_extended_text(string text);
    void set_extended_text2(string text);

    int get_service_id();
    int get_event_id();
    int get_duration();
    int get_running_status();
    int get_start_time();
    string get_language_code();
    string get_event_name();
    string get_short_text();
    string get_extended_text();
    string get_extended_text2();
};

event_data::event_data(){
    service_id = 0;
    event_id = 0;
    duration = 0;
    running_status = 0;
    //free_ca_mode = 0;
    start_time = 0;

    language_code = "";
    event_name = "";
    short_text = "";

    extended_text = "";
    extended_text2 = "";
}
void event_data::set_service_id(int id){
    service_id=id;
}
void event_data::set_event_id(int id){
    event_id=id;
}
void event_data::set_duration(int d){
    duration=d;
}
void event_data::set_running_status(int status){
    running_status=status;
}
void event_data::set_start_time(int time){
    start_time=time;
}
void event_data::set_language_code(string code){
    language_code=code;
}
void event_data::set_event_name(string name){
    event_name=name;
}
void event_data::set_short_text(string text){
    short_text+=text;
}
void event_data::set_extended_text(string text){
    extended_text+=text;
}

void event_data::set_extended_text2(string text){
    extended_text2+="|";
    extended_text2+=text;
}
int event_data::get_service_id(){
    return service_id;
}
int event_data::get_event_id(){
    return event_id;
}
int event_data::get_duration(){
    return duration;
}
int event_data::get_running_status(){
    return running_status;
}
int event_data::get_start_time(){
    return start_time;
}
string event_data::get_language_code(){
    return language_code;
}
string event_data::get_event_name(){
    return event_name;
}
string event_data::get_short_text(){
    return short_text;
}
string event_data::get_extended_text(){
    return extended_text;
}

string event_data::get_extended_text2(){
    return extended_text2;
}

struct lnb_types_st {
	string	name;
	unsigned long	low_val;
	unsigned long	high_val;	/* zero indicates no hiband */
	unsigned long	switch_val;	/* zero indicates no hiband */
};




int read_data(int defd, __u8 * data, int size_data ,int pid);
void parse_channels(uint8_t *buf, int len, list<channel> &channels);
void parse_events(uint8_t *buf, int len,  list<event_data> &events);
void parse_channel_descriptor(struct descriptor *d,  channel &C);
void parse_event_descriptor(struct descriptor *d,  event_data &E);
string ToString(uint8_t l, uint8_t* s);
void ToXml(char* path, list<channel> ch, list<event_data> e);
void AddIntoEventList(list<event_data> &events, event_data e);
void AddIntoChannelList(list<channel> &channels, channel c);
bool SortCompare(event_data &a, event_data &b);
int channels_cb(struct dvbcfg_zapchannel *channel, void *priv);
int tune(int fe_fd, dvb_frontend_parameters p);
void *readDataThread(void *d);
void diseqc_send_msg(int fd, fe_sec_voltage_t v, struct dvb_diseqc_master_cmd *cmd, fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b);
int diseqc(int secfd, int sat_no, int pol_vert, int hi_band);
