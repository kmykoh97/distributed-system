# Reliable Data Transfer(RDT)

# Implementation

## Packet Structure

| size | seq or ack | checksum | payload |
|---|---|---|---|
| 1 byte | 4 bytes | 2 bytes | the rest |

## Policy

Go Back N

- Window size is 10.
- Sender receives message. Sender divide message into subpackets and calculate checksum and seq then fill into window. Unfilled packets will go into buffer.
- Sender will send all packets in window in a batch to lower layer and activate timer.
    - if timer expired and no ack received, resend everything in window.
    - if ack received before timer expired, record it and increment ack expected. Do relevant calculation in timer array from the timer descriptor and reactivate timer. If all ack gotten, clear window and repeat procedure using subsequence packets from buffer.
- Receiver receives packets from lower layer. Receiver will first check the checksum. Then receiver will analyse the packet.
    - if seq is expected seq, send ack to sender, pass message to upper layer.
    - if seq is pass seq, resend ack to sender.
    - if seq is future seq, place packet into buffer in sequence(easy to implement as buffer is linked-list). If all previous packets are received, process this packet, send ack and increment ack into expected next value.
- 

## Timer

All packets have similar timeout and they will normally sent in batches. So sender will need a data structure that can store indefinitely long array of timer descriptors. When first descriptor(first packet) is added, sender activates the timer. If ack gotten before timeout, sender remove first descriptor and get the next descriptor then activate the timer based on the remaining time. If timeout occurs, sender will destruct the data structure and rebuild everything from current ack after getting those packets from buffer. I am using linked-list to implement this data structure.

## Buffer

Sender:  
If current window is fully occupied, sender need to store packets into buffer. Linked-list is used in my implementation.

Receiver:  
If packets of future seq are received but packet of current seq not yet received, receiver need to store those packets into buffer. Linked-list is used in my implementation.

## Checksum  

Easy implementation of checksum algorithm used. Sender or receiver just add the payload data in every byte together and get the former 2 bytes result. 

We only use 2 bytes as checksum to try to improve spacial efficiency especially if message size is large.

## Result

``` shell
os@ubuntu:~/Desktop/rdt$ ./rdt_sim 1000 0.1 100 0.02 0.02 0.02 0
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 2.00%
	average loss rate is 2.00%
	average corrupt rate is 2.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 1000.30s: sender finalizing ...
At 1000.30s: receiver finalizing ...

## Simulation completed at time 1000.30s with
	985823 characters sent
	985823 characters delivered
	32245 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

517030990022 MENG YIT KOH kmykoh97@sjtu.edu.cn