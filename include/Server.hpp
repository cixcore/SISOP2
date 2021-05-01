#pragma once
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <list>
#include <string>
#include <map>
#include <vector>
#include <queue>
#include <iostream>
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include "Socket.hpp"
using namespace std;



typedef struct __notification {

    __notification();
    __notification(uint32_t new_id, string new_author, time_t new_timestamp, string new_body, uint16_t new_length, uint16_t new_pending) :
        id(new_id), author(new_author), timestamp(new_timestamp), body(new_body), length(new_length), pending(new_pending) {}

    uint32_t id; //Identificador da notificação (sugere-se um identificador único)
    string author; 
    time_t timestamp; //Timestamp da notificação
    string body; //Mensagem
    uint16_t length; //Tamanho da mensagem
    uint16_t pending; //Quantidade de leitores pendentes

    bool operator ==(__notification other) const {
		return id == other.id;
	}

} notification;

typedef struct _event {

    _event();
    _event(uint16_t new_seqn, string new_command, string arg_1, string arg_2, string arg_3, bool new_commited) 
        : seqn(new_seqn), command(new_command), arg1(arg_1), arg2(arg_2), arg3(arg_3), commited(new_commited) {}

    uint16_t seqn;
    string command; // can use packet types
    string arg1;
    string arg2;
    string arg3;
    bool commited;

    bool operator ==(_event other) const {
		return seqn == other.seqn && commited == other.commited;
	}

} event;


class Server
{
public:
    Server(int port);
    Server(host_address addr);
    string ip; 
    int port;
    vector<int> possibleServerPeerPorts; 
    int portPrimarySever;
    bool backupMode; 
    bool electionStarted;
    bool gotAnsweredInElection;

    vector<event> event_history; 

    vector<Packet> packetsToBeSent;

    bool try_to_start_session(string user, host_address address);
    void follow_user(string user, string user_to_follow);
    void create_notification(string user, string body, time_t timestamp);
    void close_session(string user, host_address address);
    void retrieve_notifications_from_offline_period(string user, host_address addr);
    void read_notifications(host_address addr, vector<notification>* notifications);

    void updatePossibleServerPeerPorts();
    void setAsPrimaryServer();

    static void *groupCommunicationHandler(void *handlerArgs);
    static void *groupReadMessagesHandler(void *handlerArgs);
    static void *groupSendMessagesHandler(void *handlerArgs);
    static void *electionTimeoutHandler(void *handlerArgs);


    static void *communicationHandler(void *handlerArgs);
    static void *readCommandsHandler(void *handlerArgs);
    static void *sendNotificationsHandler(void *handlerArgs);


    void print_users_unread_notifications();
    void print_sessions();
    void print_active_notifications();
    void print_active_users_unread_notifications();
    void print_followers();    
    
    void print_COPY_users_unread_notifications();
    void print_COPY_sessions();
    void print_COPY_active_notifications();
    void print_COPY_active_users_unread_notifications();
    void print_COPY_followers();

    
private: 
    pthread_mutex_t mutex_session;
    pthread_mutex_t follow_mutex;
    pthread_mutex_t follower_count_mutex;
    pthread_mutex_t packetsToBeSentMutex;

    pthread_cond_t 	cond_notification_empty, cond_notification_full;
    pthread_mutex_t mutex_notification_sender;

    pthread_mutex_t seqn_transaction_serializer;

    uint32_t notification_id_counter;

    map<string, sem_t> user_sessions_semaphore;
    map< string, list< host_address > > sessions; // {user, [<ip, port>]}
    map< string, list< uint32_t > > users_unread_notifications; // {user, [notification]]}
    map< string, list<string> > followers;
    vector<notification> active_notifications;
    map< host_address, priority_queue< uint32_t, vector<uint32_t>, greater<uint32_t> > > active_users_pending_notifications; // {<ip, port>, min_heap[notification]]}

    bool user_exists(string user);
    bool user_is_active(string user);
    void assign_notification_to_active_sessions(uint32_t notification_id, list<string> followers);
    bool wait_primary_commit(uint16_t seqn);
    bool send_backup_change(uint16_t seqn);

    // transaction management
	vector<Socket*> replica_sockets;
	vector<uint16_t> seqn_history;  

    map<string, sem_t> COPY_user_sessions_semaphore;
    map< string, list< host_address > > COPY_sessions;
    map< string, list< uint32_t > > COPY_users_unread_notifications;
    map< string, list<string> > COPY_followers;
    vector<notification> COPY_active_notifications;
    map< host_address, priority_queue< uint32_t, vector<uint32_t>, greater<uint32_t> > > COPY_active_users_pending_notifications;

    void deepcopy_user_sessions_semaphore(map<string, sem_t> from, map<string, sem_t> *to);
    void deepcopy_sessions(map< string, list<host_address> > from, map< string, list<host_address> > *to);
    void deepcopy_users_unread_notifications(map< string, list<uint32_t> > from, map< string, list<uint32_t> > *to);
    void deepcopy_followers(map< string, list<string> > from, map< string, list<string> > *to);
    void deepcopy_active_notifications(vector<notification> from, vector<notification> *to);
    void deepcopy_active_users_pending_notifications(map< host_address, priority_queue< uint32_t, vector<uint32_t>, greater<uint32_t>>> from, 
                                                     map< host_address, priority_queue< uint32_t, vector<uint32_t>, greater<uint32_t>>> *to);
  
};


struct communiction_handler_args {
	Socket* connectedSocket;
	host_address client_address; 
	string user;
    Server* server;
};

struct group_communiction_handler_args {
    int peerPort;
    Socket* connectedSocket;
    Server* server;
    bool isAcceptingConnection;  // Wether the connection started by this server instance was by accepting (true),
                                 // where the opposite would be it trying to connect to other instance (false)
};


class ServerSocket : public Socket {
	
	public:
		struct sockaddr_in serv_addr;

		void bindAndListen(Server* server);
		void connectNewClientOrServer(pthread_t *threadID, Server *server);
        void connectToGroupMembers(Server* server);
        bool connectToMember(sockaddr_in serv_addr, Server* server);

		ServerSocket();
};