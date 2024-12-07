#include <iostream>
#include "WriteOutput.h"
#include <vector>
#include <pthread.h>
#include <queue>
#include "monitor.h"
#include "helper.h"


using namespace std;

struct Car {
    int id;
    int travelTime;
    vector<pair<pair<char, int>, pair<int, int>>> path;
};

class NarrowBridge : public Monitor {
public:
    int id;
    int travelTime;
    int maxWaitTime;
    int currentDirection;


    pthread_mutex_t mutex;
    pthread_mutex_t sMutex;
    pthread_cond_t directionCondition;
    pthread_cond_t queueCondition;

    bool firstArrived;

    std::queue<int> carQueue0,carQueue1;

    NarrowBridge(int bridgeId, int travelTime, int maxWaitTime)
            : id(bridgeId), travelTime(travelTime), maxWaitTime(maxWaitTime) , currentDirection(0), firstArrived(true)
              {
                  pthread_mutex_init(&mutex, NULL);
                  pthread_mutex_init(&sMutex, NULL);
                  pthread_cond_init(&directionCondition, NULL);
                  pthread_cond_init(&queueCondition, NULL);
    }
    ~NarrowBridge() {
        pthread_mutex_destroy(&mutex);
        pthread_mutex_destroy(&sMutex);
        pthread_cond_destroy(&directionCondition);
        pthread_cond_destroy(&queueCondition);
    }

    void arrive(int direction,int carID){
        pthread_mutex_lock(&mutex);
        WriteOutput(carID, 'N', id, ARRIVE);
        std::queue<int>& queue = (direction == 0) ? carQueue0 : carQueue1;
        queue.push(carID);

        pthread_mutex_unlock(&mutex);
    }
    void pass(int direction, int carID) {
        pthread_mutex_lock(&mutex);
        std::queue<int>& queue = (direction == 0) ? carQueue0 : carQueue1;

        if (this->carQueue0.empty() && this->carQueue1.empty()){
            currentDirection = direction;
        }

        while (!(currentDirection == direction && !queue.empty() && queue.front() == carID)) {
            if (currentDirection != direction) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += maxWaitTime / 1000;
                ts.tv_nsec += (maxWaitTime % 1000) * 1000000;
                ts.tv_sec += ts.tv_nsec / 1000000000;
                ts.tv_nsec %= 1000000000;

                int res = pthread_cond_timedwait(&directionCondition, &mutex, &ts);
                if (res == ETIMEDOUT && currentDirection != direction) {

                    switchDirection();
                }
            } else {
                pthread_cond_wait(&queueCondition, &mutex);
            }
        }


        if (firstArrived){
            firstArrived= false;
        }
        else {
            pthread_mutex_unlock(&mutex);

            sleep_milli(PASS_DELAY);

            pthread_mutex_lock(&mutex);
        }

        WriteOutput(carID, 'N', id, START_PASSING);
        queue.pop();

        pthread_cond_broadcast(&queueCondition);

        pthread_mutex_unlock(&mutex);
        sleep_milli(travelTime);
        pthread_mutex_lock(&mutex);
        WriteOutput(carID, 'N', id, FINISH_PASSING);

        std::queue<int>& otherQueue = (direction == 0) ? carQueue1 : carQueue0;
        if (queue.empty() && currentDirection == direction && !otherQueue.empty() ){
            switchDirection();
        }
        pthread_mutex_unlock(&mutex);
    }
private:

    void switchDirection() {
        pthread_mutex_lock(&sMutex);
        firstArrived = true;
        currentDirection = 1 - currentDirection;
        pthread_cond_broadcast(&directionCondition);
        pthread_mutex_unlock(&sMutex);
    }
};

class Ferry {
public:
    int id;
    int travelTime;
    int maxWaitTime;
    int capacity;
    int carsLoaded;

    pthread_mutex_t mutex;
    pthread_cond_t conditionCanBoard;
    pthread_cond_t conditionCanDepart;

    bool isDeparting;

