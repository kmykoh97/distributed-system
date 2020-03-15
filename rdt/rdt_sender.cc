/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-1byte->|<-   4 bytes   ->|<- 2 bytes ->|<-             the rest            ->|
 *       |  size   | sequence number |  checksum   |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <climits>
#include <assert.h>

#include "rdt_struct.h"
#include "rdt_sender.h"

// some costants and declarations
static const int WINDOWSIZE = 10;
static const double TIMEOUT = 0.3;

static unsigned int seq = 0;
static unsigned int ack = 0;
static int currentBufferNum = 0;
static packet currentBuffer[WINDOWSIZE];

// implementing buffer for sender's space

struct bufferNode
{
    struct packet packet;
    bufferNode* next = nullptr;
};

class SenderBuffer
{
    private:
    bufferNode* head; // to record sendPacket
    bufferNode* tail; // to record addPacket

    public:
    void addPacket(packet* packet); // add packet into sender's buffer
    bufferNode* sendPacket(); // take packet from sender's buffer
    int buffersize; // record number of packets in this buffer

    SenderBuffer() {
        head = nullptr;
        tail = nullptr;
        buffersize = 0;
    }
};

void SenderBuffer::addPacket(packet* packet) {
    bufferNode* p = (bufferNode *)malloc(sizeof(*p));
    memcpy(&p->packet, packet, sizeof(struct packet));
    p->next = nullptr;

    if (this->head == nullptr) {
        this->head = this->tail = p;
    } else {
        this->tail->next = p;
        this->tail = this->tail->next;
    }

    this->buffersize++;
}

bufferNode* SenderBuffer::sendPacket() {
    bufferNode* temp = this->head;
    this->buffersize--;

    if (this->head == this->tail) {
        this->head = this->tail = nullptr;
    } else {
        this->head = this->head->next;
    }

    return temp;
}

// implementing timer for ack check

struct timerNode
{
    unsigned int ack;
    double starttime;
    double timeout;
    timerNode* next = nullptr;
};

class Timer
{
    private:
    timerNode* head;
    timerNode* tail;
    
    public:
    void newTimer(unsigned int seq); // called after sending packet to receiver
    void removeTimer(unsigned int ack); // called after getting ack from receiver
    // void reset(); // not needed as implemented in destructor

    Timer() {
        head = nullptr;
        tail = nullptr;
    }

    ~Timer() {
        while (this->head != nullptr) {
            timerNode* temp = this->head;
            this->head = this->head->next;
            free(temp);
        }
    }
};

// void Timer::reset() {
//     while (this->head != nullptr) {
//         timerNode* temp = this->head;
//         this->head = this->head->next;
//         free(temp);
//     }

//     this->head = this->tail = nullptr;
// }

void Timer::newTimer(unsigned int seq) {
    timerNode* temp = (timerNode *)malloc(sizeof(*temp));
    temp->timeout = TIMEOUT;
    temp->starttime = GetSimulationTime();
    temp->ack = seq;
    temp->next = nullptr;

    if (this->head) {
        this->tail->next = temp;
        this->tail = this->tail->next;
    } else {
        this->head = this->tail = temp;
    }

    if (!Sender_isTimerSet()) {
        Sender_StartTimer(TIMEOUT);
    }
}

void Timer::removeTimer(unsigned int ack) {
    timerNode* temp = this->head;
    timerNode* previous = nullptr;

    while (temp) {
        if (temp->ack == ack) {
            if (temp == this->head) {
                Sender_StopTimer();
                timerNode* i = this->head;

                // remove first node
                if (i == this->tail) {
                    this->head = this->tail = nullptr;
                } else {
                    this->head = this->head->next;
                }
                
                // calculate the next node remaining time
                if (this->head) {
                    double remainingTimeout = this->head->timeout + this->head->starttime - GetSimulationTime();

                    if (remainingTimeout > 0) {
                        Sender_StartTimer(remainingTimeout);
                    } else {
                        Sender_Timeout();
                    }
                }

                free(i);
            } else {
                // remove this node
                previous->next = temp->next;
                free(temp);
            }

            return;
        }

        previous = temp;
        temp = temp->next;
    }
}

// some helpful functions

static unsigned short generateChecksum(char* packet, int size) {
    unsigned long checksum = 0;
    int tempsize = size;

    for (; tempsize > 0; --tempsize) {
        checksum += *packet++;
    }

    return checksum;
}

