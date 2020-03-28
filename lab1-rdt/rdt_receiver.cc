/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include "rdt_struct.h"
#include "rdt_receiver.h"
#include "utils.h"




static bool buffer_flag[SEQUNCE_SIZE] = {};
static struct message * msg_buffer[SEQUNCE_SIZE] = {};
static seq_nr_t expected_seq = 0;
static std::list<struct message *> submit_buffer; 
/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}


void Ack_seq(seq_nr_t seq_num){
    fprintf(stdout, "At %.2fs: Receiver: ack seq %d send \n", GetSimulationTime(), seq_num);
    packet pkt;
    pkt.data[2] = 0;
    pkt.data[3] = seq_num; 
    uint16_t checksum = crc_16((const unsigned char *)(pkt.data + 2), 2);
    memcpy(pkt.data, &checksum, 2);
    Receiver_ToLowerLayer(&pkt);
}
static void SubmitMsg(struct message* msg, bool last_pkt){
    ASSERT(msg);
    if(last_pkt){// build msg and to upper layer
        int size = 0;
        for(auto message : submit_buffer){
            size += message->size;
        }
        size += msg->size;
        struct message *final_msg = (struct message *) malloc(sizeof(struct message));
        final_msg->size = size;
        final_msg->data = (char *)malloc(size);
        int cursor = 0;
        for(auto& message :submit_buffer){
            memcpy(final_msg->data+cursor, message->data, message->size);
            cursor += message->size;
            free(message->data);
            free(message);
        }
        memcpy(final_msg->data+cursor, msg->data, msg->size);
        fprintf(stdout,"submiting \n");
        Receiver_ToUpperLayer(final_msg);
        free(msg->data);
        free(msg);
        submit_buffer.clear();
    }else{
        submit_buffer.push_back(msg);
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    /* 2-byte header indicating the chsum */
    /* 1-byte header indicating the size of the payload */
    /* 1-byte header indicating sequence number  */
    int header_size = 4;
    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message*) malloc(sizeof(struct message));
    // if not using unsigned char , computation of this value will be signed and negative.
    msg->size = (unsigned char) pkt->data[2];
    int seq_num = pkt->data[3] & 127;
    bool last_pkt = (pkt->data[3] & 128) != 0;
    fprintf(stdout, "At %.2fs: Receiver: receive %d, expect %d, is last %d ，size %d\n", GetSimulationTime(), seq_num, expected_seq,last_pkt,msg->size);
    /* sanity check in case the packet is corrupted */
    // int checklength = (unsigned char)msg->size + 2;
    int checklength = msg->size + 2;
    // fprintf(stdout,"checklength,%d\n",checklength);
    if ( *(uint16_t*)(pkt->data) != crc_16((const unsigned char *)pkt->data + 2,checklength))
    {
        fprintf(stdout, "At %.2fs: packet checksum mismatch\n", GetSimulationTime());
        return ;
    }
    // if msg size out of bounds. it will be detected by checksum.
    ASSERT(msg->size > 0 && msg->size <= RDT_PKTSIZE-header_size);

    if(!between(expected_seq, seq_num,(expected_seq + WINDOW_SIZE)% SEQUNCE_SIZE)){
        fprintf(stdout, "At %.2fs: Receiver: packet %d, no in region\n", GetSimulationTime(), seq_num);
        Ack_seq((expected_seq - 1) % SEQUNCE_SIZE);
        return ;
    }

    /* send mesg to upper layer */
    msg->data = (char*) malloc(msg->size);
    ASSERT(msg->data);
    memcpy(msg->data, pkt->data+header_size, msg->size);

    if(seq_num == expected_seq){ // this seq num, update state.
        SubmitMsg(msg,last_pkt);
        inc(expected_seq, SEQUNCE_SIZE);
        //flush receive buffer to msg slices.
        while (msg_buffer[expected_seq] != nullptr)
        {
            SubmitMsg(msg_buffer[expected_seq], buffer_flag[expected_seq]);
            msg_buffer[expected_seq] = nullptr;
            buffer_flag[expected_seq] = false;
            inc(expected_seq, SEQUNCE_SIZE);
        }
        //reply ack for this seqnum.
        Ack_seq((expected_seq - 1) % SEQUNCE_SIZE);
        
    }else { // other seq num, store in buffer
        if(msg_buffer[seq_num] == nullptr){
            msg_buffer[seq_num] = msg;
            buffer_flag[seq_num ] = last_pkt;
        }
        else {
            /* don't forget to free the space */
            if (msg->data!=NULL) free(msg->data);
            if (msg!=NULL) free(msg);
        }
    }
}
