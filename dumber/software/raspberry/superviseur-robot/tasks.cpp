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

#include "tasks.h"
#include <stdexcept>

// Déclaration des priorités des taches
#define PRIORITY_TSERVER 28
#define PRIORITY_TRESTARTSERVER 31
#define PRIORITY_TCLOSECOMROBOT 29
#define PRIORITY_TOPENCOMROBOT 20
#define PRIORITY_TMOVE 24
#define PRIORITY_TSENDTOMON 30
#define PRIORITY_TRECEIVEFROMMON 27
#define PRIORITY_TSTARTROBOT 20
#define PRIORITY_TCAMERA 21
#define PRIORITY_TBATTERY 10
#define PRIORITY_WATCHDOG 26

/*
 * Some remarks:
 * 1- This program is mostly a template. It shows you how to create tasks, semaphore
 *   message queues, mutex ... and how to use them
 * 
 * 2- semDumber is, as name say, useless. Its goal is only to show you how to use semaphore
 * 
 * 3- Data flow is probably not optimal
 * 
 * 4- Take into account that ComRobot::Write will block your task when serial buffer is full,
 *   time for internal buffer to flush
 * 
 * 5- Same behavior existe for ComMonitor::Write !
 * 
 * 6- When you want to write something in terminal, use cout and terminate with endl and flush
 * 
 * 7- Good luck !
 */

/**
 * @brief Initialisation des structures de l'application (tâches, mutex, 
 * semaphore, etc.)
 */
void Tasks::Init() {
    int status;
    int err;

    /**************************************************************************************/
    /* 	Mutex creation                                                                    */
    /**************************************************************************************/
    if (err = rt_mutex_create(&mutex_monitor, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robot, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_robotStarted, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_move, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Mutexes created successfully" << endl << flush;

    /**************************************************************************************/
    /* 	Semaphors creation       							  */
    /**************************************************************************************/
    if (err = rt_sem_create(&sem_barrier, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_openComRobot, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_serverOk, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_startRobot, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }

    if (err = rt_sem_create(&sem_startCamera, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_searchArena, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_restartServer, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_closeComRobot, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_sem_create(&sem_watchdog, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Semaphores created successfully" << endl << flush;

     if (err = rt_task_create(&th_restartServer, "th_restartServer", 0, PRIORITY_TRESTARTSERVER, 0)) {
        cerr << "Error task create: th_restartServer" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    taskinit();

    /**************************************************************************************/
    /* Message queues creation                                                            */
    /**************************************************************************************/
    if ((err = rt_queue_create(&q_messageToMon, "q_messageToMon", sizeof (Message*)*50, Q_UNLIMITED, Q_FIFO)) < 0) {
        cerr << "Error msg queue create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Queues created successfully" << endl << flush;

}
void Tasks::taskinit(){
    int err=0;
    /**************************************************************************************/
    /* Tasks creation                                                                     */
    /**************************************************************************************/
    if (err = rt_task_create(&th_server, "th_server", 0, PRIORITY_TSERVER, 0)) {
        cerr << "Error task create: th_server" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    
    if (err = rt_task_create(&th_sendToMon, "th_sendToMon", 0, PRIORITY_TSENDTOMON, 0)) {
        cerr << "Error task create: th_sendToMon" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_receiveFromMon, "th_receiveFromMon", 0, PRIORITY_TRECEIVEFROMMON, 0)) {
        cerr << "Error task create: th_receiveFromMon" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_openComRobot, "th_openComRobot", 0, PRIORITY_TOPENCOMROBOT, 0)) {
        cerr << "Error task create: th_openComRobot" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_startRobot, "th_startRobot", 0, PRIORITY_TSTARTROBOT, 0)) {
        cerr << "Error task create: th_startRobot" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_move, "th_move", 0, PRIORITY_TMOVE, 0)) {
        cerr << "Error task create: th_move" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }    
    if (err = rt_task_create(&th_battery, "th_battery", 0, PRIORITY_TBATTERY, 0)) {
        cerr << "Error task create: th_battery" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_closeComRobot, "th_closeComRobot", 0, PRIORITY_TCLOSECOMROBOT, 0)) {
        cerr << "Error task create: th_closeComRobot" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_watchdog, "th_watchdog", 0, PRIORITY_WATCHDOG, 0)) {
        cerr << "Error task create: th_watchdog" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    cout << "Tasks created successfully" << endl << flush;
}
/**
 * @brief Démarrage des tâches
 */
void Tasks::Run() {
    rt_task_set_priority(NULL, T_LOPRIO);
    int err;

    if (err = rt_task_start(&th_server, (void(*)(void*)) & Tasks::ServerTask, this)) {
        cerr << "Error task start: th_server" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }    
    if (err = rt_task_start(&th_restartServer, (void(*)(void*)) & Tasks::RestartServerTask, this)) {
        cerr << "Error task start: th_restartServer" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_sendToMon, (void(*)(void*)) & Tasks::SendToMonTask, this)) {
        cerr << "Error task start: th_sendToMon" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_receiveFromMon, (void(*)(void*)) & Tasks::ReceiveFromMonTask, this)) {
        cerr << "Error task start: th_receiveFromMon" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_openComRobot, (void(*)(void*)) & Tasks::OpenComRobot, this)) {
        cerr << "Error task start: th_openComRobot" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_closeComRobot, (void(*)(void*)) & Tasks::CloseComRobotTask, this)) {
        cerr << "Error task start: th_closeComRobot" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_startRobot, (void(*)(void*)) & Tasks::StartRobotTask, this)) {
        cerr << "Error task start: th_startRobot" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_move, (void(*)(void*)) & Tasks::MoveTask, this)) {
        cerr << "Error task start: th_move" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_battery, (void(*)(void*)) & Tasks::BatteryLevelTask, this)) {
        cerr << "Error task start: th_battery" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }/*
    if (err = rt_task_start(&th_startCamera, (void(*)(void*)) & Tasks::StartCameraTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_searchArena, (void(*)(void*)) & Tasks::SearchArenaTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_stopCamera, (void(*)(void*)) & Tasks::StopCameraTask, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }    */
    if (err = rt_task_start(&th_closeComRobot, (void(*)(void*)) & Tasks::CloseComRobotTask, this)) {
        cerr << "Error task start: th_closeComRobot" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_start(&th_watchdog, (void(*)(void*)) & Tasks::WatchDog, this)) {
        cerr << "Error task start: th_watchdog" << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    
    cout << "Tasks launched" << endl << flush;
}
/**
 * @brief Arrêt des tâches
 */
