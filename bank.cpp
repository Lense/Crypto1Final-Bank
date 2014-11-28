/**
    @file bank.cpp
    @brief Top level bank implementation file
 */
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <limits.h>
#include "structs.h"
#include "cryptoAPI.h"


// Declare bank accounts
Account bank_accounts[] = {
    {0, 3141592653LL, 100, "Alice"},
    {1, 1619033988LL, 50, "Bob"},
    {2, 2718281828LL, 0, "Eve"}
};

// Declare sessions
uint64_t bank_sessions[3] = { 0 };

// Current connections
int current_connect = 0;

void* client_thread(void* arg);
void* console_thread(void* arg);
void abort_thread(const uint8_t acc_num);
void receive_and_decrypt(ATM_to_server * msg, int sock, const uint8_t acc_num);
void encrypt_and_send(server_to_ATM * msg, int sock, const uint8_t acc_num);
bool process_packet(ATM_to_server * incoming, server_to_ATM * outgoing);
bool deposit_amount(const uint8_t dest, const uint32_t amount);
bool withdraw_amount(const uint8_t src, const uint32_t amount);
bool transfer_amount(const uint8_t src, const uint8_t dest, const uint32_t amount);


int main(int argc, char** argv)
{
    if(argc != 2)
    {
        printf("[bank] Usage: bank listen-port\n");
        return -1;
    }

    unsigned short ourport = atoi(argv[1]);

    //socket setup
    int lsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(!lsock)
    {
        printf("[bank] Startup: fail to create socket\n");
        return -1;
    }

    //listening address
    sockaddr_in addr_l;
    addr_l.sin_family = AF_INET;
    addr_l.sin_port = htons(ourport);
    unsigned char* ipaddr = reinterpret_cast<unsigned char*>(&addr_l.sin_addr);
    ipaddr[0] = 127;
    ipaddr[1] = 0;
    ipaddr[2] = 0;
    ipaddr[3] = 1;

    if(0 != bind(lsock, reinterpret_cast<sockaddr*>(&addr_l), sizeof(addr_l)))
    {
        printf("[bank] Startup: failed to bind socket\n");
        return -1;
    }

    if(0 != listen(lsock, SOMAXCONN))
    {
        printf("[bank] Startup: failed to listen on socket\n");
        return -1;
    }

    const int max_connect = 16;

    pthread_t cthread;
    pthread_create(&cthread, NULL, console_thread, NULL);

    //loop forever accepting new connections
    while(1)
    {
        sockaddr_in unused;
        socklen_t size = sizeof(unused);
        int csock = accept(lsock, reinterpret_cast<sockaddr*>(&unused), &size);

        // bad client, skip it
        if(csock < 0)
            continue;

        if(current_connect < max_connect)
        {
            current_connect++;
            pthread_t thread;
            printf("[bank] main: Creating new thread, connection count at %u\n", current_connect);
            pthread_create(&thread, NULL, client_thread, (void*)csock);
        }
    }
}


// if the ATM dies without us gracefully disconnecting...
void abort_thread(const uint8_t acc_num)
{
    if(acc_num < 3)
        bank_sessions[acc_num] = 0;
        
    current_connect--;
    printf("[bank] abort: Thread count is now %u\n", current_connect);
    pthread_exit(0);
}

