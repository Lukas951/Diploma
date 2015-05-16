#include "epgread.h"



#define DEMUXDEVICE "/dev/dvb/adapter%d/demux%d"
#define FRONTENDDEVICE "/dev/dvb/adapter%d/frontend%d"
#define TS_PACKET_SIZE 188

#define TS_BUF_SIZE   (256 * 1024)       /* default DMX_Buffer Size for TS */

#define PID_EIT 18
#define PID_SDT 17


list<channel> channels;
list<event_data> events;

char dedev[128];
char frdev[128];

struct lnb_types_st lnb_type;


struct lnb_types_st lnbs[] = {
    {"UNIVERSAL",	9750, 10600, 11700 },
    {"DBS",		11250, 0, 0 },
    {"STANDARD",	10000, 0, 0 },
    {"ENHANCED",	9750, 0, 0 },
    {"C-BAND",	5150, 0, 0 }
};


int main( int argc, char *argv[] ){

    pid_t pid, sid;

    pid = fork();
    if (pid < 0) {
            exit(EXIT_FAILURE);
    }

    if (pid > 0) {
            exit(EXIT_SUCCESS);
    }

    umask(0);

    openlog("EPGREAD_LOG:", LOG_PID|LOG_CONS, LOG_USER);

    sid = setsid();
    if (sid < 0) {
            syslog(LOG_INFO, "SID fail");
            exit(EXIT_FAILURE);
    }


    if ((chdir("/")) < 0) {
            /* Log the failure */
             syslog(LOG_INFO, "chdir fail");
            exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);


    int adapter = 0;
    int frontend = 0;
    int demux = 0;
    int opt;
    char channelsfile[128] = "/tmp/channels.conf";
    int lnb = 0;

    char xmlPath[128] = "/tmp/XML/";


    while ((opt = getopt(argc, argv, "a:f:d:c:l:t:")) != -1) {
        switch (opt) {
        case 'a':
            adapter = strtoul(optarg, NULL, 0);
            break;
        case 'f':
            frontend = strtoul(optarg, NULL, 0);
            break;
        case 'd':
            demux = strtoul(optarg, NULL, 0);
            break;
        case 'c':
            strncpy(channelsfile, optarg, sizeof(channelsfile));
            break;
        case 'l':
            lnb = strtoul(optarg, NULL, 0);
            break;
        case 't':
            strncpy(xmlPath, optarg, sizeof(xmlPath));
            break;
        }
    }
    if(lnb < 5){
        lnb_type = lnbs[lnb];
    }
    else {
        syslog(LOG_INFO, "Bad lnb number: %d", lnb );
        return -1;
    }


    snprintf(dedev, sizeof(dedev), DEMUXDEVICE, adapter, demux);
    snprintf(frdev, sizeof(frdev), FRONTENDDEVICE, adapter, frontend);






    FILE *fchannels = fopen(channelsfile, "r");
    if (fchannels == NULL) {

        syslog(LOG_INFO, "Unable to open %s\n", channelsfile);
        exit(1);
    }

    while (1) {

        dvbcfg_zapchannel_parse(fchannels, channels_cb, NULL);

        events.sort(SortCompare);

        ToXml(xmlPath, channels, events);

        sleep(10); /* wait 10 seconds */

        fseek(fchannels, 0, SEEK_SET);
    }


    fclose(fchannels);
    exit(EXIT_SUCCESS);

}



int read_data(int defd, __u8 * data, int size_data ,int pid){

    long dmx_buffer_size = TS_BUF_SIZE;

    if( ioctl(defd,DMX_SET_BUFFER_SIZE,dmx_buffer_size) < 0){
        syslog(LOG_INFO, "set demux buffer failed");
        return 0;
    }

    struct dmx_sct_filter_params    sctFilterParams;
    memset(&sctFilterParams, 0, sizeof(struct dmx_sct_filter_params));
    sctFilterParams.pid=pid;
    sctFilterParams.timeout=3000;
    sctFilterParams.flags=DMX_IMMEDIATE_START|DMX_CHECK_CRC;

    if( ioctl(defd,DMX_SET_FILTER,&sctFilterParams) < 0){
        syslog(LOG_INFO, "set demux filter failed");
        return 0;
    }

    int len = read(defd,data,size_data);

    return len;
}

