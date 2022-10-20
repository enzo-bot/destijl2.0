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
#define PRIORITY_TSERVER 30
#define PRIORITY_TRECEIVEFROMMON 25
#define PRIORITY_TSENDTOMON 22
#define PRIORITY_TCAMERA 21
#define PRIORITY_TOPENCOMROBOT 20
#define PRIORITY_TMOVE 20
#define PRIORITY_TSTARTROBOT 20
#define PRIORITY_TBATTERY 20
#define PRIORITY_TWD_KEEP_ALIVE 23

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

#define FUNC_NAME cout << "Start of " << __PRETTY_FUNCTION__ << endl << flush
#define DEBUG cout << "Debug in line " << __LINE__ << endl << flush

void Tasks::Init() {
    int status;
    int err;

	this->camera = Camera(captureSize::sm, 25);

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
    if (err = rt_mutex_create(&mutex_watchdog, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_arena, NULL)) {
        cerr << "Error mutex create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_mutex_create(&mutex_position, NULL)) {
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
    if (err = rt_sem_create(&sem_battery, NULL, 0, S_FIFO)) {
        cerr << "Error semaphore create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Semaphores created successfully" << endl << flush;

    /**************************************************************************************/
    /* Tasks creation                                                                     */
    /**************************************************************************************/
    if (err = rt_task_create(&th_server, "th_server", 0, PRIORITY_TSERVER, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_sendToMon, "th_sendToMon", 0, PRIORITY_TSENDTOMON, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_receiveFromMon, "th_receiveFromMon", 0, PRIORITY_TRECEIVEFROMMON, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_openComRobot, "th_openComRobot", 0, PRIORITY_TOPENCOMROBOT, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_startRobot, "th_startRobot", 0, PRIORITY_TSTARTROBOT, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_move, "th_move", 0, PRIORITY_TMOVE, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_battery, "th_battery", 0, PRIORITY_TBATTERY, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_WDKeepAlive, "th_WDKeepAlive", 0, PRIORITY_TWD_KEEP_ALIVE, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    if (err = rt_task_create(&th_camera, "th_camera", 0, PRIORITY_TCAMERA, 0)) {
        cerr << "Error task create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Tasks created successfully" << endl << flush;

    /**************************************************************************************/
    /* Message queues creation                                                            */
    /**************************************************************************************/
    if ((err = rt_queue_create(&q_messageToMon, "q_messageToMon", sizeof (Message*)*50, Q_UNLIMITED, Q_FIFO)) < 0) {
        cerr << "Error msg queue create: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
    cout << "Queues created successfully" << endl << flush;

}

/**
 * @brief Démarrage des tâches
 */
void Tasks::Run() {
    rt_task_set_priority(NULL, T_LOPRIO);
    int err;
	this->startTask(&th_server, &Tasks::ServerTask);
	this->startTask(&th_sendToMon, &Tasks::SendToMonTask);
	this->startTask(&th_sendToMon,& Tasks::SendToMonTask);
	this->startTask(&th_receiveFromMon,& Tasks::ReceiveFromMonTask);
	this->startTask(&th_openComRobot,& Tasks::OpenComRobot);
	this->startTask(&th_startRobot,& Tasks::StartRobotTask);
	this->startTask(&th_move,& Tasks::MoveTask);
	this->startTask(&th_battery,& Tasks::BatteryTask);
	this->startTask(&th_WDKeepAlive,& Tasks::WDKeepAliveTask);
	this->startTask(&th_camera,& Tasks::CameraTask);

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
void Tasks::Join() {
    cout << "Tasks synchronized" << endl << flush;
    rt_sem_broadcast(&sem_barrier);
    pause();
}

/**
 * @brief Thread handling server communication with the monitor.
 */
void Tasks::ServerTask(void *arg) {
    int status;
	int err;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are started)
    rt_sem_p(&sem_barrier, TM_INFINITE);
	if (err = rt_task_suspend(&th_camera)) {
		cerr << "Error suspending CameraTask: " << strerror(-err) << endl << flush;
	}

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

    /**************************************************************************************/
    /* The task sendToMon starts here                                                     */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);

    while (1) {
		FUNC_NAME;
        msg = ReadInQueue(&q_messageToMon);
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
	int err;
	Message* msgSend;
    
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task receiveFromMon starts here                                                */
    /**************************************************************************************/
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    cout << "Received message from monitor activated" << endl << flush;

	while (1) {
		FUNC_NAME;
		msgRcv = monitor.Read();
		cout << "Rcv <= " << msgRcv->ToString() << endl << flush;

		if (msgRcv->CompareID(MESSAGE_MONITOR_LOST)) {
		    //this->SendToRobot(ComRobot::Stop());
		    this->SendToRobot(ComRobot::PowerOff());
				this->camera.Close();
		    delete(msgRcv);
		    exit(-1);
		} else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_OPEN)) {
		    rt_sem_v(&sem_openComRobot);
			} else if (msgRcv->CompareID(MESSAGE_CAM_OPEN)) {
				if(this->camera.Open()) {
					msgSend = new Message(MESSAGE_ANSWER_ACK);
				} else {
					msgSend = new Message(MESSAGE_ANSWER_NACK);
				}
				if (err = rt_task_resume(&th_camera)) {
					cerr << "Error resuming Camera: " << strerror(-err) << endl << flush;
				}
				WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon
			} else if (msgRcv->CompareID(MESSAGE_CAM_CLOSE)) {
				rt_mutex_acquire(&mutex_camera, TM_INFINITE);
				if (err = rt_task_suspend(&th_camera)) {
					cerr << "Error suspending camera: " << strerror(-err) << endl << flush;
				}
				rt_mutex_release(&mutex_camera);
				this->camera.Close();
				if (this->camera.IsOpen()) {
					msgSend = new Message(MESSAGE_ANSWER_NACK);
				} else {
					msgSend = new Message(MESSAGE_ANSWER_ACK);
				}
				WriteInQueue(&q_messageToMon, msgSend); // msgSend will be deleted by sendToMon
		} else if (msgRcv->CompareID(MESSAGE_ROBOT_COM_CLOSE)) {
				this->SendToRobot(ComRobot::Reset());
				rt_mutex_acquire(&mutex_watchdog, TM_INFINITE);
				if (watchDog) {
					if (err = rt_task_suspend(&th_WDKeepAlive)) {
						cerr << "Error suspending WDKeepAlive: " << strerror(-err) << endl << flush;
					}
				}
				rt_mutex_release(&mutex_watchdog);
				rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
				robotStarted = 0;
				rt_mutex_release(&mutex_robotStarted);
		} else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITHOUT_WD)) {
				watchDog = 0;
		    rt_sem_v(&sem_startRobot);
		} else if (msgRcv->CompareID(MESSAGE_ROBOT_START_WITH_WD)) {
				watchDog = 1;
		    rt_sem_v(&sem_startRobot);
		} else if (msgRcv->CompareID(MESSAGE_ROBOT_GO_FORWARD) ||
			msgRcv->CompareID(MESSAGE_ROBOT_GO_BACKWARD) ||
			msgRcv->CompareID(MESSAGE_ROBOT_GO_LEFT) ||
			msgRcv->CompareID(MESSAGE_ROBOT_GO_RIGHT) ||
			msgRcv->CompareID(MESSAGE_ROBOT_STOP)) {

		    rt_mutex_acquire(&mutex_move, TM_INFINITE);
		    move = msgRcv->GetID();
		    rt_mutex_release(&mutex_move);
		} else if (msgRcv->CompareID(MESSAGE_ROBOT_BATTERY_GET)) {
			rt_sem_v(&sem_battery);
		} else if (msgRcv->CompareID(MESSAGE_CAM_ASK_ARENA)) {
			rt_mutex_acquire(&mutex_arena, TM_INFINITE);
			this->arena_search = true;
			this->arena_responded = false;
			if (this->arena != nullptr) {
				delete this->arena;
				this->arena = nullptr;
			}
			rt_mutex_release(&mutex_arena);
		} else if (msgRcv->CompareID(MESSAGE_CAM_ARENA_CONFIRM)) {
			rt_mutex_acquire(&mutex_arena, TM_INFINITE);
			this->arena_responded = true;
			rt_mutex_release(&mutex_arena);
		} else if (msgRcv->CompareID(MESSAGE_CAM_ARENA_INFIRM)) {
			rt_mutex_acquire(&mutex_arena, TM_INFINITE);
			this->arena_responded = true;
			if (this->arena != nullptr) {
				delete this->arena;
				this->arena = nullptr;
			}
			rt_mutex_release(&mutex_arena);
		} else if (msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_START)) {
			rt_mutex_acquire(&mutex_position, TM_INFINITE);
			this->compute_position = true;
			rt_mutex_release(&mutex_position);
		} else if (msgRcv->CompareID(MESSAGE_CAM_POSITION_COMPUTE_STOP)) {
			rt_mutex_acquire(&mutex_position, TM_INFINITE);
			this->compute_position = false;
			rt_mutex_release(&mutex_position);
		}
    
		delete(msgRcv); // mus be deleted manually, no consumer
	}
}

/**
 * @brief Thread opening communication with the robot.
 */
void Tasks::OpenComRobot(void *arg) {
    int status;
    int err;

    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    rt_sem_p(&sem_barrier, TM_INFINITE);
    
    /**************************************************************************************/
    /* The task openComRobot starts here                                                  */
    /**************************************************************************************/
    while (1) {
		FUNC_NAME;
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
	int err;
	if (err = rt_task_suspend(&th_WDKeepAlive)) {
        cerr << "Error suspending WDKeepAlive: " << strerror(-err) << endl << flush;
	}
	
    /**************************************************************************************/
    /* The task startRobot starts here                                                    */
    /**************************************************************************************/
    while (1) {

		FUNC_NAME;
        Message * msgSend;
        rt_sem_p(&sem_startRobot, TM_INFINITE);

		if (watchDog) {
			msgSend = robot.Write(ComRobot::StartWithWD());
			//msgSend = SendToRobot(ComRobot::StartWithWD());
			cout << "Start robot with watchdog (";
			if (err = rt_task_resume(&th_WDKeepAlive)) {
				cerr << "Error resuming WDKeepAlive: " << strerror(-err) << endl << flush;
			}
		} else {
			msgSend = robot.Write(ComRobot::StartWithoutWD());
			//msgSend = SendToRobot(ComRobot::StartWithoutWD());
			cout << "Start robot without watchdog (";
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
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /**************************************************************************************/
    rt_task_set_periodic(NULL, TM_NOW, 100000000);

    while (1) {
		// FUNC_NAME;
        rt_task_wait_period(NULL);
        //cout << "Periodic movement update";
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        rs = robotStarted; 
        rt_mutex_release(&mutex_robotStarted);
        if (rs == 1) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            cpMove = move;
            rt_mutex_release(&mutex_move);
            
            //cout << " move: " << cpMove << endl << flush;
            
            SendToRobot(new Message((MessageID)cpMove));
        } else {
            //cout << endl << flush;
		}
    } 
}

void Tasks::BatteryTask(void *arg) {
    int rs;
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
    
    /**************************************************************************************/
    /* The task starts here                                                               */
    /*************************************** ***********************************************/
    rt_sem_p(&sem_barrier, TM_INFINITE);
    rt_task_set_periodic(NULL, TM_NOW, 500000000);
	rt_sem_p(&sem_battery, TM_INFINITE);

    while (1) {
        rt_task_wait_period(NULL);
		FUNC_NAME;
		Message* batLvl = this->SendToRobot(ComRobot::GetBattery());
		if (batLvl == nullptr) {
			cout << "Communication with Robot lost" << endl << flush;
		} else {
			this->WriteInQueue(&q_messageToMon, batLvl);
		}
    }
}

void Tasks::WDKeepAliveTask(void* arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
	rt_task_set_periodic(NULL, TM_NOW, 1000000000); // every 1 sec
    rt_sem_p(&sem_barrier, TM_INFINITE);

	while(1) {
		rt_task_wait_period(NULL);
		FUNC_NAME;
		this->SendToRobot(ComRobot::ReloadWD());
	}
}

void Tasks::CameraTask(void* arg) {
    cout << "Start " << __PRETTY_FUNCTION__ << endl << flush;
    // Synchronization barrier (waiting that all tasks are starting)
	rt_task_set_periodic(NULL, TM_NOW, 100000000); // every 100 ms
    rt_sem_p(&sem_barrier, TM_INFINITE);
	Img* picture;

	while(1) {
		rt_task_wait_period(NULL);
		FUNC_NAME;
		if (this->camera.IsOpen()) {
			cout << "Camera is open" << endl << flush;
			rt_mutex_acquire(&mutex_arena, TM_INFINITE);
			if (this->arena_search) {
				cout << "We have to search the arena" << endl << flush;
				rt_mutex_acquire(&mutex_camera,TM_INFINITE);
				picture = new Img(this->camera.Grab());
				rt_mutex_release(&mutex_camera);
				this->arena = new Arena();
				*this->arena = picture->SearchArena();
				if (this->arena->IsEmpty()) {
					cout << "Arena not found" << endl << flush;
					WriteInQueue(&q_messageToMon, new Message(MESSAGE_ANSWER_NACK)); // msgSend will be deleted by sendToMon
					delete this->arena;
				} else {
					cout << "Arena found" << endl << flush;
					picture->DrawArena(*(this->arena));
					this->arena_search = false;
					WriteInQueue(&q_messageToMon, new MessageImg(MESSAGE_CAM_IMAGE, picture)); // msgSend will be deleted by sendToMon
				}
			} else if (this->arena_responded) {
				cout << "User sent an answer" << endl << flush;
				rt_mutex_acquire(&mutex_camera,TM_INFINITE);
				picture = new Img(this->camera.Grab());
				rt_mutex_release(&mutex_camera);
				if (this->arena != nullptr) {
					cout << "We still have an Arena object" << endl << flush;
					picture->DrawArena(*(this->arena));
					rt_mutex_acquire(&mutex_position, TM_INFINITE);
					if (this->compute_position) {
						std::list<Position> positions = picture->SearchRobot(*(this->arena));
						if (positions.empty()) {
							Position pos;
							pos.center = cv::Point2f(-1.0, -1.0);
							WriteInQueue(&q_messageToMon, new MessagePosition(MESSAGE_CAM_POSITION, pos)); // msgSend will be deleted by sendToMon
						} else {
							picture->DrawAllRobots(positions);
							WriteInQueue(&q_messageToMon, new MessagePosition(MESSAGE_CAM_POSITION, positions.front())); // msgSend will be deleted by sendToMon

						}
					}
					rt_mutex_release(&mutex_position);
				}
				WriteInQueue(&q_messageToMon, new MessageImg(MESSAGE_CAM_IMAGE, picture)); // msgSend will be deleted by sendToMon
			}
			rt_mutex_release(&mutex_arena);
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
    } **/

    return msg;
}


Message* Tasks::SendToRobot(Message* msg) {
	//FUNC_NAME;
	int rs;
	int counter = 0;
	bool succeed = false;
	Message* ret = nullptr;
	rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
	rs = robotStarted;
	rt_mutex_release(&mutex_robotStarted);
	if (rs) {
		while (counter < 3 && !succeed) {
		   rt_mutex_acquire(&mutex_robot, TM_INFINITE);
		   ret = robot.Write(msg);
		   rt_mutex_release(&mutex_robot);
		   if (ret->CompareID(MESSAGE_ANSWER_ROBOT_TIMEOUT)) {
			   cout << msg->ToString() << " was lost" << endl << flush;
			   counter++;
		   } else {
			   succeed = true;
		   }
		}
		if (!succeed) {
		   cout << "Lost communication with robot, shutting down..." << endl << flush;
			robot.Close();
			rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
			robotStarted = 0;
			rt_mutex_release(&mutex_robotStarted);
			this->WriteInQueue(&q_messageToMon, new Message(MESSAGE_ANSWER_ROBOT_TIMEOUT));
		}
	}
	if (ret == nullptr) {
		ret = new Message(MESSAGE_ANSWER_ROBOT_TIMEOUT);
	}
	return ret;

}

void Tasks::startTask(RT_TASK* task, void(Tasks::*method)(void*)) {
    int err;

    if (err = rt_task_start(task, method, this)) {
        cerr << "Error task start: " << strerror(-err) << endl << flush;
        exit(EXIT_FAILURE);
    }
}