// ATM <---> Bank Session Handler 
void* client_thread(void* arg)
{
    int csock = (int)arg;
    bool valid = true;
    bool authenticated = false;
    uint8_t acc_num = 0;
    ATM_to_server incoming;
    server_to_ATM outgoing;
        
    printf("[bank] Thread: client ID #%d connected\n", csock);

    // recieve initial auth message
    receive_and_decrypt(&incoming, csock, 0xFF);
    acc_num = incoming.accounts >> 4;
    
    // if no active session, save token and send good response
    if(acc_num >= 3 || acc_num < 0)
        printf("[bank] Auth: invalid account (%u)\n", acc_num);
	else if(bank_sessions[acc_num] != 0)
        printf("[bank] Auth: an active sessions exists for account (%u)\n", acc_num);
	else
    {    
        // save session token
        bank_sessions[acc_num] = incoming.session_token;
    
        // check if the provided pin is valid
        if(bank_accounts[acc_num].pin == incoming.pin)
        {
            // construct auth success message
            strncpy(outgoing.message, "auth", 4);
            outgoing.session_token = incoming.session_token;
            outgoing.transaction_num = 1;
        
            // yay authenticated
            printf("[bank] Auth: user %s authenticated\n", bank_accounts[acc_num].name);
            authenticated = true;
        }
        
        /* because we've set a session token, this will prevent a pin
           bruteforce across all threads for this account (: */
        else
        {
            sleep(3);
            printf("[bank] Auth: invalid PIN (%u) against stored (%u)\n", incoming.pin, bank_accounts[acc_num].pin);
        }
    }
        
    // main command loop
    while(valid & authenticated)
    {
        
        // Encrypt and send packet back
        encrypt_and_send(&outgoing, csock, acc_num);
        printf("[bank] Thread: sent packet\n");
        
        // Get, decrypt, parse, and test packet from ATM
        receive_and_decrypt(&incoming, csock, acc_num);
        printf("[bank] Thread: got packet\n");

        // Make the transaction
        valid = process_packet(&incoming, &outgoing);
        printf("[bank] Thread: processed packet\n");
    }

    // clear session token if this connection ever authenticated
    if(authenticated)
        bank_sessions[acc_num] = 0;
        printf("[bank] Logout: ending %s's session\n", bank_accounts[acc_num].name);
    
    // construct disconnect packet
    strncpy(outgoing.message, "disc", 4);
    //outgoing.session_token = 0;
    outgoing.transaction_num = 0xFF;
    outgoing.session_token = incoming.session_token;
    //outgoing.transaction_num = incoming.transaction_num+1;
    
    // goodbye!
    encrypt_and_send(&outgoing, csock, acc_num);
    printf("[bank] Thread: client ID #%d disconnected\n", csock);
    current_connect--;

    close(csock);
    return NULL;
}

void receive_and_decrypt(ATM_to_server * msg, int sock, const uint8_t acc_num)
{
    int length = 16;
    unsigned char packet[length+1];

    // Receive packet
    if(length != recv(sock, packet, length, MSG_WAITALL))
    {
        printf("[bank] Recv: fail to read packet\n");
        abort_thread(acc_num);
    }

    unsigned char dec_string[length+1];
    // Decrypt packet into message
    if(symmetric_decrypt(packet, dec_string) != length-1)
    {
        printf("[bank] Recv: failed to decrypt message\n");
        abort_thread(acc_num);
    }

    memcpy(msg, dec_string, length-1);
    
}

void encrypt_and_send(server_to_ATM * msg, int sock, const uint8_t acc_num)
{

    int length = 16;
    unsigned char msg_string[length+1];
    memcpy(msg_string, msg, length-1);
    unsigned char packet[length+1];

    // Encrypt message into packet
    if(symmetric_encrypt(msg_string, packet) != length)
    {
        printf("[bank] Send: failed to encrypt message\n");
        abort_thread(acc_num);
    }

    // Send packet through the proxy to the atm
    if(length != send(sock, (void*)packet, length, 0))
    {
        printf("[bank] Send: fail to send packet\n");
        abort_thread(acc_num);
    }
}

bool process_packet(ATM_to_server * incoming, server_to_ATM * outgoing)
{
    int src = incoming->accounts >> 4;
    int dest = incoming->accounts & 0x0F;

    // check src and dest accounts are valid
    if(src > 2 or dest > 2)
    {
        printf("[bank] cmd: Invalid account(s) src (%u) dest (%u)\n", src, dest);
        return false;
    }
        
    // check session token & transaction number are valid
    if(bank_sessions[src] == incoming->session_token && incoming->transaction_num != (outgoing->transaction_num+1))
    {
        printf("[bank] cmd: Invalid session token or transaction number\n");
        printf("[bank] cmd: Incoming session: %llu - Stored session: %llu\n", bank_sessions[src], incoming->session_token);
        printf("[bank] cmd: Incoming trans #: %u - Expected trans #: %u\n", incoming->transaction_num, outgoing->transaction_num+1);
        return false;
    }
    
    // check for transaction number rollover
    if(incoming->transaction_num + 1 == 0)
    {
        printf("[bank] cmd: Transaction limit hit for this session\n");
        return false;
    }
    
    // assign next transaction number and copy over the current session token
    outgoing->transaction_num = incoming->transaction_num + 1;
    outgoing->session_token = incoming->session_token;

    // process all bank action
    switch(incoming->action)
    {
        // check balance
        case 1:
            printf("[bank] cmd: checking balance for user %s\n", bank_accounts[src].name);
            memcpy(outgoing->message, &bank_accounts[src].balance, 4);
            break;
                
        // withdraw money
        case 2:
            if(withdraw_amount(src, incoming->amount))
            {
                printf("[bank] cmd: %s withdrew $%u and has $%u remaining\n", bank_accounts[src].name, incoming->amount, bank_accounts[src].balance);
                memcpy(outgoing->message, &bank_accounts[src].balance, 4);
            }
            else
            {
                printf("[bank] cmd: Withdraw by %s for $%u failed ($%u in account)\n", bank_accounts[src].name, incoming->amount, bank_accounts[src].balance);
                return false;
            }
            
            break;
        
        // logout
        case 3:
            return false;
        
        // transfer money
        case 4:
            if(transfer_amount(src, dest, incoming->amount))
            {
                printf("[bank] cmd: %s sent %s amount $%u\n", bank_accounts[src].name, bank_accounts[dest].name, incoming->amount);
                memcpy(outgoing->message, &bank_accounts[src].balance, 4);
            }
            else
            {
                printf("[bank] cmd: Transfer between %s and %s failed with amount $%u", bank_accounts[src].name, bank_accounts[dest].name, incoming->amount);
                return false;
            }
                
            break;
        
        // action not found
        default:
            return false;
    }

    return true;
}