void parse_events(uint8_t *buf, int len, list<event_data> &events)
{
    struct section *section;
    struct section_ext *section_ext = NULL;

    if ((section = section_codec(buf, len)) == NULL) {
        return;
    }

    switch(section->table_id) {

    case stag_dvb_event_information_nownext_actual:
    case stag_dvb_event_information_nownext_other:
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
    case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
    case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
    case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
    {

        struct dvb_eit_section *eit;
        struct dvb_eit_event *cur_event;
        struct descriptor *curd;
        time_t start_time;

        if ((section_ext = section_ext_decode(section, 1)) == NULL) {
            return;
        }

        if ((eit = dvb_eit_section_codec(section_ext)) == NULL) {
            return;
        }

        dvb_eit_section_events_for_each(eit, cur_event) {

            event_data E = event_data();


            start_time = dvbdate_to_unixtime(cur_event->start_time);

            E.set_service_id(dvb_eit_section_service_id(eit));
            E.set_event_id(cur_event->event_id);
            E.set_duration(dvbduration_to_seconds(cur_event->duration));
            E.set_running_status(cur_event->running_status);
            E.set_start_time((int) start_time);

            dvb_eit_event_descriptors_for_each(cur_event, curd) {
                parse_event_descriptor(curd,  E);
            }

            AddIntoEventList(events, E);

        }
        break;
    }
    }
}

void parse_channels(uint8_t *buf, int len,  list<channel> &channels)
{
    struct section *section;
    struct section_ext *section_ext = NULL;

    if ((section = section_codec(buf, len)) == NULL) {
        return;
    }

    switch(section->table_id) {

    case stag_dvb_service_description_actual:
    case stag_dvb_service_description_other:
    {
        struct dvb_sdt_section *sdt;
        struct dvb_sdt_service *cur_service;
        struct descriptor *curd;

        if ((section_ext = section_ext_decode(section, 1)) == NULL) {
            return;
        }

        if ((sdt = dvb_sdt_section_codec(section_ext)) == NULL) {
            return;
        }

        dvb_sdt_section_services_for_each(sdt, cur_service) {

            channel C = channel();
            C.set_service_id(cur_service->service_id);

            dvb_sdt_service_descriptors_for_each(cur_service, curd) {
                parse_channel_descriptor(curd, C);
            }
            AddIntoChannelList(channels, C);
        }
        break;
    }
    }
}

void parse_channel_descriptor(struct descriptor *d,  channel &C)
{

    switch(d->tag) {

    case dtag_dvb_service:
    {
        struct dvb_service_descriptor *dx;
        struct dvb_service_descriptor_part2 *part2;

        dx = dvb_service_descriptor_codec(d);
        if (dx == NULL) {
            return;
        }
        part2 = dvb_service_descriptor_part2(dx);

        C.set_provider_name(ToString(dx->service_provider_name_length, dvb_service_descriptor_service_provider_name(dx)));
        C.set_service_name(ToString(part2->service_name_length, dvb_service_descriptor_service_name(part2)));

        break;
    }


    default:
        return;
    }
}

void parse_event_descriptor(struct descriptor *d,  event_data &E)
{

    switch(d->tag) {

    case dtag_dvb_short_event:
    {
        struct dvb_short_event_descriptor *dx;
        struct dvb_short_event_descriptor_part2 *part2;

        dx = dvb_short_event_descriptor_codec(d);
        if (dx == NULL) {
            return;
        }
        part2 = dvb_short_event_descriptor_part2(dx);

        E.set_event_name(ToString(dx->event_name_length, dvb_short_event_descriptor_event_name(dx)));
        E.set_short_text(ToString(part2->text_length, dvb_short_event_descriptor_text(part2)));
        E.set_language_code(ToString(3, dx->language_code));
        break;
    }

    case dtag_dvb_extended_event:
    {
        struct dvb_extended_event_descriptor *dx;
        struct dvb_extended_event_descriptor_part2 *part2;

        dx = dvb_extended_event_descriptor_codec(d);
        if (dx == NULL) {
            return;
        }

        part2 = dvb_extended_event_descriptor_part2(dx);

        E.set_extended_text(ToString(part2->text_length, dvb_extended_event_descriptor_part2_text(part2)));

        break;
    }

    }
}