void Tasks::Stop() {
    monitor.Close();
    robot.Close();
}

/**
 */
void Tasks::BR() {
    cout << "Tasks synchronized" << endl << flush;
    rt_sem_broadcast(&sem_barrier);
    pause();
}
/** @brief
 * TODO
 */
void Tasks::StopCameraTask(void* arg){
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier,TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    // lunch only when needed in receiveFromMon
    rt_sem_p(&sem_stopCamera,TM_INFINITE);
    //TODO MESSAGE_CAM_IMAGE
    cout << "CAMERA STOPED" << endl << flush;
}
/** @brief
 * TODO
 */
void Tasks::SearchArenaTask(void *arg){
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier,TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    // lunch only when needed in receiveFromMon
    rt_sem_p(&sem_searchArena,TM_INFINITE);
    //TODO MESSAGE_CAM_IMAGE
}

/** @brief
 * TODO
 */
void Tasks::StartCameraTask(void *arg){
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting t1hat all tasks are starting)
    rt_sem_p(&sem_barrier,TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    // lunch only when needed in receiveFromMon
    rt_sem_p(&sem_startCamera,TM_INFINITE);
    //TODO
}
/** @brief 
 * Gestion affichage du niveau de batterie (envoi au moniteur)
 */
void Tasks::BatteryLevelTask(void * arg){
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    int err, rs; 
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    rt_task_set_periodic(NULL, TM_NOW, 500000000);
    
    while (1) {
        
        //Attente du lancement de la Comm avec le robot
        rt_task_wait_period(NULL);
        //cout << "Periodic battery update" << endl << flush;
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            //Envoi du message
            Message * mSent = new Message(MESSAGE_ROBOT_BATTERY_GET);
            Message * mReceived = new Message();
            mReceived = robot.Write(mSent);
            
            if (err = mReceived->CompareID(MESSAGE_ANSWER_COM_ERROR)){
                cerr << "Error BatteryLevel: " << strerror(-err) << endl << flush;
                //throw std::runtime_error{"Error in BatteryLevel"};
            }
            if (err = mReceived->CompareID(MESSAGE_ANSWER_ROBOT_TIMEOUT)){
                cerr << "Timeout BatteryLevel: " << strerror(-err) << endl << flush;
                //throw std::runtime_error{"Error in BatteryLevel"};
            }
            cout << "Message reçu : " << mReceived->ToString() << endl << flush;
            
            WriteInQueue(&q_messageToMon,mReceived);
            //monitor.Write(mReceived);
            //delete(mSent);
            //delete(mReceived);
        }       
    }
}


