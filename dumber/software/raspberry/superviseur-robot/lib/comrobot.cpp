/*
 * Copyright (C) 2018 dimercur
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "comrobot.h"

#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>  

#include <string>
#include <stdexcept>

#ifdef __SIMULATION__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
int sock = 0;
string host = "127.0.0.1";
#define PORT 6699
#endif

#ifdef __FOR_PC__
#define USART_FILENAME "/dev/ttyUSB0"
#else
#define USART_FILENAME "/dev/ttyS0"
#endif /* __FOR_PC__ */

/*
 * Constants to be used for communicating with robot. Contains command tag
 */
const char LABEL_ROBOT_PING = 'p';
const char LABEL_ROBOT_RESET = 'r';
const char LABEL_ROBOT_START_WITH_WD = 'W';
const char LABEL_ROBOT_START_WITHOUT_WD = 'u';
const char LABEL_ROBOT_RELOAD_WD = 'w';
const char LABEL_ROBOT_MOVE = 'M';
const char LABEL_ROBOT_TURN = 'T';
const char LABEL_ROBOT_GET_BATTERY = 'v';
const char LABEL_ROBOT_GET_STATE = 'b';
const char LABEL_ROBOT_POWEROFF = 'z';

const char LABEL_ROBOT_OK = 'O';
const char LABEL_ROBOT_ERROR = 'E';
const char LABEL_ROBOT_UNKNOWN_COMMAND = 'C';

const char LABEL_ROBOT_SEPARATOR_CHAR = '=';
const char LABEL_ROBOT_ENDING_CHAR = 0x0D; // carriage return (\\r)

/**
 * Open serial link with robot
 * @return File descriptor
 * @throw std::runtime_error if it fails
 */
int ComRobot::Open() {
    return this->Open(USART_FILENAME);
}

/**
 * Open serial link with robot
 * @param usart Filename of usart to open
 * @return File descriptor
 * @throw std::runtime_error if it fails
 */
int ComRobot::Open(string usart) {
    struct termios options;

#ifdef __SIMULATION__

    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof tv);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form 
    if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof (serv_addr)) < 0) {
        return -1;
    }
    return 1;
#else

    fd = open(usart.c_str(), O_RDWR | O_NOCTTY /*| O_NDELAY*/); //Open in blocking read/write mode
    if (fd == -1) {
        cerr << "[" << __PRETTY_FUNCTION__ << "] Unable to open UART (" << usart << "). Ensure it is not in use by another application" << endl << flush;
        throw std::runtime_error{"Unable to open UART"};
        exit(EXIT_FAILURE);
    } else {
        fcntl(fd, F_SETFL, 0);
        tcgetattr(fd, &options);
        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        cfsetospeed(&options, B9600);
        cfsetispeed(&options, B9600);
        options.c_cc[VMIN] = 0;
        options.c_cc[VTIME] = 1; /* Timeout of 100 ms per character */
        tcsetattr(fd, TCSANOW, &options);
    }

    return fd;
#endif
}

int ComRobot::Open(string shost, int nport) {
#ifdef __SIMULATION__

    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cout <<"\n Socket creation error " << endl << flush;
        return -1;
    }
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof tv);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(nport);

    if (inet_pton(AF_INET, shost.c_str(), &serv_addr.sin_addr) <= 0) {
        cout << "Invalid address/ Address not supported" << endl << flush;
        return -1;
    }

    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof (serv_addr)) < 0) {
        return -2;
    }
    return 1;
#else
    return -1
#endif
}

/**
 * Close serial link
 * @return Success if above 0, failure if below 0
 */
int ComRobot::Close() {
#ifdef __SIMULATION__
    return close(sock);
#elif
    return close(fd);
#endif
}

/**
 * Send a message to robot
 * @param msg Message to send to robot
 * @return 1 if success, 0 otherwise
 * @attention Message is destroyed (delete) after being sent. You do not need to delete it yourself
 * @attention Write is blocking until message is written into buffer (linux side)
 * @warning Write is not thread save : check that multiple tasks can't access this method simultaneously  
 */