string ToString(uint8_t l, uint8_t* s){

    string str = "";

    for (int i = 0; i <l; i++) {
        if(s[i] == 0xC2){
            i++;
            switch(s[i]){
            case 'A':
                str+="Á";
                break;
            case 'a':
                str+="á";
                break;
            case 'E':
                str+="É";
                break;
            case 'e':
                str+="é";
                break;
            case 'I':
                str+="Í";
                break;
            case 'i':
                str+="í";
                break;
            case 'O':
                str+="Ó";
                break;
            case 'o':
                str+="ó";
                break;
            case 'U':
                str+="Ú";
                break;
            case 'u':
                str+="ú";
                break;
            case 'Y':
                str+="Ý";
                break;
            case 'y':
                str+="ý";
                break;

            }
        }
        else if(s[i] == 0xCF){
            i++;
            switch(s[i]){
            case 'C':
                str+="Č";
                break;
            case 'c':
                str += "č";
                break;
            case 'D':
                str+="Ď";
                break;
            case 'd':
                str+= "ď";
                break;
            case 'E':
                str+="Ě";
                break;
            case 'e':
                str+= "ě";
                break;
            case 'N':
                str+="Ň";
                break;
            case 'n':
                str+= "ň";
                break;
            case 'R':
                str+="Ř";
                break;
            case 'r':
                str += "ř";
                break;
            case 'S':
                str+="Š";
                break;
            case 's':
                str+="š";
                break;
            case 'T':
                str+="Ť";
                break;
            case 't':
                str+= "ť";
                break;
            case 'Z':
                str+="Ž";
                break;
            case 'z':
                str+= "ž";
                break;
            }

        }else if(s[i]==0xCA){
            i++;
            switch(s[i]){
            case 'U':
                str+= "Ů";
                break;
            case 'u':
                str+= "ů";
                break;
            }
        }else if(s[i]=='&'){
            str+="&amp;";
        }else if(s[i]<= 0x7F){
            str += s[i];
        }
    }

    return str;
}

void ToXml(char* path, list<channel> ch, list<event_data> e){

    struct stat st = {0};

    if (stat(path, &st) == -1) {
        mkdir(path, 0777);
    }

    list<channel>::iterator p1 = ch.begin();
    while(p1 != ch.end()) {

        string str = path + p1->get_service_name() + ".xml";

        const char * cstr = str.c_str();

        ofstream outFile;
        outFile.open(cstr, std::ios_base::binary);

        outFile << "<tv>\n";
        outFile << "<channel id=\"" << p1->get_service_id() << "\"> \n";
        outFile << "    <display-name>" + p1->get_service_name() + "</display-name> \n";
        outFile << "</channel>\n";

        list<event_data>::iterator p2 = e.begin();
        while(p2 != e.end()) {

            if(p1->get_service_id() == p2->get_service_id()){
                outFile << "<programme channel=\"" << p2->get_service_id() << "\" start= \"" << p2->get_start_time() << "\" stop= \"" << p2->get_start_time()+p2->get_duration()<< "\">\n";
                outFile << "    <title>"<< p2->get_event_name() <<"</title>\n";
                outFile << "    <sub-title>"<< p2->get_short_text() <<"</sub-title>\n";
                outFile << "    <desc>"<< p2->get_extended_text() <<"</desc>\n";
                outFile << "</programme>\n";
            }

            p2++;

        }


        outFile << "</tv>";
        outFile.close();



        p1++;
    }

}


void AddIntoEventList(list<event_data> &events, event_data e){
    list<event_data>::iterator p = events.begin();
    while(p != events.end()) {
        if(p->get_service_id() == e.get_service_id() && p->get_event_id() == e.get_event_id()){
            break;
        }
        p++;
    }
    if(p==events.end()){
        events.push_back(e);
    }
    else{
        return;
    }
}


