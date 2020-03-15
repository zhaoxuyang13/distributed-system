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



static bool last_buffer[SEQUNCE_SIZE] = {};
static struct message * recv_buffer[SEQUNCE_SIZE] = {};
static seq_nr_t expected_seq = 0;
static std::list<struct message *> message_slices; 

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    for(int i = 0; i < SEQUNCE_SIZE; i ++){
        recv_buffer[i] = nullptr;
        last_buffer[i] = false;
    }
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
    if(last_pkt){// build msg and toupper layer
        int size = 0;
        for(auto message : message_slices){
            size += message->size;
        }
        size += msg->size;

        struct message *final_msg = (struct message *) malloc(sizeof(struct message));
        final_msg->size = size;
        final_msg->data = (char *)malloc(size);
        int cursor = 0;
        for(auto& message :message_slices){
            memcpy(final_msg->data+cursor, message->data, message->size);
            cursor += message->size;
            free(message->data);
            free(message);
        }
        memcpy(final_msg->data+cursor, msg->data, msg->size);
        assert(cursor + msg->size == size);
        fprintf(stdout,"submiting \n");
        Receiver_ToUpperLayer(final_msg);
        free(msg->data);
        free(msg);
        message_slices.clear();
    }else{
        message_slices.push_back(msg);
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void stop(){}
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
    if ( *(uint16_t*)(pkt->data)!= crc_16((const unsigned char *)pkt->data + 2,checklength))
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
    // Receiver_ToUpperLayer(msg);

    if(seq_num == expected_seq){ // this seq num, update state.
        SubmitMsg(msg,last_pkt);
        inc(expected_seq, SEQUNCE_SIZE);
        //flush receive buffer to msg slices.
        while (recv_buffer[expected_seq] != nullptr)
        {
            SubmitMsg(recv_buffer[expected_seq], last_buffer[expected_seq]);
            recv_buffer[expected_seq] = nullptr;
            last_buffer[expected_seq] = 0;
            inc(expected_seq, SEQUNCE_SIZE);
        }
        //reply ack for this seqnum.
        Ack_seq((expected_seq - 1) % SEQUNCE_SIZE);
        
    }else { // other seq num, store in buffer
        if(recv_buffer[seq_num] == nullptr){
            recv_buffer[seq_num] = msg;
            last_buffer[seq_num ] = last_pkt;
        }
        else {
            /* don't forget to free the space */
            if (msg->data!=NULL) free(msg->data);
            if (msg!=NULL) free(msg);
        }
    }
}
