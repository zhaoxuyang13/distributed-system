/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  2 byte  ->|<-  1 byte  ->|<-  1 byte  ->|<-             the rest            ->|
 *       |<- checksum ->| payload size |<-  seqnum  ->|<-             payload             ->|
 * 
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include "rdt_struct.h"
#include "rdt_sender.h"
#include "utils.h"

class Timer
{
public:
    packet *pkt;
    double expire;
    bool done;
    Timer(packet *p, double t, bool done)
        : pkt(p), expire(t), done(done)
    {}
};
seq_nr_t next_frame_to_send = 0;
seq_nr_t next_ack = 0;
seq_nr_t nbuffered = 0;
packet sliding_window[WINDOW_SIZE];
std::list<packet> waiting_buffer;
std::list<Timer> timers;

static seq_nr_t getSeqNum(packet *pkt)
{
    return (seq_nr_t)(pkt->data[3] & 127);
}
static void Add_Timer(packet *pkt, double expire)
{
    fprintf(stdout,"At %.2fs: start Timer %d, expire time %.2fs \n", GetSimulationTime(),getSeqNum(pkt), expire);
    Timer timer = Timer(pkt, expire, false);
    timers.push_back(timer);
    // first timer set timeout .
    if (timers.size() == 1 && !Sender_isTimerSet())
        Sender_StartTimer(TIME_OUT);
    
}
static void Remove_Timer(seq_nr_t seq_num)
{
    // printf("At %.2fs: Removing timer %d\n", GetSimulationTime(), seq_num);
    // if next due timer is this timer. then pop out and pop out subsequent due timer, and start new timer.
    
    if (getSeqNum(timers.front().pkt) == seq_num)
    {
        printf("At %.2fs: remove front timer %d\n", GetSimulationTime(),seq_num);
        Sender_StopTimer();
        timers.pop_front();
        while (!timers.empty())
        {
            if (timers.front().done)
            {
                timers.pop_front();
            }
            else
            {
                printf("At %.2fs: start Timer %d, left time %.2fs\n", GetSimulationTime(),
                       getSeqNum(timers.front().pkt), timers.front().expire - GetSimulationTime());
                Sender_StartTimer(timers.front().expire - GetSimulationTime());
                break;
            }
        }
    }
    else
    { // if next due timer isn't this timer, then mark this timer as done.
        for (auto& timer : timers)
        {
            if (getSeqNum(timer.pkt) == seq_num){
                printf("At %.2fs: mark timer done %d\n", GetSimulationTime(),seq_num);
                timer.done = true;
            }
        }
    }
}
/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{

    /* 2-byte header indicating the chsum */
    /* 1-byte header indicating the size of the payload */
    /* 1-byte header indicating sequence number  */
    int header_size = 4;

    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - header_size;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;
    while (msg->size - cursor > 0)
    {
        int payload_size = msg->size - cursor > maxpayload_size ? maxpayload_size : msg->size - cursor;

        pkt.data[2] = payload_size;
        pkt.data[3] = next_frame_to_send;
        inc(next_frame_to_send,SEQUNCE_SIZE);
        if (payload_size == msg->size - cursor)
        {
            pkt.data[3] |= (1<<7); // last pkt
        }
        memcpy(pkt.data + header_size, msg->data + cursor, payload_size); // copy data to packet payload
        uint16_t checksum = crc_16((unsigned const char *)(pkt.data + 2), payload_size + 2);
        memcpy(pkt.data, &checksum, 2); // copy checksum to packet front.
        // pkt ready, send it out. if buffered pkt not reach limit, send it and set timer.
        if (nbuffered < WINDOW_SIZE && waiting_buffer.size() == 0)
        {
            int next_pkt = (next_ack + nbuffered) % WINDOW_SIZE;
            sliding_window[next_pkt] = pkt;
            /* send it out through the lower layer */
            Sender_ToLowerLayer(&sliding_window[next_pkt]);
            fprintf(stdout, "At %.2fs: sending pkt %d to lower layer,size %d\n",  GetSimulationTime(),getSeqNum(&sliding_window[next_pkt]),payload_size);
            Add_Timer(&sliding_window[next_pkt], GetSimulationTime() + TIME_OUT);
            nbuffered += 1;
        }
        else
        { // store the pkt in the waiting buffer, and send it when window moves.
            fprintf(stdout, "At %.2fs: push pkt %d to waiting buffer\n",  GetSimulationTime(), getSeqNum(&pkt));
            waiting_buffer.push_back(pkt);
        }
        /* move the cursor */
        cursor += maxpayload_size;
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    fprintf(stderr, "At %.2fs: ack packet %d received\n", GetSimulationTime(), getSeqNum(pkt));
    if (*(uint16_t *)(pkt->data) != crc_16( (const unsigned char *)(pkt->data + 2), 2))
    {
        if(pkt->data[2] != '0') {
            fprintf(stdout,"shit %d, %d\n",pkt->data[2],pkt->data[2] - '0' );
            fprintf(stderr, "%d,%d,%d,%d\n", pkt->data[0],pkt->data[1],pkt->data[2],pkt->data[3]);
        }
        fprintf(stderr, "At %.2fs: ack packet checksum mismathc\n", GetSimulationTime());
        return;
    }
    seq_nr_t seq_ack = getSeqNum(pkt);
    fprintf(stdout, "At %.2fs: receive packet %d ack\n", GetSimulationTime(), seq_ack);
    //forwarding to the next_pkt.
    while(nbuffered > 0 && between(getSeqNum(&sliding_window[next_ack]), seq_ack, 
                                        (getSeqNum(&sliding_window[(next_ack + nbuffered - 1) % WINDOW_SIZE]) + 1) % SEQUNCE_SIZE )){
        nbuffered--;
        Remove_Timer(getSeqNum(&sliding_window[next_ack]));
        inc(next_ack, WINDOW_SIZE);
    }
    //emptying the waiting buffer
    while (nbuffered < WINDOW_SIZE && !waiting_buffer.empty()){
        int next_pkt = (next_ack + nbuffered ) % WINDOW_SIZE;
        sliding_window[next_pkt] = waiting_buffer.front();
        waiting_buffer.pop_front();
        // send pakcet and inc nbuffered.
        Sender_ToLowerLayer(&sliding_window[next_pkt]);
        fprintf(stdout, "At %.2fs: sending pkt %d to lower layer\n", GetSimulationTime(),getSeqNum(&sliding_window[next_pkt]));
        Add_Timer(&sliding_window[next_pkt], GetSimulationTime() + TIME_OUT);
        nbuffered ++ ;
    }
    // Remove_Timer(seq_ack);
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    Timer timer = timers.front();
    timers.pop_front();
    int seq_num = getSeqNum(timer.pkt);
    // resend it 
    /* send it out through the lower layer */
    Sender_ToLowerLayer(timer.pkt);
    fprintf(stdout, "At %.2fs: timeout and resending pkt %d to lower layer\n", GetSimulationTime(), seq_num);
    Add_Timer(timer.pkt, GetSimulationTime() + TIME_OUT);
    // update timer
    while(!timers.empty()){
        if(timers.front().done){
            fprintf(stdout, "At %.2fs: pop out timer for pkt %d\n", GetSimulationTime(), getSeqNum(timers.front().pkt));
            timers.pop_front();
        }else {
            fprintf(stdout, "At %.2fs: restart timer for pkt %d, rest time %.2f\n", GetSimulationTime(), getSeqNum(timers.front().pkt),
                    timers.front().expire-GetSimulationTime());
            double rest_time = timers.front().expire-GetSimulationTime();
            if(rest_time > 0){
                Sender_StartTimer(rest_time);
                return;
            }else {
                packet *pkt = timers.front().pkt;
                timers.pop_front();
                fprintf(stdout, "At %.2fs: resending pkt %d to lower layer\n", GetSimulationTime(), getSeqNum(pkt));
                Sender_ToLowerLayer(pkt);
                Add_Timer(pkt,GetSimulationTime() + TIME_OUT);
            }
        }
    }
    // Remove_Timer(seq_num);
    
}
