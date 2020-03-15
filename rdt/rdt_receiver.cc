/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-   4 bytes   ->|<- 4 bytes ->|<-             the rest            ->|
 *       | payload size | sequence number |  checksum   |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <climits>

#include "rdt_struct.h"
#include "rdt_receiver.h"

// some costants and declarations

static unsigned int ack = 0;

// implementing buffer for receiver space

struct bufferNode
{
    struct packet packet;
    unsigned int seq; // to ease seq checking
    bufferNode* next = nullptr;
};

class ReceiverBuffer
{
    private:
    bufferNode* head;
    bufferNode* tail;

    public:
    void addPacket(packet* packet); // add packet into buffer according to sequence
    bufferNode* getPacket(); // get first packet(lowest seq no) from buffer
    bool checkPacket(); // check is there next subsequence seq in buffer
    // int buffersize; // record number of packets in this buffer

    ReceiverBuffer() {
        head = nullptr;
        tail = nullptr;
        // buffersize = 0;
    }
};

bufferNode* ReceiverBuffer::getPacket() {
    if (this->head != nullptr) {
        bufferNode* temp = this->head;

        if (this->head == this->tail) {
            this->head = this->tail = nullptr;
        } else {
            this->head = this->head->next;
        }

        return temp;
    }

    return nullptr;
}

bool ReceiverBuffer::checkPacket() {
    if (this->head == nullptr) {
        return false;
    }

    if (this->head->seq == ack) {
        return true;
    } else {
        return false;
    }
}

void ReceiverBuffer::addPacket(packet* packet) {
    bufferNode* p = (bufferNode *)malloc(sizeof(*p));
    memcpy(&p->packet, packet, sizeof(struct packet));
    p->next = nullptr;
    unsigned int temp1 = *((unsigned int *)(packet->data + 1));
    p->seq = temp1;

    if (this->head == nullptr) {
        this->head = this->tail = p;
    } else {
        for (bufferNode* temp = this->head, * previous = nullptr; temp; previous = temp, temp = temp->next) {
            unsigned int temp2 = *((unsigned int *)(temp->packet.data + 1));

            if (temp1 == temp2) {
                free(p);
                return;
            } else if (temp1 < temp2) {
                p->next = temp;

                if (previous) {
                    previous->next = p;
                } else {
                    this->head = p;
                }

                return;
            }
        }

        this->tail->next = p;
        this->tail = this->tail->next;
    }
}

// class declarations

ReceiverBuffer* receiverBuffer;

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

static void addAck()
{
    if (ack < 4294967295) {
        ack++;
    } else {
        ack = 0;
    }
}

static void sendAck(unsigned int ackno) {
    packet packet;
    int maxpayload_size = RDT_PKTSIZE - 7;
    packet.data[0] = maxpayload_size;
    *((unsigned int *)(packet.data + 1)) = ackno;
    memset(packet.data+7, 1, maxpayload_size);
    unsigned short checksum = generateChecksum(packet.data+7, maxpayload_size);
    *((unsigned short *)(packet.data+5)) = checksum;
    Receiver_ToLowerLayer(&packet);
}

static void normal(packet *pkt) {
    /* 9 bytes header size */
    int header_size = 7;

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message*)malloc(sizeof(struct message));
    ASSERT(msg!=NULL);

    msg->size = pkt->data[0];

    /* sanity check in case the packet is corrupted */
    if (msg->size<0) msg->size=0;
    if (msg->size>RDT_PKTSIZE-header_size) msg->size=RDT_PKTSIZE-header_size;

    msg->data = (char*)malloc(msg->size);
    ASSERT(msg->data!=NULL);
    memcpy(msg->data, pkt->data+header_size, msg->size);
    Receiver_ToUpperLayer(msg);

    /* don't forget to free the space */
    if (msg->data!=NULL) free(msg->data);
    if (msg!=NULL) free(msg);

    // send ack
    sendAck(ack);
    addAck();

    // scan receiverBuffer and check is there any following packets. Iterate and send latest ack
    while (receiverBuffer->checkPacket()) {
        bufferNode* node = receiverBuffer->getPacket();
        normal(&node->packet);
        free(node);
    }
}

// given code framework

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());

    receiverBuffer = new ReceiverBuffer();
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());

    delete(receiverBuffer);
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    // first, verify checksum. Dump packet if fail
    int pktsize = pkt->data[0];
    unsigned short ochecksum = *((unsigned short *)(pkt->data + 5));

    if (!verifyChecksum(pkt->data+7, ochecksum, pktsize)) {
        return;
    }

    // next, process response
    unsigned int seqReceived = *((unsigned int *)(pkt->data + 1));

    if (ack == seqReceived) { // normal
        normal(pkt);
    } else if (ack < seqReceived) { // this packet is ahead. I will need to put in buffer hoping the previous packet will arrive before timeout
        receiverBuffer->addPacket(pkt);
    } else { // something in sender went wrong, but I am ok. I will just resend ack
        sendAck(seqReceived);
    }
}