    Ferry(int ferryId, int tTime, int mWaitTime, int cap)
            : id(ferryId), travelTime(tTime), maxWaitTime(mWaitTime), capacity(cap), carsLoaded(0), isDeparting(false) {
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&conditionCanBoard, NULL);
        pthread_cond_init(&conditionCanDepart, NULL);
    }

    ~Ferry() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&conditionCanBoard);
        pthread_cond_destroy(&conditionCanDepart);
    }

    void boardFerry(int direction, int carID) {
        pthread_mutex_lock(&mutex);
        while (carsLoaded >= capacity || isDeparting) {
            pthread_cond_wait(&conditionCanBoard, &mutex);
        }

        carsLoaded++;

        if (carsLoaded == capacity) {
            isDeparting = true;
            pthread_cond_broadcast(&conditionCanDepart);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += maxWaitTime / 1000;
            ts.tv_nsec += (maxWaitTime % 1000) * 1000000;
            ts.tv_sec += ts.tv_nsec / 1000000000;
            ts.tv_nsec %= 1000000000;
            pthread_cond_timedwait(&conditionCanDepart, &mutex, &ts);
        }

        pthread_mutex_unlock(&mutex);
        WriteOutput(carID, 'F', id, START_PASSING);
        sleep_milli(travelTime);
        pthread_mutex_lock(&mutex);
        WriteOutput(carID, 'F', id, FINISH_PASSING);
        carsLoaded--;
        if (carsLoaded == 0) {
            isDeparting = false;
            pthread_cond_broadcast(&conditionCanBoard);
        }
        pthread_mutex_unlock(&mutex);
    }
};

class Crossroad {
private:
    std::vector<std::queue<int>> queues;

public:
    int id;
    int maxWaitTime;
    int currentDirection;
    pthread_mutex_t mutex;
    pthread_mutex_t smutex;

    pthread_cond_t directionCondition;
    pthread_cond_t queueCondition;
    int activeLane;
    int travelTime;
    bool firstArrived;

    Crossroad(int id, int travelTime, int maxWaitTime) : travelTime(travelTime), activeLane(-1),
                                                         maxWaitTime(maxWaitTime), id(id), firstArrived(
                    true),queues(4) {

        pthread_mutex_init(&mutex, NULL);
        pthread_mutex_init(&smutex, NULL);

        pthread_cond_init(&directionCondition, NULL);
        pthread_cond_init(&queueCondition, NULL);

    }

    ~Crossroad() {
        pthread_mutex_destroy(&mutex);
        pthread_mutex_destroy(&smutex);
        pthread_cond_destroy(&queueCondition);

        pthread_cond_destroy(&directionCondition);
    }

    void arrive(int direction, int carID) {
        pthread_mutex_lock(&mutex);
        WriteOutput(carID, 'C', id, ARRIVE);
        std::queue<int> &queue = queues[direction];
        queue.push(carID);

        pthread_mutex_unlock(&mutex);
    }

    void pass(int direction, int carID) {
        pthread_mutex_lock(&mutex);



        bool otherQueuesEmpty = true;
        for (int i = 0; i < queues.size(); ++i) {
            if (i != direction && !queues[i].empty()) {
                otherQueuesEmpty = false;
                break;
            }
        }

        if (otherQueuesEmpty) {
            currentDirection = direction;
        }
        std::queue<int> &queue = queues[direction];

        while (!(currentDirection == direction && !queue.empty() && queue.front() == carID)) {
            if (currentDirection != direction) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += maxWaitTime / 1000;
                ts.tv_nsec += (maxWaitTime % 1000) * 1000000;
                ts.tv_sec += ts.tv_nsec / 1000000000;
                ts.tv_nsec %= 1000000000;

                int res = pthread_cond_timedwait(&directionCondition, &mutex, &ts);
                if (res == ETIMEDOUT && currentDirection != direction) {
                    std::cout << "Timeout " << currentDirection << std::endl;

                    switchDirection();
                }
            } else {
                pthread_cond_wait(&queueCondition, &mutex);
            }
        }


        if (firstArrived) {
            firstArrived = false;
        } else {
            pthread_mutex_unlock(&mutex);

            sleep_milli(PASS_DELAY);

            pthread_mutex_lock(&mutex);
        }

        WriteOutput(carID, 'C', id, START_PASSING);
        queue.pop();

        pthread_cond_broadcast(&queueCondition);

        pthread_mutex_unlock(&mutex);
        sleep_milli(travelTime);
        pthread_mutex_lock(&mutex);
        WriteOutput(carID, 'C', id, FINISH_PASSING);

