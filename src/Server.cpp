#include "../include/Server.hpp"

using namespace std;

Server::Server()
{
    this->notification_id_counter = 0;
    mutex_session = PTHREAD_MUTEX_INITIALIZER;
    follow_mutex = PTHREAD_MUTEX_INITIALIZER;
    follower_count_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_cond_init(&cond_notification_empty, NULL);
    pthread_cond_init(&cond_notification_full, NULL);
    pthread_mutex_init(&mutex_notification_sender, NULL);
}

Server::Server(host_address address)
{
    this->notification_id_counter = 0;
	this->ip = address.ipv4;
	this->port = address.port;

    mutex_session = PTHREAD_MUTEX_INITIALIZER;
    follow_mutex = PTHREAD_MUTEX_INITIALIZER;
    follower_count_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_cond_init(&cond_notification_empty, NULL);
    pthread_cond_init(&cond_notification_full, NULL);
    pthread_mutex_init(&mutex_notification_sender, NULL);
}


// se não tiver um host_address, eu posso fazer e devolver um session_id
bool Server::try_to_start_session(string user, host_address address)
{
    pthread_mutex_lock(&mutex_session);

    if(!user_exists(user))
    {
        sem_t num_sessions;
        sem_init(&num_sessions, 0, 2);
        user_sessions_semaphore.insert({user, num_sessions}); // user is created with 2 sessions available
        sessions.insert({user, list<host_address>()});
        followers.insert(pair<string, list<string>>(user, list<string>()));
        users_unread_notifications.insert({user, list<uint32_t>()});
    } 
    
    int session_started = sem_trywait(&(user_sessions_semaphore[user])); // try to consume a session resource
    if(session_started == 0) // 0 if session started, -1 if not
    { 
        sessions[user].push_back(address);
        active_users_pending_notifications.insert({address, priority_queue<uint32_t, vector<uint32_t>, greater<uint32_t>>()});
    }
    pthread_mutex_unlock(&mutex_session); // Fim SC
    this->print_sessions();
    return session_started == 0; 
}

bool Server::user_exists(string user)
{
    return user_sessions_semaphore.find(user) != user_sessions_semaphore.end();
}


// call this function when new notification is created
void Server::create_notification(string user, string body, time_t timestamp)
{
    cout << "\nNew notification!\n";
    pthread_mutex_lock(&follower_count_mutex);

    uint16_t pending_users{0};
    for (auto follower : followers[user])
    {                    
        pthread_mutex_lock(&mutex_notification_sender);
        users_unread_notifications[follower].push_back(notification_id_counter);
        pthread_mutex_unlock(&mutex_notification_sender);

        pending_users++;
    }

    notification notif(notification_id_counter, user, timestamp, body, body.length(), pending_users);
    active_notifications.push_back(notif);
    assign_notification_to_active_sessions(notification_id_counter, followers[user]);
    notification_id_counter += 1;

    pthread_mutex_unlock(&follower_count_mutex);
}

// call this function after new notification is created
void Server::assign_notification_to_active_sessions(uint32_t notification_id, list<string> followers) 
{
    cout << "\nAssigning new notification to active sessions...\n";
    print_users_unread_notifications();
    
    for (auto user : followers)
    {
        if(user_is_active(user)) 
        {
            pthread_mutex_lock(&mutex_notification_sender);
            for(auto address : sessions[user]) 
            {
                //cout << "assigning " << address.ipv4 <<":"<< address.port << " notification " << notification_id <<"\n\n";
                active_users_pending_notifications[address].push(notification_id);
            }
            // when all sessions from same user have notification on its entry, remove @ from list
            list<uint32_t>::iterator it = find(users_unread_notifications[user].begin(), users_unread_notifications[user].end(), notification_id);
            users_unread_notifications[user].erase(it);

            // signal consumer that will send to client
            pthread_cond_signal(&cond_notification_full);
            pthread_mutex_unlock(&mutex_notification_sender);
        }
    }
    print_active_users_unread_notifications();

}

bool Server::user_is_active(string user) 
{
    return sessions.find(user) != sessions.end() && !(sessions[user].empty());
}


// call this function when new session is started (after try_to_start_session()) to wake notification producer to client
void Server::retrieve_notifications_from_offline_period(string user, host_address addr) 
{
    cout << "\nGetting notifications from offline period to active sessions...\n";
    print_users_unread_notifications();
    pthread_mutex_lock(&mutex_notification_sender);
    
    for(auto notification_id : users_unread_notifications[user]) 
    {
        active_users_pending_notifications[addr].push(notification_id);
    }
    
    (users_unread_notifications[user]).clear();

    // signal consumer
    pthread_cond_signal(&cond_notification_full);
    pthread_mutex_unlock(&mutex_notification_sender);
    print_active_users_unread_notifications();
}