Message *ComRobot::Write(Message* msg) {
    Message *msgAnswer;
    string s;

    if (this->fd != -1) {

        Write_Pre();

        s = MessageToString(msg);
#ifdef __SIMULATION__
        s += "\r";

        char buffer[1024] = {0};
        cout << "[" << __PRETTY_FUNCTION__ << "] Send command: " << s << endl << flush;
        send(sock, s.c_str(), s.length(), MSG_NOSIGNAL);

        int valread = read(sock, buffer, 1024);

        if (valread == 0) {
            cout << "The communication is out of order" << endl;
            msgAnswer = new Message(MESSAGE_ANSWER_COM_ERROR);
        } else if (valread < 0) {
            cout << "Timeout" << endl;
            msgAnswer = new Message(MESSAGE_ANSWER_ROBOT_TIMEOUT);
        } else {
            string s(&buffer[0], valread);
            msgAnswer = StringToMessage(s);
            cout << "Response: " << buffer << ", id: " << msgAnswer->GetID() << endl;
        }

#else       
        AddChecksum(s);

        //cout << "[" <<__PRETTY_FUNCTION__<<"] Send command: "<<s<<endl<<flush;
        int count = write(this->fd, s.c_str(), s.length()); //Filestream, bytes to write, number of bytes to write

        if (count < 0) {
            cerr << "[" << __PRETTY_FUNCTION__ << "] UART TX error (" << to_string(count) << ")" << endl << flush;
            msgAnswer = new Message(MESSAGE_ANSWER_COM_ERROR);
        } else { /* write successfull, read answer from robot */

            try {
                s = Read();
                //cout << "Answer = "<<s<<endl<<flush;

                if (VerifyChecksum(s)) {
                    msgAnswer = StringToMessage(s);
                } else msgAnswer = new Message(MESSAGE_ANSWER_ROBOT_UNKNOWN_COMMAND);

            } catch (std::runtime_error &e) {
                s = string(e.what());

                if (s.find("imeout")) { // timeout detecté
                    msgAnswer = new Message(MESSAGE_ANSWER_ROBOT_TIMEOUT);
                } else {
                    msgAnswer = new Message(MESSAGE_ANSWER_COM_ERROR);
                }
            }
        }
#endif
    } else {
        cerr << __PRETTY_FUNCTION__ << ": Com port not open" << endl << flush;
        throw std::runtime_error{"Com port not open"};
    }

    // deallocation of msg
    delete(msg);

    return msgAnswer;
}

/**
 * Get a message from robot
 * @return Message currently received
 * @attention A message object is created (new) when receiving data from robot. You MUST remember to destroy is (delete) after use
 * @attention Read method is blocking until a message is received
 * @warning Read is not thread safe : Do not call it in multiple tasks simultaneously
 */
string ComRobot::Read() {
    string s;
    int rxLength;
    unsigned char receivedChar;

    do {
        rxLength = read(this->fd, (void*) &receivedChar, 1); //Filestream, buffer to store in, number of bytes to read (max)
        if (rxLength == 0) { // timeout
            // try again
            rxLength = read(this->fd, (void*) &receivedChar, 1); //Filestream, buffer to store in, number of bytes to read (max)
            if (rxLength == 0) { // re-timeout: it sucks !
                throw std::runtime_error{"ComRobot::Read: Timeout when reading from com port"};
            }
        } else if (rxLength < 0) { // big pb !
            throw std::runtime_error{"ComRobot::Read: Unknown problem when reading from com port"};
        } else { // everything ok
            if ((receivedChar != '\r') && (receivedChar != '\n')) s += receivedChar;
        }
    } while ((receivedChar != '\r') && (receivedChar != '\n'));

    return s;
}

Message *ComRobot::SendCommand(Message* msg, MessageID answerID, int maxRetries) {
    int counter = maxRetries;
    Message *msgSend;
    Message *msgRcv;
    Message *msgTmp;

    do {
        msgSend = msg->Copy();
        cout << "S => " << msgSend->ToString() << endl << flush;
        msgTmp = Write(msgSend);
        cout << "R <= " << msgTmp->ToString() << endl << flush;

        if (msgTmp->CompareID(answerID)) counter = 0;
        else counter--;

        if (counter == 0) msgRcv = msgTmp->Copy();

        delete(msgTmp);
    } while (counter);

    delete (msg);

    return msgRcv;
}

/**
 * Convert an array of char to its message representation (when receiving data from stm32)
 * @param bytes Array of char
 * @return Message corresponding to received array of char
 */
Message* ComRobot::StringToMessage(string s) {
    Message *msg;

    switch (s[0]) {
        case LABEL_ROBOT_OK:
            msg = new Message(MESSAGE_ANSWER_ACK);
            break;
        case LABEL_ROBOT_ERROR:
            msg = new Message(MESSAGE_ANSWER_ROBOT_ERROR);
            break;
        case LABEL_ROBOT_UNKNOWN_COMMAND:
            msg = new Message(MESSAGE_ANSWER_ROBOT_UNKNOWN_COMMAND);
            break;
        case '0':
            msg = new MessageBattery(MESSAGE_ROBOT_BATTERY_LEVEL, BATTERY_EMPTY);
            break;
        case '1':
            msg = new MessageBattery(MESSAGE_ROBOT_BATTERY_LEVEL, BATTERY_LOW);
            break;
        case '2':
            msg = new MessageBattery(MESSAGE_ROBOT_BATTERY_LEVEL, BATTERY_FULL);
            break;
        default:
            msg = new Message(MESSAGE_ANSWER_ROBOT_ERROR);
            cerr << "[" << __PRETTY_FUNCTION__ << "] Unknown message received from robot (" << s << ")" << endl << flush;
    }

    return msg;
}