void AddIntoChannelList(list<channel> &channels, channel c){
    list<channel>::iterator p = channels.begin();
    while(p != channels.end()) {
        if(p->get_service_id() == c.get_service_id()){
            break;
        }
        p++;
    }
    if(p==channels.end()){
        channels.push_back(c);
    }
    else{
        return;
    }
}


bool SortCompare(event_data &a, event_data &b){
    int x = a.get_start_time();
    int y = b.get_start_time();

    return x<y;
}


int channels_cb(struct dvbcfg_zapchannel *channel, void *priv)
{
    int pol = (channel->polarization == 'h' ? 0 : 1);
    int dswitch = channel->diseqc_switch;


    struct dvb_frontend_parameters p;

    p.frequency = channel->fe_params.frequency;
    p.inversion = static_cast<fe_spectral_inversion_t>(channel->fe_params.inversion);

    switch(channel->fe_type){
    case DVBFE_TYPE_DVBS:
        p.u.qpsk.symbol_rate = channel->fe_params.u.dvbs.symbol_rate;
        p.u.qpsk.fec_inner = static_cast<fe_code_rate_t>(channel->fe_params.u.dvbs.fec_inner);
        break;
    case DVBFE_TYPE_DVBC:
        p.u.qam.symbol_rate = channel->fe_params.u.dvbc.symbol_rate;
        p.u.qam.fec_inner = static_cast<fe_code_rate_t>(channel->fe_params.u.dvbc.fec_inner);
        p.u.qam.modulation = static_cast<fe_modulation_t>(channel->fe_params.u.dvbc.modulation);
        break;
    case DVBFE_TYPE_DVBT:
        p.u.ofdm.bandwidth = static_cast<fe_bandwidth_t>(channel->fe_params.u.dvbt.bandwidth);
        p.u.ofdm.code_rate_HP=static_cast<fe_code_rate_t>(channel->fe_params.u.dvbt.code_rate_HP);
        p.u.ofdm.code_rate_LP = static_cast<fe_code_rate_t>(channel->fe_params.u.dvbt.code_rate_LP);
        p.u.ofdm.constellation = static_cast<fe_modulation_t>(channel->fe_params.u.dvbt.constellation);
        p.u.ofdm.transmission_mode = static_cast<fe_transmit_mode_t>(channel->fe_params.u.dvbt.transmission_mode);
        p.u.ofdm.guard_interval = static_cast<fe_guard_interval_t>(channel->fe_params.u.dvbt.guard_interval);
        p.u.ofdm.hierarchy_information = static_cast<fe_hierarchy_t>(channel->fe_params.u.dvbt.hierarchy_information);
        break;
    case DVBFE_TYPE_ATSC:
        break;

    }


    ////open////
    int defd, front;

    if((front = open(frdev,O_RDWR)) < 0){
        syslog(LOG_INFO, "Opening frontend failed");
        return -1;
    }

    if ((defd = open(dedev, O_RDWR | O_LARGEFILE )) < 0) {
        syslog(LOG_INFO, "Opening demux failed");
        close(front);
        return -1;
    }



    struct dvb_frontend_info fe_info;

    if (ioctl(front, FE_GET_INFO, &fe_info) < 0) {
        syslog(LOG_INFO, "FE_GET_INFO failed");
        close(front);
        close(defd);
        return -1;
    }


    if(fe_info.type == FE_OFDM){

        if(tune(front, p) >0){

            pthread_t thread1_id;
            pthread_t thread2_id;
            pthread_t thread3_id;
            pthread_t thread4_id;

            pthread_create(&thread1_id, NULL, &readDataThread, &defd);
            sleep(1);
            pthread_create(&thread2_id, NULL, &readDataThread, &defd);
            sleep(1);
            pthread_create(&thread3_id, NULL, &readDataThread, &defd);
            sleep(1);
            pthread_create(&thread4_id, NULL, &readDataThread, &defd);

            pthread_join(thread1_id, NULL);
            pthread_join(thread2_id, NULL);
            pthread_join(thread3_id, NULL);
            pthread_join(thread4_id, NULL);


        }
        else{
            syslog(LOG_INFO, "Tune failed");
        }


    }else if(fe_info.type == FE_QPSK) {

        int hiband = 0;
        int freq = p.frequency;

        if (lnb_type.switch_val && lnb_type.high_val && freq >= lnb_type.switch_val)
            hiband = 1;

        if (hiband)
            p.frequency = freq - lnb_type.high_val;
        else {
            if (freq < lnb_type.low_val)
                p.frequency = lnb_type.low_val - freq;
            else
                p.frequency = freq - lnb_type.low_val;
        }

        if (diseqc(front, dswitch, pol, hiband) > 0){
            if (tune(front, p) > 0){

                pthread_t thread1_id;
                pthread_t thread2_id;
                pthread_t thread3_id;
                pthread_t thread4_id;

                pthread_create(&thread1_id, NULL, &readDataThread, &defd);
                sleep(1);
                pthread_create(&thread2_id, NULL, &readDataThread, &defd);
                sleep(1);
                pthread_create(&thread3_id, NULL, &readDataThread, &defd);
                sleep(1);
                pthread_create(&thread4_id, NULL, &readDataThread, &defd);

                pthread_join(thread1_id, NULL);
                pthread_join(thread2_id, NULL);
                pthread_join(thread3_id, NULL);
                pthread_join(thread4_id, NULL);


            }
            else {
                syslog(LOG_INFO, "Tune failed");
            }
        }else {
            syslog(LOG_INFO, "DiSEqC failed");
        }

    }

    ////close////
    close(front);
    close(defd);


    return 0;
}