void Server::print_users_unread_notifications() 
{
    cout << "\nUsers unread notifications: \n";

    for(auto it = users_unread_notifications.begin(); it != users_unread_notifications.end(); it++)
    {
        cout << it->first << ": [";
        for(auto itl = (it->second).begin(); itl != (it->second).end(); itl++)
        {
            cout << *itl << ", ";
        }
        cout << "]\n";
    }
}


void Server::print_active_users_unread_notifications() 
{
    cout << "Active users notifications to receive: \n";

    for(auto it = active_users_pending_notifications.begin(); it != active_users_pending_notifications.end(); it++)
    {
        cout << it->first.ipv4 << ":" << it->first.port << ": [";
        cout << it->second.size() << "]\n";
    }
}

void Server::print_sessions() 
{
    cout << "\nSessions: " << sessions.size() << "\n";

    for(auto it = sessions.begin(); it != sessions.end(); it++)
    {
        cout << it->first << ": [";
        for(auto itl = (it->second).begin(); itl != (it->second).end(); itl++)
        {
            cout << (*itl).ipv4 << ":" << (*itl).port << ", ";
        }
        cout << "]\n";
    }
}

void Server::print_active_notifications() 
{
    cout << "\nNotifications: " << active_notifications.size() << "\n";

    for(auto it = active_notifications.begin(); it != active_notifications.end(); it++)
    {
        cout << it->id << "\n";
        cout << it->author << "\n";
        cout << it->body << "\n";
        cout << "\n";
    }
}

void Server::print_followers() 
{
    cout << "\nFollowers: " << followers.size() << "\n";

    for(auto it = followers.begin(); it != followers.end(); it++)
    {
        cout << it->first << ": [";
        for(auto itl = (it->second).begin(); itl != (it->second).end(); itl++)
        {
            cout << *itl << ", ";
        }
        cout << "]\n";
    }
}


// call this function on consumer thread that will feed the user with its notifications
void Server::read_notifications(host_address addr, vector<notification>* notifications) 
{
    pthread_mutex_lock(&mutex_notification_sender);
    
    cout << "Reading notifications...\n";
    while (active_users_pending_notifications[addr].empty()) { 
        // sleep while user doesn't have notifications to read
        cout << "No notifications for address " << addr.ipv4 <<":"<< addr.port << ". Sleeping...\n";
        pthread_cond_wait(&cond_notification_full, &mutex_notification_sender); 
    }
    cout << "Assembling notifications...\n";
    while(!active_users_pending_notifications[addr].empty()) 
    {
        uint32_t notification_id = (active_users_pending_notifications[addr]).top();
        for(auto notif : active_notifications)
        {
            if(notif.id == notification_id)
            {
                notifications->push_back(notif);
                break;
            }
        }
        
        (active_users_pending_notifications[addr]).pop();
    }

    // signal producer
    pthread_cond_signal(&cond_notification_empty);
    pthread_cond_signal(&cond_notification_full); // testing if waking someone again will improve client flow without messing shit up
    pthread_mutex_unlock(&mutex_notification_sender);

}


// call this function when client presses ctrl+c or ctrl+d
void Server::close_session(string user, host_address address) 
{
    pthread_mutex_lock(&mutex_session);
    
    list<host_address>::iterator it = find(sessions[user].begin(), sessions[user].end(), address);
    if(it != sessions[user].end()) // remove address from sessions map and < (ip, port), notification to send > 
    {
        sessions[user].erase(it);
        active_users_pending_notifications.erase(address);

        // signal semaphore
        sem_post(&(user_sessions_semaphore[user]));
    }
    pthread_mutex_unlock(&mutex_session);
    print_sessions();
}


void Server::follow_user(string user, string user_to_follow)
{
    pthread_mutex_lock(&follow_mutex);

    if (find(followers[user_to_follow].begin(), followers[user_to_follow].end(), user) == followers[user_to_follow].end())
    {
        followers[user_to_follow].push_back(user);
    }

    pthread_mutex_unlock(&follow_mutex);
    print_followers();
}





ServerSocket::ServerSocket() : Socket(){
    
    this->serv_addr.sin_family = AF_INET;
	this->serv_addr.sin_port = htons(PORT);
	this->serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(this->serv_addr.sin_zero), 8);

}

