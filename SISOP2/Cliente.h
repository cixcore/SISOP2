#pragma once
#include <stdint.h.>
#define MAX_MESSAGE_SIZE 128
#define FALSE 0
#define TRUE  1

class Cliente{
public:
	Cliente();
    Cliente(char *clientName,char *listOfFollowers,int numberOfAccess);
    char* clientName; 
    char* listOfFollowers;
    int numberOfAccess;

 private:
	void do_threadSender(void* arg);
	void do_threadReceiver(void* arg);

};


//As estruturas abaixos s�o para ficar dentro da classe Cliente? Se sim, como se passa uma typedef struct como par�metro do m�todo construtor?
typedef struct __packet {
    uint16_t type;              //Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn;              //N�mero de sequ�ncia
    uint16_t length;            //Comprimento do payload
    uint16_t timestamp;         // Timestamp do dado
    const char* _payload;       //Dados da mensagem
} packet;

typedef struct __notification {
    uint32_t id; //Identificador da notifica��o (sugere-se um identificador �nico)
    uint32_t timestamp; //Timestamp da notifica��o
    const char* _string; //Mensagem
    uint16_t length; //Tamanho da mensagem
    uint16_t pending; //Quantidade de leitores pendentes

} notification;