// bank console for the bank managerz and stuffs
void* console_thread(void* arg)
{
    char buf[80];
    char * cmd = NULL;
    char * username = NULL;
    char * str_amount = NULL;
    uint32_t amount = 0;
    uint8_t acc_num = 0xFF;

    while(1)
    {
       
        // shell input
        printf("bank> ");
        fgets(buf, 79, stdin);
        buf[strlen(buf)-1] = '\0';    //trim off trailing newline
        
        // parse command and username
        cmd = strtok(buf, " ");
        username = strtok(NULL, " ");
        acc_num = 0xFF;
        
        // find associated account
        if(username != NULL)
            for(unsigned int i = 0; i < 3; i++)
                if(strcmp(username, bank_accounts[i].name) == 0)
                    acc_num = i;
        else
            continue;
        
        // process commands
        if(cmd != NULL && username != NULL && acc_num != 0xFF)
        {
            // check balance
            if(strcmp(cmd, "balance") == 0)
                printf("[bank] Shell: User %s's balance is $%u\n", bank_accounts[acc_num].name, bank_accounts[acc_num].balance);
            
            // parse and deposit amount
            else if(strcmp(cmd, "deposit") == 0)
            {
                str_amount = strtok(NULL, " ");
                if(str_amount != NULL)
                {
                    sscanf(str_amount, "%u", &amount);
                    if(deposit_amount(acc_num, amount))
                        printf("[bank] Shell: Successfully deposited $%u into %s's account\n", amount, bank_accounts[acc_num].name);
                    else
                        printf("[bank] Shell: Invalid deposit amount\n");
                }
            }
        }
    }
    
    return NULL;
}

// withdraw some amount of money from an account
bool withdraw_amount(const uint8_t src, const uint32_t amount)
{
    // drop 'negative' numbers cuz we don't want to do more work
    if(amount > INT_MAX)
        return false;

        // make sure it's a valid withdraw amount
    if(bank_accounts[src].balance < amount)
        return false;
        
    // withdraw money
    bank_accounts[src].balance -= amount;
    return true;
}

// deposit money into an account
bool deposit_amount(const uint8_t dest, const uint32_t amount)
{
    // drop 'negative' numbers cuz we don't want to do more work
    if(amount > INT_MAX)
        return false;

    // drop if the amount would cause a uint32 overflow
    if(bank_accounts[dest].balance + amount < bank_accounts[dest].balance)
        return false;
        
    // add the money to the dest account
    bank_accounts[dest].balance += amount;
    return true;
}

// transfer some amount of money between two accounts
bool transfer_amount(const uint8_t src, const uint8_t dest, const uint32_t amount)
{
    // drop 'negative' numbers cuz we don't want to do more work
    if(amount > INT_MAX)
        return false;

    // drop if the amount would cause a uint32 overflow
    if(bank_accounts[dest].balance + amount < bank_accounts[dest].balance)
        return false;
    
    // drop if the source can't withdraw that much money
    // if this passes, withdraw will subtract the funds appropriately
    if(!withdraw_amount(src, amount))
        return false;
        
    // this should always return true at this point
    return deposit_amount(dest, amount);
}