void ServerSocket::connectNewClient(pthread_t *threadID, Server* server){

    int *newsockfd = (int *) calloc(1, sizeof(int));
    Socket *newClientSocket = (Socket *) calloc(1, sizeof(Socket));
	socklen_t clilen;
	struct sockaddr_in cli_addr;
    host_address client_address;
    string user;
    

    // Accepting connection to start communicating
    clilen = sizeof(struct sockaddr_in);
    if ((*newsockfd = accept(this->getSocketfd(), (struct sockaddr *) &cli_addr, &clilen)) == -1) {
        std::cout << "ERROR on accepting client connection" << std::endl;
        //exit(1);
        return;
    }
    newClientSocket = new Socket(*newsockfd);

    std::cout << "New connection estabilished on socket: " << *newsockfd << "\n\n";
    
    // Verify if there are free sessions available
    // read client username from socket in 'user' var
    Packet *userPacket = newClientSocket->readPacket();

    if (userPacket == NULL){
        std::cout << "Unable to read user information. Closing connection." << std::endl;
        return;     // destructor automatically closes the socket
    } else 
        user = userPacket->getPayload();
    

    client_address.ipv4 = inet_ntoa(cli_addr.sin_addr);
    client_address.port = ntohs(cli_addr.sin_port);
    bool sessionAvailable = server->try_to_start_session(user, client_address);

    server->print_sessions();
    server->print_users_unread_notifications();
    
    Packet sessionResultPkt;
    if (!sessionAvailable){
        sessionResultPkt = Packet(SESSION_OPEN_FAILED, "Unable to connect to server: no sessions available.");
        newClientSocket->sendPacket(sessionResultPkt);
        return; // destructor automatically closes the socket
    } else{
        sessionResultPkt = Packet(SESSION_OPEN_SUCCEDED, "Connection succeded! Session established.");
        newClientSocket->sendPacket(sessionResultPkt);
    }
    
    // Build args
    communiction_handler_args *args = (communiction_handler_args *) calloc(1, sizeof(communiction_handler_args));
    args->client_address = client_address;
    args->connectedSocket = newClientSocket;
    args->user = user;
    args->server = server;

    pthread_create(threadID, NULL, Server::communicationHandler, (void *)args);
    //pthread_join(*threadID, NULL);
}


void ServerSocket::bindAndListen(){
    
    if (bind(this->getSocketfd(), (struct sockaddr *) &this->serv_addr, sizeof(this->serv_addr)) < 0) {
		cout << "ERROR on binding\n";
        exit(1);
    }
	
	listen(this->getSocketfd(), MAX_TCP_CONNECTIONS);
	std::cout << "Listening..." << "\n\n";
}




void *Server::communicationHandler(void *handlerArgs){

    pthread_t readCommandsT;
    pthread_t sendNotificationsT;

    pthread_create(&readCommandsT, NULL, Server::readCommandsHandler, handlerArgs);
    pthread_create(&sendNotificationsT, NULL, Server::sendNotificationsHandler, handlerArgs);

    pthread_join(readCommandsT, NULL);
    pthread_join(sendNotificationsT, NULL);

    return NULL;
}


void *Server::readCommandsHandler(void *handlerArgs){
	struct communiction_handler_args *args = (struct communiction_handler_args *)handlerArgs;

    while(1){
        Packet* receivedPacket = args->connectedSocket->readPacket();
        if (receivedPacket == NULL){  // connection closed
            args->server->close_session(args->user, args->client_address);
            return NULL;
        }
        cout << receivedPacket->getPayload() << "\n\n";

        switch(receivedPacket->getType()){

            case COMMAND_FOLLOW_PKT:
                args->server->follow_user(args->user, receivedPacket->getPayload());
                break;

            case COMMAND_SEND_PKT:
                args->server->create_notification(args->user, receivedPacket->getPayload(), receivedPacket->getTimestamp());
                break;

            default:
                break;
        }
    }
}


void *Server::sendNotificationsHandler(void *handlerArgs)
{
    struct communiction_handler_args *args = (struct communiction_handler_args *)handlerArgs;
    vector<notification> notifications = vector<notification>();
    Packet notificationPacket;
    int n;

    args->server->print_users_unread_notifications();
    args->server->retrieve_notifications_from_offline_period(args->user, args->client_address);
    args->server->print_users_unread_notifications();

    while(1)
    {
        /*
        args->connectedSocket->sendPacket(Packet(MESSAGE_PKT, "quem mutex sempre alcança\n"));
        */
        args->server->read_notifications(args->client_address, &notifications);
        for(auto it = std::begin(notifications); it != std::end(notifications); ++it)
        {        
            notificationPacket = Packet(NOTIFICATION_PKT, it->timestamp, it->body.c_str(), it->author.c_str());
            
            n = args->connectedSocket->sendPacket(notificationPacket);
            if (n<0)
            {
                args->server->close_session(args->user, args->client_address);
                return NULL;
            }
        }
    }
}