        if (queue.empty()) {
            bool shouldSwitch = false;
            for (int i = 0; i < queues.size(); ++i) {
                if (i != direction && !queues[i].empty()) {
                    shouldSwitch = true;
                    break;
                }
            }

            if (shouldSwitch) {
                switchDirection();
            }
        }
        pthread_mutex_unlock(&mutex);
    }

private:

    void switchDirection() {
        pthread_mutex_lock(&smutex);
        int newDirection = -1;
        for (int i = 1; i <= queues.size(); ++i) {
            int checkDirection = (currentDirection + i) % queues.size();
            if (!queues[checkDirection].empty()) {
                newDirection = checkDirection;
                break;
            }
        }

        if (newDirection != -1) {
            currentDirection = newDirection;
            std::cout << "Switched direction to: " << newDirection << std::endl;
            pthread_cond_broadcast(&directionCondition);
        }
        pthread_mutex_unlock(&smutex);
    }
};



vector<NarrowBridge> narrowBridges;
vector<Ferry> ferries;
vector<Crossroad> crossroads;



void *simulateCar(void *arg) {
    Car *car = static_cast<Car *>(arg);
    for (const auto &step: car->path) {
        switch (step.first.first) {
            case 'N':
                WriteOutput(car->id, 'N', step.first.second, TRAVEL);
                sleep_milli(car->travelTime);
                narrowBridges[step.first.second].arrive(step.second.first, car->id);
                narrowBridges[step.first.second].pass(step.second.first, car->id);
                break;
            case 'F':
                WriteOutput(car->id, 'F', step.first.second, TRAVEL);
                sleep_milli(car->travelTime);
                WriteOutput(car->id, 'F', step.first.second, ARRIVE);
                ferries[step.first.second].boardFerry(step.second.first,car->id);

                break;
            case 'C':
                WriteOutput(car->id, 'C', step.first.second, TRAVEL);
                sleep_milli(car->travelTime);
                crossroads[step.first.second].arrive(step.second.first, car->id);
                crossroads[step.first.second].pass(step.second.first, car->id);
                break;
        }
    }
    return nullptr;
}

int main() {

    int numNarrowBridges, numFerries, numCrossroads, numCars;


    cin >> numNarrowBridges;

    for (int i = 0; i < numNarrowBridges; ++i) {
        int travelTime, maxWaitTime;
        cin >> travelTime >> maxWaitTime;
        NarrowBridge narrowBridge(i, travelTime, maxWaitTime);
        narrowBridges.push_back(narrowBridge);
    }


    cin >> numFerries;

    for (int i = 0; i < numFerries; ++i) {
        int travelTime, maxWaitTime,capacity;
        cin >> travelTime >> maxWaitTime >> capacity;
        Ferry ferry(i,travelTime,maxWaitTime,capacity);
        ferries.push_back(ferry);
    }


    cin >> numCrossroads;

    for (int i = 0; i < numCrossroads; ++i) {
        int travelTime, maxWaitTime;
        cin >> travelTime >> maxWaitTime;
        Crossroad crossroad(i,travelTime, maxWaitTime);
        crossroads.push_back(crossroad);
    }


    cin >> numCars;
    vector<Car> cars(numCars);
    for (int i = 0; i < numCars; ++i) {
        cin >> cars[i].travelTime;
        int pathLength;
        cin >> pathLength;
        for (int j = 0; j < pathLength; ++j) {
            char connectorType;
            int connectorID;
            int fromDirection;
            int toDirection;
            cin >> connectorType >> connectorID;
            cin >> fromDirection;
            cin >> toDirection;
            pair<char, int> firstPair = make_pair(connectorType, connectorID);
            pair<int, int> secondPair = make_pair(fromDirection, toDirection);
            cars[i].path.emplace_back(firstPair, secondPair);
            cars[i].id = i;
        }
    }


    pthread_t carThreads[cars.size()];
    for (size_t i = 0; i < cars.size(); ++i) {
        InitWriteOutput();
        pthread_create(&carThreads[i], nullptr, simulateCar, &cars[i]);
    }

    for (size_t i = 0; i < cars.size(); ++i) {
        pthread_join(carThreads[i], nullptr);
    }

    return 0;
}