void *readDataThread(void *d){
    int defd = *(int*) d;

    __u8 databuff[TS_PACKET_SIZE*20];

    time_t end = time(NULL) + 10;
    while (time(NULL) <= end){

        int len = read_data(defd, databuff, sizeof(databuff) ,PID_SDT);
        parse_channels(databuff, len, channels);

        len = read_data(defd, databuff, sizeof(databuff) ,PID_EIT);
        parse_events(databuff, len, events);

    }
}


int tune(int fe_fd, dvb_frontend_parameters p){


    if (ioctl(fe_fd, FE_SET_FRONTEND, &p) < 0) {
        syslog(LOG_INFO, "FE_SET_FRONTEND failed");
        return -1;
    }

    return 1;

}


void diseqc_send_msg(int fd, fe_sec_voltage_t v, struct dvb_diseqc_master_cmd *cmd, fe_sec_tone_mode_t t, fe_sec_mini_cmd_t b)
{
    if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF) == -1)
        syslog(LOG_INFO, "FE_SET_TONE failed");
    if (ioctl(fd, FE_SET_VOLTAGE, v) == -1)
        syslog(LOG_INFO, "FE_SET_VOLTAGE failed");
        usleep(15 * 1000);
    if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd) == -1)
        syslog(LOG_INFO, "FE_DISEQC_SEND_MASTER_CMD failed");
        usleep(15 * 1000);
    if (ioctl(fd, FE_DISEQC_SEND_BURST, b) == -1)
        syslog(LOG_INFO, "FE_DISEQC_SEND_BURST failed");
        usleep(15 * 1000);
    if (ioctl(fd, FE_SET_TONE, t) == -1)
        syslog(LOG_INFO, "FE_SET_TONE failed");
}


/* digital satellite equipment control,
 * specification is available from http://www.eutelsat.com/
 */
int diseqc(int secfd, int sat_no, int pol_vert, int hi_band)
{

    struct dvb_diseqc_master_cmd cmd =
     {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4};

    /* param: high nibble: reset bits, low nibble set bits,
    * bits are: option, position, polarization, band
    */
    cmd.msg[3] =
    0xf0 | (((sat_no * 4) & 0x0f) | (hi_band ? 1 : 0) | (pol_vert ? 0 : 2));

    diseqc_send_msg(secfd, pol_vert ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18,
            &cmd, hi_band ? SEC_TONE_ON : SEC_TONE_OFF,
            sat_no % 2 ? SEC_MINI_B : SEC_MINI_A);

    return 1;
}