/**
 * Convert a message to its array of char representation (for sending command to stm32)
 * @param msg Message to be sent to robot
 * @param buffer Array of char, image of message to send
 */
string ComRobot::MessageToString(Message *msg) {
    string s;

    float val_f;
    int val_i;
    unsigned char *b;

    switch (msg->GetID()) {
        case MESSAGE_ROBOT_PING:
            s += LABEL_ROBOT_PING;
            break;
        case MESSAGE_ROBOT_RESET:
            s += LABEL_ROBOT_RESET;
            break;
        case MESSAGE_ROBOT_POWEROFF:
            s += LABEL_ROBOT_POWEROFF;
            break;
        case MESSAGE_ROBOT_START_WITHOUT_WD:
            s += LABEL_ROBOT_START_WITHOUT_WD;
            break;
        case MESSAGE_ROBOT_START_WITH_WD:
            s += LABEL_ROBOT_START_WITH_WD;
            break;
        case MESSAGE_ROBOT_RELOAD_WD:
            s += LABEL_ROBOT_RELOAD_WD;
            break;
        case MESSAGE_ROBOT_BATTERY_GET:
            s += LABEL_ROBOT_GET_BATTERY;
            break;
        case MESSAGE_ROBOT_STATE_GET:
            s += LABEL_ROBOT_GET_STATE;
            break;
        case MESSAGE_ROBOT_GO_FORWARD:
            s += LABEL_ROBOT_MOVE;
            s += LABEL_ROBOT_SEPARATOR_CHAR;
            s.append(to_string(500000));
            break;
        case MESSAGE_ROBOT_GO_BACKWARD:
            s += LABEL_ROBOT_MOVE;
            s += LABEL_ROBOT_SEPARATOR_CHAR;
            s.append(to_string(-500000));
            break;
        case MESSAGE_ROBOT_GO_LEFT:
            s += LABEL_ROBOT_TURN;
            s += LABEL_ROBOT_SEPARATOR_CHAR;
            s.append(to_string(-500000));
            break;
        case MESSAGE_ROBOT_GO_RIGHT:
            s += LABEL_ROBOT_TURN;
            s += LABEL_ROBOT_SEPARATOR_CHAR;
            s.append(to_string(500000));
            break;
        case MESSAGE_ROBOT_STOP:
            s += LABEL_ROBOT_MOVE;
            s += LABEL_ROBOT_SEPARATOR_CHAR;
            s.append(to_string(0));
            break;
        case MESSAGE_ROBOT_MOVE:
            s += LABEL_ROBOT_MOVE;
            s += LABEL_ROBOT_SEPARATOR_CHAR;
            s.append(to_string(((MessageInt*) msg)->GetValue()));
            break;
        case MESSAGE_ROBOT_TURN:
            s += LABEL_ROBOT_TURN;
            s += LABEL_ROBOT_SEPARATOR_CHAR;
            s.append(to_string(((MessageInt*) msg)->GetValue()));
            break;
        default:
            cerr << "[" << __PRETTY_FUNCTION__ << "] Invalid message for robot (" << msg->ToString() << ")" << endl << flush;
            throw std::runtime_error{"Invalid message"};
    }

    return s;
}

/**
 * Add a checksum and carriage return to a command string
 * @param[in,out] s String containing command for robot, without ending char (carriage return) 
 */
void ComRobot::AddChecksum(string &s) {
    unsigned char checksum = 0;

    for (string::iterator it = s.begin(); it != s.end(); ++it) {
        checksum ^= (unsigned char) *it;
    }

    s += (char) checksum; // Add calculated checksum
    s += (char) LABEL_ROBOT_ENDING_CHAR;
}

/**
 * Verify if checksum of an incoming answer from robot is valid, 
 * then remove checksum from incoming answer (if checksum is ok)
 * @param[in,out] s String containing incoming answer from robot
 * @return true is checksum is valid, false otherwise.
 */
bool ComRobot::VerifyChecksum(string &s) {
    unsigned char checksum = 0;

    for (string::iterator it = s.begin(); it != s.end(); ++it) {
        checksum ^= (unsigned char) *it;
    }

    if (checksum == 0) { // checksum is ok, remove last char of string (checksum)
        s.pop_back(); // remove last char
        return true;
    } else return false;
}