static bool verifyChecksum(char* packet, unsigned short ochecksum, int size) {
    unsigned long checksum = 0;
    int tempsize = size;

    for (; tempsize > 0; --tempsize) {
        checksum += *packet++;
    }

    return (checksum == ochecksum);
}

static void addSeq() {
    if (seq < 4294967295) {
        seq++;
    } else {
        seq = 0;
    }
}

static void addAck() {
    if (ack < 4294967295) {
        ack++;
    } else {
        ack = 0;
    }
}

// class declarations

SenderBuffer* senderBuffer;
Timer* timer;

// given code framework

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());

    senderBuffer = new SenderBuffer();
    timer = new Timer();
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());

    delete(senderBuffer);
    delete(timer);
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    /* 9-bytes header indicating the size of the payload, sequence number and checksum */
    int header_size = 7;

    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - header_size;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size-cursor > maxpayload_size) {
	    /* fill in the packet */
	    pkt.data[0] = maxpayload_size;
        *((unsigned int *)(pkt.data + 1)) = seq;
        unsigned int tempseq = seq;
        addSeq();
        memcpy(pkt.data+header_size, msg->data+cursor, maxpayload_size);
        unsigned short checksum = generateChecksum(pkt.data+header_size, maxpayload_size);
        *((unsigned short *)(pkt.data + 5)) = checksum;
        
	    /* send it out through the lower layer */
	    if (currentBufferNum < WINDOWSIZE) {
            // first need to save inside currentBuffer. We can only remove from buffer after getting corresponding ack
            memcpy(currentBuffer+(tempseq%WINDOWSIZE), &pkt, sizeof(struct packet));
            ++currentBufferNum;
            Sender_ToLowerLayer(currentBuffer+(tempseq%WINDOWSIZE));
            timer->newTimer(tempseq);
        } else {
            // store in senderBuffer
            senderBuffer->addPacket(&pkt);
        }

	    /* move the cursor */
	    cursor += maxpayload_size;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
	    /* fill in the packet */        
        pkt.data[0] = msg->size-cursor;
        *((unsigned int *)(pkt.data + 1)) = seq;
        unsigned int tempseq = seq; // we will need this when placing into currentBuffer
        addSeq();
        memcpy(pkt.data+header_size, msg->data+cursor, pkt.data[0]);
        unsigned short checksum = generateChecksum(pkt.data+header_size, msg->size-cursor);
        *((unsigned short *)(pkt.data + 5)) = checksum;

	    /* send it out through the lower layer */
	    if (currentBufferNum < WINDOWSIZE) {
            // first need to save inside currentBuffer. We can only remove from buffer after getting corresponding ack
            memcpy(currentBuffer+(tempseq%WINDOWSIZE), &pkt, sizeof(struct packet));
            ++currentBufferNum;
            Sender_ToLowerLayer(currentBuffer+(tempseq%WINDOWSIZE));
            timer->newTimer(tempseq);
        } else {
            // store in senderBuffer
            senderBuffer->addPacket(&pkt);
        }
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    // first calculate checksum, ignore packet if fail checking
    unsigned short ochecksum = *((unsigned short *)(pkt->data + 5));
    int pktsize = pkt->data[0];
    
    if (!verifyChecksum(pkt->data+7, ochecksum, pktsize)) {
        return;
    }

    // next record and process ack received
    unsigned int acktemp = *((unsigned int *)(pkt->data + 1));

    while (((ack <= acktemp) && (acktemp < seq)) || ((acktemp < seq) && (seq < ack)) || ((seq < ack) && (ack <= acktemp))) {
        currentBufferNum--;
        
        // try to send as much packets as possible from senderBuffer
        while (currentBufferNum < WINDOWSIZE && senderBuffer->buffersize != 0) {
            bufferNode* node = senderBuffer->sendPacket();
            unsigned int seqnode = *((unsigned int *)((&(node->packet))->data + 1));
            memcpy(currentBuffer+(seqnode%WINDOWSIZE), &(node->packet), sizeof(struct packet));
            ++currentBufferNum;
            Sender_ToLowerLayer(currentBuffer+(seqnode%WINDOWSIZE));
            timer->newTimer(seqnode);
            free(node);
        }

        timer->removeTimer(ack);
        addAck();
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    unsigned int lostpacketseq = ack;
    delete(timer);
    timer = new Timer();
    // timer->reset();

    // resend everything from lost ack to seq
    for (int i = 0; i < currentBufferNum; i++) {
        Sender_ToLowerLayer(currentBuffer+(lostpacketseq%WINDOWSIZE));
        timer->newTimer(lostpacketseq);
        lostpacketseq++;
    }
}