/**
 * @brief Thread starting over server when there is an issue.
 */
void Tasks::RestartServerTask(void *arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    int err;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);    
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    while(1){
        rt_sem_p(&sem_restartServer, TM_INFINITE);
        cout << "[RestartServerTask][!]SERVER RESTARTING !!!!!!!" << endl << flush;
        //Arret de la camera
        rt_sem_v(&sem_stopCamera);
        
        //sleep(1);
        //Fermeture de la communication avec le robot
        rt_sem_v(&sem_closeComRobot);
        //sleep(1);
        //Fermeture du moniteur
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        //sleep(1);
        //monitor.Write(); 
        monitor.Close();
        //sleep(1);
        rt_mutex_release(&mutex_monitor);
        rt_task_delete(&th_sendToMon);
        rt_task_delete(&th_receiveFromMon);
        rt_task_delete(&th_move);
        rt_task_delete(&th_watchdog);
        rt_task_delete(&th_battery);
        rt_task_delete(&th_server);
        rt_task_delete(&th_openComRobot);
        rt_task_delete(&th_startRobot);
        rt_task_join(&th_closeComRobot);
       
        sleep(1);
        //rt_task_join(&th_stopCamera);
        
        cout << "[RestartServerTask]Monitor closed" << endl << flush;
        //robotStarted = 0;
        move = MESSAGE_ROBOT_STOP;
        WD =-1;
        position=0;
        arenaOK=-1;
        cout << "[RestartServerTask]Server restarts" << endl << flush;
        taskinit();
        Run();
        sleep(1);
        /*
        rt_sem_v(&sem_barrier);
        rt_sem_v(&sem_barrier);
        rt_sem_v(&sem_barrier);
        rt_sem_v(&sem_barrier);
        rt_sem_v(&sem_barrier);
        rt_sem_p(&sem_serverOk,TM_INFINITE);*/
        cout << "Tasks synchronized" << endl << flush;
        rt_sem_broadcast(&sem_barrier);
        cout << "[RestartServerTask]Tasks Relaunched going to sleep" << endl << flush;
    }
}
        

/**
 * @brief Thread closing communication wiht the robot.
 */
void Tasks::CloseComRobotTask(void * arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);  
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    //Message * toSend = new Message(MESSAGE_ROBOT_COM_CLOSE);
            
    rt_sem_p(&sem_closeComRobot, TM_INFINITE);
    //Arret du robot
    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
    robotStarted = 0;
    rt_mutex_release(&mutex_robotStarted);
    //Envoi d'un message au moniteur
    rt_mutex_acquire(&mutex_robot, TM_INFINITE); //mutex du robot?
    //WriteInQueue(&q_messageToMon, toSend); // msgSend will be deleted by sendToMon
    robot.Close();
    rt_mutex_release(&mutex_robot);

    //????close_communication_robot;
    cout << "ComRobot closed" << endl << flush;
}

/**
 * @brief Thread handling server communication with the monitor.
 */
