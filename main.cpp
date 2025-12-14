#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <array>
#include <cstring>

#include "timerC.h"
#include "unreliableTransport.h"
#include "logging.h"

constexpr size_t WINDOW_SIZE = 10;  
constexpr size_t PACKET_SIZE = 255; 


int main(int argc, char* argv[]) {
    // Defaults
    uint16_t portNum(12345);
    std::string hostname("isengard.mines.edu");
    std::string inputFilename("input.dat");

    int opt;
    try {
        while ((opt = getopt(argc, argv, "f:h:p:d:")) != -1) {
            switch (opt) {
                case 'p':
                    portNum = std::stoi(optarg);
                    break;
                case 'h':
                    hostname = optarg;
                    break;
                case 'd':
                    LOG_LEVEL = std::stoi(optarg);
                    break;
                case 'f':
                    inputFilename = optarg;
                    break;
                case '?':
                default:
                    std::cout << "Usage: " << argv[0] << " [-h hostname] [-p port] [-d debug_level]" << std::endl;
                    return 1;
            }
        }
    } 
    catch (std::exception& e) {
        FATAL << "Invalid command line arguments: " << e.what() << ENDL;
        return 1;
    }

    TRACE << "Command line arguments parsed." << ENDL;
    TRACE << "\tServername: " << hostname << ENDL;
    TRACE << "\tPort number: " << portNum << ENDL;
    TRACE << "\tDebug Level: " << LOG_LEVEL << ENDL;
    TRACE << "\tInput file name: " << inputFilename << ENDL;

    //open input file
    std::ifstream inputFile(inputFilename, std::ios::binary);
    if (!inputFile.is_open()) {
        FATAL << "Error opening file: " << inputFilename << ENDL;
        return 1;
    }

    try {
        unreliableTransportC transport(hostname, portNum);
        timerC timer;
        timer.setDuration(1000);

        std::array<datagramS, WINDOW_SIZE> window;
        bool allSent = false;
        bool allAcked = false;
        int base = 0;       
        int nextSeqNum = 0;   

        //retries
        int retries = 0;           
        const int MAX_RETRIES = 10; 

        while (!allAcked || !allSent) {
            //send packets
            if (!allSent && nextSeqNum < base + 10) {
                char buffer[PACKET_SIZE];
                inputFile.read(buffer, PACKET_SIZE);
                int bytesRead = inputFile.gcount();
                datagramS pkt;
                pkt.seqNum = nextSeqNum;
                pkt.payloadLength = bytesRead;
                memcpy(pkt.data, buffer, bytesRead);
                pkt.checksum = computeChecksum(pkt);
                transport.udt_send(pkt);
                int index = nextSeqNum % 10;
                window[index] = pkt;
                nextSeqNum++;

                if(base == nextSeqNum-1){
                    timer.start();
                }
                TRACE << "Sent packet: SeqNum=" << pkt.seqNum << ENDL;
                if (bytesRead == 0) {
                    allSent = true; //when its all sent
                }  
            }

            //send and recieve acks
            datagramS ack;
            ssize_t bytesRead = transport.udt_receive(ack);

            if (bytesRead == 0) {
                TRACE << "No data available from udt_receive()." << ENDL;
            } 
            else if (bytesRead < 0) {
                FATAL << "Error reading from transport. Exiting." << ENDL;
                break;
            } 
            else {
                TRACE << "Received ACK: SeqNum=" << ack.ackNum << ENDL;
                if (validateChecksum(ack)) {
                    base = ack.ackNum + 1;
                    if(base == nextSeqNum)
                    {
                        timer.stop();
                        TRACE << "Timer stopped" << ack.ackNum << ENDL;

                    }
                    else
                    {
                        timer.start();
                        TRACE << "Timer started" << ack.ackNum << ENDL;

                    }
                    TRACE << "Window shifted ew base: " << base << ENDL;
                    TRACE << "Sequence number" << nextSeqNum << ENDL;

                } 
                else {
                    TRACE << "Invalid or duplicate ACK: SeqNum=" << ack.ackNum << ENDL;
                }
            }

            
            if (timer.timeout()) {
                TRACE << "Timeout occurred. Retry #" << retries << ENDL;

                //resend packet
                timer.start();
                for (int i = 0; i < WINDOW_SIZE; i++) {
                        transport.udt_send(window[i]);
                        TRACE << "Resent packet: SeqNum=" << window[i].seqNum << ENDL;
                    
                }
            }

            //break out 
            if (allSent && base == nextSeqNum) {
                TRACE << "All packets acknowledged exiting transfer loop" << ENDL;
                break;
            }
        }

    } catch (const std::exception &e) {
        FATAL << "Error during file transfer: " << e.what() << ENDL;
        return 1;
    }

    inputFile.close();
    TRACE << "File transfer complete" << ENDL;
    return 0;
}
