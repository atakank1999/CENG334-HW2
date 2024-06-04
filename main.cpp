#include <iostream>
#include "WriteOutput.h"
#include <vector>
#include <pthread.h>
#include <queue>
#include "monitor.h"
#include "helper.h"


using namespace std;

// Define a struct to represent a car
struct Car {
    int id;
    int travelTime;
    vector<pair<pair<char, int>, pair<int, int>>> path;
};

// Define structs to represent different types of connectors
class NarrowBridge : public Monitor {
public:
    NarrowBridge(int bridgeId, int tTime, int maxWait)
            : id(bridgeId), travelTime(tTime), maxWaitTime(maxWait), condition(this), current_direction(0),
              directionSwitchTime(0),carPassing(false) {
    }

    // Function to simulate passing through the narrow bridge
    void pass(int direction, int carID) {
        __synchronized__;

        if (direction == 0) {
            carQueue0.push(carID);
        } else {
            carQueue1.push(carID);
        }
        while (true) {
            if (direction == current_direction) {
                if (current_direction == 0 && carQueue0.front() == carID) {
                    sleep_milli(PASS_DELAY);
                    WriteOutput(carID, 'N', id, START_PASSING);
                    carQueue0.pop();
                    condition.notifyAll();
                    sleep_milli(travelTime);
                    condition.notifyAll();
                    WriteOutput(carID, 'N', id, FINISH_PASSING);
                    return;
                } else if (current_direction == 1 && carQueue1.front() == carID) {
                    sleep_milli(PASS_DELAY);
                    WriteOutput(carID, 'N', id, START_PASSING);
                    carQueue1.pop();
                    condition.notifyAll();
                    mutex.unlock();
                    sleep_milli(travelTime);
                    mutex.lock();
                    condition.notifyAll();
                    WriteOutput(carID, 'N', id, FINISH_PASSING);
                    return;
                }

            } else if (GetTimestamp() - directionSwitchTime >= maxWaitTime) {
                directionSwitchTime=GetTimestamp();
                switchDirection();
                condition.notifyAll();
            } else if (current_direction == 1 && carQueue1.empty()) {
                switchDirection();
                condition.notifyAll();
            } else if (current_direction == 0 && carQueue0.empty()) {
                switchDirection();
                condition.notifyAll();
            } else {
                condition.wait();
            }
        }
    }

private:
    int id;
    int travelTime;
    int maxWaitTime;
    bool carPassing;
    Condition condition; // Use Condition class for condition variable
    std::queue<int> carQueue0;
    std::queue<int> carQueue1;
    int current_direction;
    unsigned long long directionSwitchTime;
    void switchDirection() {
        current_direction = (current_direction + 1) % 2;
    }
};

struct Ferry {
    int id;
    int travelTime;
    int maxWaitTime;
    int capacity;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int numCarsLoaded;
};

struct Crossroad {
    int id;
    int travelTime;
    int maxWaitTime;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int activeLane;
};

// Define global variables for connectors
vector<NarrowBridge> narrowBridges;
vector<Ferry> ferries;
vector<Crossroad> crossroads;


// Function to simulate passing through a ferry
void passFerry(int carId, Ferry &ferry) {
    pthread_mutex_lock(&ferry.mutex);
    // Check if the ferry is full
    if (ferry.numCarsLoaded == ferry.capacity) {
        pthread_cond_wait(&ferry.condition, &ferry.mutex);
    }
    // Load the car onto the ferry
    ferry.numCarsLoaded++;
    pthread_mutex_unlock(&ferry.mutex);
    usleep(ferry.travelTime * 1000); // Simulate travel time
    pthread_mutex_lock(&ferry.mutex);
    ferry.numCarsLoaded--;
    pthread_cond_broadcast(&ferry.condition); // Notify waiting cars
    pthread_mutex_unlock(&ferry.mutex);
}

// Function to simulate passing through a crossroad
void passCrossroad(int carId, Crossroad &crossroad) {
    pthread_mutex_lock(&crossroad.mutex);
    // Check if the current lane is free
    if (crossroad.activeLane != -1) {
        pthread_cond_wait(&crossroad.condition, &crossroad.mutex);
    }
    // Simulate passing through the crossroad
    crossroad.activeLane = (carId % 4); // Assign lane based on car ID
    pthread_mutex_unlock(&crossroad.mutex);
    usleep(crossroad.travelTime * 1000); // Simulate travel time
    pthread_mutex_lock(&crossroad.mutex);
    crossroad.activeLane = -1; // Reset active lane
    pthread_cond_broadcast(&crossroad.condition); // Notify waiting cars
    pthread_mutex_unlock(&crossroad.mutex);
}

// Function for car thread to simulate its path
void *simulateCar(void *arg) {
    Car *car = static_cast<Car *>(arg);
    for (const auto &step: car->path) {
        switch (step.first.first) {
            case 'N':
                WriteOutput(car->id, 'N', step.first.second, TRAVEL);
                sleep_milli(car->travelTime);
                WriteOutput(car->id, 'N', step.first.second, ARRIVE);
                narrowBridges[step.first.second].pass(step.second.first, car->id);
                break;
            case 'F':
                passFerry(car->id, ferries[step.first.second]);
                break;
            case 'C':
                passCrossroad(car->id, crossroads[step.first.second]);
                break;
        }
    }
    return nullptr;
}

int main() {

    int numNarrowBridges, numFerries, numCrossroads, numCars;

    // Read number of narrow bridges
    cin >> numNarrowBridges;

    for (int i = 0; i < numNarrowBridges; ++i) {
        int travelTime, maxWaitTime;
        cin >> travelTime >> maxWaitTime;
        NarrowBridge narrowBridge(i, travelTime, maxWaitTime);
        narrowBridges.push_back(narrowBridge);
    }

    // Read number of ferries
    cin >> numFerries;

    for (int i = 0; i < numFerries; ++i) {
        cin >> ferries[i].travelTime >> ferries[i].maxWaitTime >> ferries[i].capacity;
    }

    // Read number of crossroads
    cin >> numCrossroads;

    for (int i = 0; i < numCrossroads; ++i) {
        cin >> crossroads[i].travelTime >> crossroads[i].maxWaitTime;
    }

    // Read number of cars
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


    // Initialize connectors and cars
    // Your code for initializing connectors and cars goes here

    // Create car threads
    pthread_t carThreads[cars.size()];
    for (size_t i = 0; i < cars.size(); ++i) {
        InitWriteOutput();
        pthread_create(&carThreads[i], nullptr, simulateCar, &cars[i]);
    }

    // Wait for all car threads to finish
    for (size_t i = 0; i < cars.size(); ++i) {
        pthread_join(carThreads[i], nullptr);
    }

    return 0;
}