void Tasks::ServerTask(void *arg) {
    int status;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are started)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    /**************************************************************************************/
    /* The task server starts here                                                        */
    /**************************************************************************************/
    rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
    status = monitor.Open(SERVER_PORT);
    rt_mutex_release(&mutex_monitor);

    cout << "Open server on port " << (SERVER_PORT) << " (" << status << ")" << endl;

    if (status < 0) throw std::runtime_error {
        "Unable to start server on port " + std::to_string(SERVER_PORT)
    };
    monitor.AcceptClient(); // Wait the monitor client
    cout << "Rock'n'Roll baby, client accepted!" << endl << flush;
    //pause();
    rt_sem_broadcast(&sem_serverOk);
}

/**
 * @brief Thread sending data to monitor.
 */
void Tasks::SendToMonTask(void* arg) {
    Message *msg;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    /**************************************************************************************/
    /* The task sendToMon starts here                                                     */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);

    while (1) {
        cout << "wait msg to send" << endl << flush;
        msg = ReadInQueue(&q_messageToMon);
        cout << "Send msg to mon: " << msg->ToString() << endl << flush;
        rt_mutex_acquire(&mutex_monitor, TM_INFINITE);
        monitor.Write(msg); // The message is deleted with the Write
        rt_mutex_release(&mutex_monitor);
    }
}

/**
 * @brief Thread receiving data from monitor.
 */
void Tasks::ReceiveFromMonTask(void *arg) {
    Message *msgRcv;
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    /**************************************************************************************/
    /* The task receiveFromMon starts here                                                */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    cout << "Received message from monitor activated" << endl << flush;

    while (1) {
        msgRcv = monitor.Read();
        cout << "Rcv <= " << msgRcv->ToString() << endl << flush;

        if (msgRcv->CompareID(MESSAGE_MONITOR_LOST)) {
            delete(msgRcv);
            rt_sem_v(&sem_restartServer);
            return;
        } else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_OPEN)) {
            rt_sem_v(&sem_openComRobot);
        } else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITHOUT_WD)) {
            WD =0;
            rt_sem_v(&sem_startRobot);
        } else if(msgRcv->CompareID(MESSAGE_ROBOT_START_WITH_WD)){
            WD =1;
            rt_sem_v(&sem_startRobot);
        } else if (msgRcv->CompareID(MESSAGE_ROBOT_GO_FORWARD) ||
                msgRcv->CompareID(MESSAGE_ROBOT_GO_BACKWARD) ||
                msgRcv->CompareID(MESSAGE_ROBOT_GO_LEFT) ||
                msgRcv->CompareID(MESSAGE_ROBOT_GO_RIGHT) ||
                msgRcv->CompareID(MESSAGE_ROBOT_STOP)) {

            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            move = msgRcv->GetID();
            rt_mutex_release(&mutex_move);
        } else if(msgRcv->CompareID(MESSAGE_CAM_OPEN)){
            rt_sem_v(&sem_startCamera);
        } else if(msgRcv->CompareID(MESSAGE_CAM_ASK_ARENA)){
            rt_sem_v(&sem_searchArena);
        }else if(msgRcv->CompareID(MESSAGE_CAM_ARENA_CONFIRM)){
            arenaOK=1;
            //lance la sauvegarde de l'arene confirmée
        }else if(msgRcv->CompareID(MESSAGE_CAM_ARENA_INFIRM)){
            arenaOK=0;
            // oublie l'arene infirmée
        }else if(msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_STOP)){
            rt_sem_v(&sem_stopCamera);
        }else if(msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_START)){
            position=1;
        }else if(msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_STOP)){
            position=0;
        }
        
        delete(msgRcv); // mus be deleted manually, no consumer
    }
}

/**
 * @brief Thread opening communication with the robot.
 */
void Tasks::OpenComRobot(void *arg) {
    int status;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    /**************************************************************************************/
    /* The task openComRobot starts here                                                  */
    /**************************************************************************************/
    while (1) {
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
        cout << "Open serial com (";
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        status = robot.Open();
        rt_mutex_release(&mutex_robot);
        cout << status;
        cout << ")" << endl << flush;

        Message * msgSend;
        if (status < 0) {
            msgSend = new Message(MESSAGE_ANSWER_NACK);
        } else {
            msgSend = new Message(MESSAGE_ANSWER_ACK);
        }
        WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon
    }
}

/**
 * @brief Thread starting the communication with the robot.
 */
void Tasks::StartRobotTask(void *arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    /**************************************************************************************/
    /* The task startRobot starts here                                                    */
    /**************************************************************************************/
    while (1) {
        Message * msgSend;
        rt_sem_p(&sem_startRobot, TM_INFINITE);
        
        rt_mutex_acquire(&mutex_robot, TM_INFINITE);
        if(WD==1){
            msgSend = robot.Write(robot.StartWithWD());
            cout << "Start robot with watchdog (";
            rt_mutex_release(&mutex_robot);
            rt_sem_v(&sem_watchdog);
        }
        else{
            msgSend = robot.Write(robot.StartWithoutWD());
            cout << "Start robot without watchdog (";
            rt_mutex_release(&mutex_robot);
        }
        cout << msgSend->GetID();
        cout << ")" << endl;

        cout << "Movement answer: " << msgSend->ToString() << endl << flush;
        WriteInQueue(&q_messageToMon, msgSend);  // msgSend will be deleted by sendToMon

        if (msgSend->GetID() == MESSAGE_ANSWER_ACK) {
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
        }
    }
}

/**
 * @brief Thread handling control of the robot.
 */
void Tasks::MoveTask(void *arg) {
    int rs;
    int cpMove;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 100000000);

    while (1) {
        rt_task_wait_period(NULL);
        //cout << "Periodic movement update";
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            cpMove = move;
            rt_mutex_release(&mutex_move);
            
            cout << " move: " << cpMove;
            
            rt_mutex_acquire(&mutex_robot, TM_INFINITE);
            robot.Write(new Message((MessageID)cpMove));
            rt_mutex_release(&mutex_robot);
        }
        cout << endl << flush;
    }
}


void Tasks::WatchDog(void *arg){
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    cout << "Launch " << __PRETTY_FUNCTION__ << endl << flush;
    /**************************************************************************************/
    /* The task WatchDog starts here                                                      */
    /**************************************************************************************/
    rt_sem_p(&sem_watchdog,TM_INFINITE);
    int mod =0;
    int err =0;
    if(WD==1){
        rt_task_set_periodic(NULL, TM_NOW, 200000000);
        while(1){
            mod=(mod+1)%5;
            Message * msgSend;
            rt_task_wait_period(NULL);
            msgSend = robot.Write(robot.Ping());
            if(msgSend->CompareID(MESSAGE_ANSWER_COM_ERROR)|| msgSend->CompareID(MESSAGE_ANSWER_ROBOT_TIMEOUT)){
                err=(err+1)%4;
                cout << "[watchdog] err = " << err << endl << flush;
            }
            else{
                err=(err-1);
                if(err == -1){
                    err =0;
                }
                cout << "[watchdog] err = " << err << endl << flush;
            }
            if(err==3){
                cout << "watchdog restart the server" << endl << flush;
                rt_sem_v(&sem_restartServer);
                return;
            }
            if(mod==0){
                robot.Write(robot.ReloadWD());
            }
        }
    }
}

/**
 * Write a message in a given queue
 * @param queue Queue identifier
 * @param msg Message to be stored
 */
void Tasks::WriteInQueue(RT_QUEUE *queue, Message *msg) {
    int err;
    if ((err = rt_queue_write(queue, (const void *) &msg, sizeof ((const void *) &msg), Q_NORMAL)) < 0) {
        cerr << "Write in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in write in queue"};
    }
}

/**
 * Read a message from a given queue, block if empty
 * @param queue Queue identifier
 * @return Message read
 */
Message *Tasks::ReadInQueue(RT_QUEUE *queue) {
    int err;
    Message *msg;

    if ((err = rt_queue_read(queue, &msg, sizeof ((void*) &msg), TM_INFINITE)) < 0) {
        cout << "Read in queue failed: " << strerror(-err) << endl << flush;
        throw std::runtime_error{"Error in read in queue"};
    }/** else {
        cout << "@msg :" << msg << endl << flush;
    } /**/

    return msg;
}

